/* Routeur solaire développé par le Profes'Solaire v5.20 - 12-04-2023 - professolaire@gmail.com
- 2 sorties 16A / 3000 watts
- 1 relais
- 1 serveur web Dash Lite avec On / Off
 * ESP32 + JSY-MK-194 (16 et 17) + Dimmer1 24A-600V (35 ZC et 25 PW) + Dimmer 2 24A-600V ( 35 ZC et 26 PW) + écran Oled (22 : SCK et 21 SDA) + relay (13)
 Utilisation des 2 Cores de l'Esp32
*/
// Librairies //
#include "config.h"  // Local file to store your config, like  Wifi Password
#include <HardwareSerial.h>
#include <RBDdimmer.h>  // Download from https://github.com/RobotDynOfficial/RBDDimmer
#include <U8g2lib.h>    // Install "U8g2" from library manager
#include <Wire.h>
#include <WiFi.h>
#include <ESPDash.h>            // Install "ESP-DASH" from library manager
#include <AsyncTCP.h>           // Install "AsyncTCP" from library manager
#include <ESPAsyncWebServer.h>  // Download from https://github.com/me-no-dev/ESPAsyncWebServer
// Add also "ArduinoJson"  from library manager
#include <ArduinoHA.h>  // Download it from https://github.com/dawidchyrzynski/arduino-home-assistant/releases/tag/2.0.0
// And add PubSubClient (by Nick O'Leary) from library manager

#define RXD2 16
#define TXD2 17
// Size of a MAC-address or BSSID
#define WL_MAC_ADDR_LENGTH 6

byte ByteArray[250];
int ByteData[20];

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);


//déclaration des variables//

float routagePuissance = -20; /* puissance d'injection réseau en watts : pour plus de précision, ajuster suivant votre charge branchée "exemple pour 3000w => -30 / pour 1000w => -10 / pour 500w => -5 */
int ajustePuissance = 0;      /* réglage puissance */
float puissanceRoutage = 0;
float mini_puissance = 0;
float max_puissance = 40;  // Set to the maximum power of your device. 2 000 for a boiler. 40 for a light bulb.
float pas_dimmer1 = 1;
float pas_dimmer2 = 0.1;
float pas_dimmer3 = 5;
float valDim1 = 0;
float valDim2 = 0;
float maxDimmer1 = 95;
float minDimmer1 = 0;
float Voltage, Intensite1, Energy1, Frequency, PowerFactor1, Intensite2, Energy2, Sens1, Sens2;
int Power1;
int Power2;
boolean Auto = 1;
int Value;
float relayOn = 1000;
float relayOff = 800;


///  configuration wifi ///

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
AsyncWebServer server(80);
ESPDash dashboard(&server);

Card button(&dashboard, BUTTON_CARD, "Auto");
Card slider(&dashboard, SLIDER_CARD, "Puissance sortie 1 en %", "", 0, 95);
Card voltage(&dashboard, GENERIC_CARD, "Voltage", "V");
Card power2(&dashboard, GENERIC_CARD, "Power2", "W");
Card frequency(&dashboard, GENERIC_CARD, "frequency", "Hz");
Card consommationsurplus(&dashboard, GENERIC_CARD, "surplus / consommation", "watts");
Card puissance(&dashboard, GENERIC_CARD, "puissance envoyée au ballon", "watts");
Card intensite1(&dashboard, GENERIC_CARD, "Intensité 1", "A");
Card intensite2(&dashboard, GENERIC_CARD, "Intensité 2", "A");
Card energy1(&dashboard, GENERIC_CARD, "énergie sauvée", "kwh");
Card energy2(&dashboard, GENERIC_CARD, "énergie 2", "kwh");
Card valdim1(&dashboard, PROGRESS_CARD, "sortie 1", "%", 0, 95);
Card valdim2(&dashboard, PROGRESS_CARD, "sortie 2", "%", 0, 95);

////////////// Fin connexion wifi //////////


/* Broches utilisées */
const int zeroCrossPin = 35; /* broche utilisée pour le zéro crossing */
const int pulsePin1 = 25;    /* broche impulsions routage 1*/
const int pulsePin2 = 26;    /* broche impulsions routage 2*/
const int Relay1 = 13;


dimmerLamp dimmer1(pulsePin1, zeroCrossPin);
dimmerLamp dimmer2(pulsePin2, zeroCrossPin);

///////////// MQTT with Home Assistant /////////////
WiFiClient client;
// Unique ID must be set for MQTT
HADevice device(MQTT_TOPIC_NAME);
HAMqtt mqtt(client, device);
//// Setting MQTT Devices
// Voltage = ByteData[1] * 0.0001;      // Tension
HASensorNumber ha_voltage("voltage", HASensorNumber::PrecisionP1);
// Intensite1 = ByteData[2] * 0.0001;   // Intensité 1
HASensorNumber ha_current1("current1", HASensorNumber::PrecisionP1);
// Power 1 in %
HASensorNumber ha_power1("power1", HASensorNumber::PrecisionP1);
// Energy1 = ByteData[4] * 0.0001;      // Energie 1
HASensorNumber ha_energy1("energy1", HASensorNumber::PrecisionP1);
// ???
HASensorNumber ha_production("ha_production", HASensorNumber::PrecisionP1);
// Frequency = ByteData[8] * 0.01;      // Fréquence
HASensorNumber ha_frequency("frequency", HASensorNumber::PrecisionP1);
// Intensite2 = ByteData[10] * 0.0001;  // Intensité 2
HASensorNumber ha_current2("current2", HASensorNumber::PrecisionP1);
// Power2
HASensorNumber ha_power2("power2", HASensorNumber::PrecisionP1);
//
HASensorNumber ha_energy2("energy2", HASensorNumber::PrecisionP1);
// Energy saved
HASensorNumber ha_energy_saved("energy_saved", HASensorNumber::PrecisionP1);
// Dimmer 1 output in %
HASensorNumber ha_dimmer1_output("dimmer1", HASensorNumber::PrecisionP1);
HASensorNumber ha_dimmer2_output("dimmer2", HASensorNumber::PrecisionP1);
// Mode Auto ON/OFF
HASensorNumber ha_mode("mode_auto", HASensorNumber::PrecisionP0);


unsigned long lastTempPublishAt = 0;
float lastTemp = 0;

// Multi-core
TaskHandle_t Task1;
TaskHandle_t Task2;

/////////////////// S E T U P /////////////////////////////
void setup() {
  Serial.begin(115200);
  Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2);  //PORT DE CONNEXION AVEC LE CAPTEUR JSY-MK-194
  delay(300);
  u8g2.begin();            // ECRAN OLED
  u8g2.enableUTF8Print();  //nécessaire pour écrire des caractères accentués
  dimmer1.begin(NORMAL_MODE, ON);
  pinMode(Relay1, OUTPUT);
  digitalWrite(Relay1, LOW);
  // WiFi connection
  WiFi.mode(WIFI_STA);  //Optional
  WiFi.begin(ssid, password);
  server.begin();
  delay(100);
  // set device's details (optional)
  device.setName("Solar PV router");
  device.setSoftwareVersion("1.0.0");
  device.setManufacturer("Le Profes'solaire");
  device.setModel("ABC-123");
  device.enableSharedAvailability();
  device.enableLastWill();

  /* MQTT Devices */
  // TODO: customize all Home Assistant integration
  ha_power1.setIcon("mdi:home");
  ha_power1.setName("Power1");
  ha_power1.setUnitOfMeasurement("W");

  mqtt.begin(MQTT_BROKER_IP, MQTT_USER, MQTT_PASSWORD);

  //Code pour créer un Task Core 0//
  xTaskCreatePinnedToCore(
    Task1PowerMonitoring, /* Task function. */
    "Task1",              /* name of task. */
    10000,                /* Stack size of task */
    NULL,                 /* parameter of the task */
    1,                    /* priority of the task */
    &Task1,               /* Task handle to keep track of created task */
    0);                   /* pin task to core 0 */
  delay(500);

  //Code pour créer un Task Core 1//
  xTaskCreatePinnedToCore(
    Task2ExternalInterface, /* Task function. */
    "Task2",                /* name of task. */
    10000,                  /* Stack size of task */
    NULL,                   /* parameter of the task */
    1,                      /* priority of the task */
    &Task2,                 /* Task handle to keep track of created task */
    1);                     /* pin task to core 1 */
  delay(500);
  Serial.println("Setup done.");
}

//programme utilisant le Core 1 de l'ESP32//
void ReadPowerMeter() {
  delay(60);
  // Modbus RTU message to get the data
  byte msg[] = { 0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18 };
  int i;
  int len = 8;

  // Send message
  for (i = 0; i < len; i++) {
    Serial2.write(msg[i]);
  }

  ////////// Reception  des données Modbus RTU venant du capteur JSY-MK-194 ////////////////////////
  int a = 0;
  while (Serial2.available()) {
    ByteArray[a] = Serial2.read();
    a++;
  }

  //// Sanity Checks
  if (ByteArray[0] != 0x01) {
    Serial.print("ERROR Message received do not start with 0x01: ");
    Serial.println(a);
    for (i = 0; i < a; i++) {
      Serial.print(ByteArray[i], HEX);
      Serial.print("-");
    }
    Serial.println(".");
  }

  if (a != 61) {  // 61
    Serial.print("ERROR Message received have wrong length: ");
    Serial.println(a);
    for (i = 0; i < a; i++) {
      Serial.print(i);
      Serial.print("=");
      Serial.print(ByteArray[i], HEX);
      Serial.print(" ");
    }
    Serial.println(".");
  }
  //////// Extract response /////////////////
  ByteData[1] = ByteArray[3] * 16777216 + ByteArray[4] * 65536 + ByteArray[5] * 256 + ByteArray[6];       // Tension en Volts
  ByteData[2] = ByteArray[7] * 16777216 + ByteArray[8] * 65536 + ByteArray[9] * 256 + ByteArray[10];      // Intensité 1 en Ampères
  ByteData[3] = ByteArray[11] * 16777216 + ByteArray[12] * 65536 + ByteArray[13] * 256 + ByteArray[14];   // Puissance 1 en Watts
  ByteData[4] = ByteArray[15] * 16777216 + ByteArray[16] * 65536 + ByteArray[17] * 256 + ByteArray[18];   // Energie 1 en kwh
  ByteData[7] = ByteArray[27];                                                                            // sens 1 du courant
  ByteData[9] = ByteArray[28];                                                                            // sens 2 du courant
  ByteData[8] = ByteArray[31] * 16777216 + ByteArray[32] * 65536 + ByteArray[33] * 256 + ByteArray[34];   // Fréquence en hz
  ByteData[10] = ByteArray[39] * 16777216 + ByteArray[40] * 65536 + ByteArray[41] * 256 + ByteArray[42];  // Intensité 2 en Ampères
  ByteData[11] = ByteArray[43] * 16777216 + ByteArray[44] * 65536 + ByteArray[45] * 256 + ByteArray[46];  // Puissance 2 en Watts
  ByteData[12] = ByteArray[47] * 16777216 + ByteArray[48] * 65536 + ByteArray[49] * 256 + ByteArray[50];  // Energie 2 en kwh

  ///////// Normalisation des valeurs ///////////////
  Voltage = ByteData[1] * 0.0001;      // Tension
  Intensite1 = ByteData[2] * 0.0001;   // Intensité 1
  Power1 = ByteData[3] * 0.0001;       // Puissance 1
  Energy1 = ByteData[4] * 0.0001;      // Energie 1
  Sens1 = ByteData[7];                 // Sens 1
  Sens2 = ByteData[9];                 // Sens 2
  Frequency = ByteData[8] * 0.01;      // Fréquence
  Intensite2 = ByteData[10] * 0.0001;  // Intensité 2
  Power2 = ByteData[11] * 0.0001;      // Puissance 2
  Energy2 = ByteData[12] * 0.0001;     // Energie 2

  if (Sens2 == 1) {
    ajustePuissance = -Power2;
  } else {
    ajustePuissance = Power2;
  }

  // Inversion des mesures
  // float Intensite_tmp = Intensite1;
  // int Power_tmp = Power1;
  // int Energy_tmp = Energy1;

  // float Intensite1 = Intensite2;
  // int Power1 = Power2;
  // int Energy1 = Energy2;

  // Intensite2 = Intensite_tmp;
  // Power2 = Power_tmp;
  // Energy2 = Energy_tmp;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
}



void Task1PowerMonitoring(void* pvParameters) {
  for (;;) {

    button.attachCallback([&](bool value) {
      Auto = value;
      button.update(Auto);
      dashboard.sendUpdates();
    });

    slider.attachCallback([&](int value) {
      Value = value;
      slider.update(value);
      dashboard.sendUpdates();
    });


    if (Auto == 0) {
      ReadPowerMeter();
      valDim1 = Value;
      dimmer1.setPower(Value);
      delay(100);
      dimmer1.setState(ON);
    }

    if (Auto == 1) {
      ReadPowerMeter();
      // calcul triacs ///
      // réglages Dimmer 1 ///
      if (valDim1 < 1) {
        dimmer1.setState(OFF);
      }

      if (Power2 == 0 && Power1 == 0) {
        valDim1 = 0;
        dimmer1.setState(OFF);
        dimmer1.setPower(valDim1);
      }

      if (ajustePuissance <= -400 && valDim1 < 90) {
        valDim1 = valDim1 + pas_dimmer3;
        dimmer1.setPower(valDim1);
        delay(200);
        dimmer1.setState(ON);
      }
      if (ajustePuissance > -400 && ajustePuissance <= -150 && valDim1 < 94) {
        valDim1 = valDim1 + pas_dimmer1;
        dimmer1.setPower(valDim1);
        delay(200);
        dimmer1.setState(ON);
      }

      if (ajustePuissance <= routagePuissance && ajustePuissance > -150 && valDim1 < 94.99) {
        valDim1 = valDim1 + pas_dimmer2;
        dimmer1.setPower(valDim1);
        delay(200);
        dimmer1.setState(ON);
      }

      if (ajustePuissance > routagePuissance && ajustePuissance <= 30 && valDim1 > 0.01) {
        valDim1 = valDim1 - pas_dimmer2;
        dimmer1.setState(ON);
        delay(200);
        dimmer1.setPower(valDim1);
      }

      if (ajustePuissance > 30 && valDim1 > 1 && ajustePuissance <= 150) {
        valDim1 = valDim1 - pas_dimmer1;
        dimmer1.setState(ON);
        delay(200);
        dimmer1.setPower(valDim1);
      }

      if (ajustePuissance > 150 && valDim1 > 5) {
        valDim1 = valDim1 - pas_dimmer3;
        dimmer1.setState(ON);
        delay(200);
        dimmer1.setPower(valDim1);
      }

      // réglages Dimmer 2 ///

      if (valDim1 >= 90) {
        if (valDim2 < 1) {
          dimmer2.setState(OFF);
        }

        if (Power2 == 0 && Power1 == 0) {
          valDim2 = 0;
          dimmer2.setPower(valDim2);
          dimmer2.setState(OFF);
        }

        if (ajustePuissance <= -400 && valDim2 < 90) {
          valDim2 = valDim2 + pas_dimmer3;
          dimmer2.setState(ON);
          delay(200);
          dimmer2.setPower(valDim2);
        }
        if (ajustePuissance > -400 && ajustePuissance <= -150 && valDim2 < 94) {
          valDim2 = valDim2 + pas_dimmer1;
          dimmer2.setState(ON);
          delay(200);
          dimmer2.setPower(valDim2);
        }


        if (ajustePuissance <= routagePuissance && ajustePuissance > -150 && valDim2 < 94.99) {
          valDim2 = valDim2 + pas_dimmer2;
          dimmer2.setState(ON);
          delay(200);
          dimmer2.setPower(valDim2);
        }


        if (ajustePuissance > routagePuissance && ajustePuissance <= 30 && valDim2 > 0.01) {
          valDim2 = valDim2 - pas_dimmer2;
          dimmer2.setState(ON);
          delay(200);
          dimmer2.setPower(valDim2);
        }

        if (ajustePuissance > 30 && valDim2 > 1 && ajustePuissance <= 150) {
          valDim2 = valDim2 - pas_dimmer1;
          dimmer2.setState(ON);
          delay(200);
          dimmer2.setPower(valDim2);
        }

        if (ajustePuissance > 150 && valDim2 > 5) {
          valDim2 = valDim2 - pas_dimmer3;
          dimmer2.setState(ON);
          delay(200);
          dimmer2.setPower(valDim2);
        }
      }

      else {
        valDim2 = 0;
        dimmer2.setPower(valDim2);
        dimmer2.setState(OFF);
      }

      //////////////////// Set relay state ///////////////
      if (Power1 > relayOn) {
        digitalWrite(Relay1, HIGH);
      }
      if (Power1 < relayOff) {
        digitalWrite(Relay1, LOW);
      }
    }
  }
}




//programme utilisant le Core 2 de l'ESP32//

void Task2ExternalInterface(void* pvParameters) {

  for (;;) {


    // affichage page web //

    consommationsurplus.update(ajustePuissance);
    voltage.update(Voltage);
    frequency.update(Frequency);
    intensite1.update(Intensite1);
    intensite2.update(Intensite2);
    puissance.update(Power1);
    power2.update(Power2);
    energy1.update(Energy1);
    energy2.update(Energy2);
    valdim1.update(valDim1);
    valdim2.update(valDim2);
    button.update(Auto);

    dashboard.sendUpdates();
    delay(50);

    // affichage écran //

    u8g2.clearBuffer();  // on efface ce qui se trouve déjà dans le buffer

    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10, 10);     // position du début du texte
    u8g2.print("Le Profes'S");  // écriture de texte
    u8g2.setFont(u8g2_font_unifont_t_symbols);
    u8g2.drawGlyph(55, 13, 0x2600);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(66, 10);  // position du début du texte
    u8g2.print("laire");     // écriture de texte

    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(40, 30);
    u8g2.print(Power1);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(110, 30);
    u8g2.print("W");  // écriture de texte
    u8g2.setCursor(10, 62);
    u8g2.print("Sauvés : ");  // écriture de texte
    u8g2.setCursor(80, 60);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(Energy1);  // écriture de texte
    u8g2.setCursor(110, 60);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.print("KWH");  // écriture de texte
    u8g2.drawRFrame(30, 15, 70, 22, 7);
    u8g2.setCursor(10, 45);
    u8g2.print(ajustePuissance);
    u8g2.setCursor(10, 55);
    u8g2.print(WiFi.localIP());

    if (Power1 > 20) {
      u8g2.setFont(u8g2_font_emoticons21_tr);
      u8g2.drawGlyph(12, 36, 0x0036);

    }

    else {
      u8g2.setFont(u8g2_font_emoticons21_tr);
      u8g2.drawGlyph(12, 36, 0x0026);
    }

    u8g2.sendBuffer();  // l'image qu'on vient de construire est affichée à l'écran

    mqtt.loop();
    // Send values to Home Assistant every 3 seconds
    if ((millis() - lastTempPublishAt) > 3000) {


      /*

    button.update(Auto);
*/

      ha_voltage.setValue(Voltage);
      // Intensite1 = ByteData[2] * 0.0001;   // Intensité 1
      ha_current1.setValue(Intensite1);
      // Power 1 in %
      ha_power1.setValue(Power1);
      // Energy1 = ByteData[4] * 0.0001;      // Energie 1
      ha_energy1.setValue(Energy1);
      // ???
      // ha_production.setValue(XXX);
      // Frequency
      ha_frequency.setValue(Frequency);
      // Intensite2 = ByteData[10] * 0.0001;  // Intensité 2
      ha_current2.setValue(Intensite2);
      // Power2
      ha_power2.setValue(Power2);
      //
      ha_energy2.setValue(Energy2);
      // Energy saved
      ha_energy_saved.setValue(ajustePuissance);
      ha_dimmer1_output.setValue(valDim1);
      ha_dimmer2_output.setValue(valDim2);
      ha_mode.setValue(Auto);
      lastTempPublishAt = millis();
      lastTemp += 0.5;
    }
  }
}


void loop() {
}
