/* Routeur solaire développé par le Profes'Solaire en avril 2023
Ce code est basé sur la v5.20 mais évolue depuis indépendemment.

- 2 sorties 16A / 3000 watts
- 1 relais
- 1 serveur web Dash Lite avec On / Off
 * ESP32 + JSY-MK-194 (16 et 17) + Dimmer1 24A-600V (35 ZC et 25 PW) + Dimmer 2
24A-600V ( 35 ZC et 26 PW) + écran Oled (22 : SCK et 21 SDA) + relay (13)
 Utilisation des 2 Cores de l'Esp32
*/
// Librairies //
#include "config.h"  // Local file to store your config, like  Wifi Password
#include <HardwareSerial.h>
#include <RBDdimmer.h>  // Download from https://github.com/RobotDynOfficial/RBDDimmer

#ifdef USE_SCREEN
#include <U8g2lib.h>  // Install "U8g2" from library manager
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
#endif

#include <WiFi.h>
#include <Wire.h>

#ifdef USE_DASHBOARD
#include <AsyncTCP.h>           // Install "AsyncTCP" from library manager
#include <ESPAsyncWebServer.h>  // Download from https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPDash.h>            // Install "ESP-DASH" from library manager
#endif

#ifdef USE_MQTT
// Add also "ArduinoJson"  from library manager
#include <ArduinoHA.h>  // Download it from https://github.com/dawidchyrzynski/arduino-home-assistant/releases/tag/2.0.0
// And add PubSubClient (by Nick O'Leary) from library manager
#endif

#include <NTPClient.h>  // Add "NTPClient"  from library manager

// JSY-MK-194 serial speed
#define MK194_FACTORY_SPEED 4800
#define MK194_SPEED 38400

// Size of a MAC-address or BSSID
#define WL_MAC_ADDR_LENGTH 6

byte SensorData[62];
int ByteData[20];
long baudRates[] = { 4800, 9600, 19200, 38400, 57600, 115200 };  // array of baud rates to test
int numBaudRates = 6;                                            // number of baud rates to test


// déclaration des variables//

float routagePuissance = -20; /* puissance d'injection réseau en watts : pour plus de précision,
            ajuster suivant votre charge branchée "exemple pour 3000w => -30 /
            pour 1000w => -10 / pour 500w => -5 */
int ajustePuissance = 0; /* réglage puissance */
float puissanceRoutage = 0;
float mini_puissance = 0;
float max_puissance = 40;  // Set to the maximum power of your device. 2 000 for
                           // a boiler. 40 for a light bulb.
float pas_dimmer1 = 1;
float pas_dimmer2 = 0.1;
float pas_dimmer3 = 5;
float valDim1 = 0;
float valDim2 = 0;
float maxDimmer1 = 95;
float minDimmer1 = 0;
float Voltage, Intensite1, Energy1, Frequency, PowerFactor1, Intensite2,
  Energy2, Sens1, Sens2;
int Power1;
int Power2;
boolean Auto = 1;
int Value;
float relayOn = 1000;
float relayOff = 800;
float EnergySavedDaily = 0;  // énergie sauvées le jour J et remise à zéro tous les jours //
float EnergyInit;
int Start = 1;   // variable de démarrage du programme //
byte ecran = 0;  // variable affichage kwh sauvés jour / total ///

// See config.h to set your Wifi config
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// NTP timeClient
WiFiUDP ntpUDP;
// 7200 : GMT+2
NTPClient timeClient(ntpUDP, "fr.pool.ntp.org", 7200, 60000);

#ifdef USE_DASHBOARD
// Configure Dashboard web
AsyncWebServer server(80);
ESPDash dashboard(&server);
Card button(&dashboard, BUTTON_CARD, "Auto");
Card slider(&dashboard, SLIDER_CARD, "Puissance sortie 1 en %", "", 0, 95);
Card voltage(&dashboard, GENERIC_CARD, "Voltage", "V");
Card power2(&dashboard, GENERIC_CARD, "Power2", "W");
Card frequency(&dashboard, GENERIC_CARD, "frequency", "Hz");
Card consommationsurplus(&dashboard, GENERIC_CARD, "surplus / consommation",
                         "watts");
Card puissance(&dashboard, GENERIC_CARD, "puissance envoyée au ballon",
               "watts");
Card intensite1(&dashboard, GENERIC_CARD, "Intensité 1", "A");
Card intensite2(&dashboard, GENERIC_CARD, "Intensité 2", "A");
Card energy1(&dashboard, GENERIC_CARD, "énergie sauvée", "kwh");
Card energy_saved_daily(&dashboard, GENERIC_CARD, "énergie sauvée aujourd'hui", "kwh");
Card energy2(&dashboard, GENERIC_CARD, "énergie 2", "kwh");
Card valdim1(&dashboard, PROGRESS_CARD, "sortie 1", "%", 0, 95);
Card valdim2(&dashboard, PROGRESS_CARD, "sortie 2", "%", 0, 95);
#endif

/* Broches utilisées */
const int zeroCrossPin = 35; /* broche utilisée pour le zéro crossing */
const int pulsePin1 = 25;    /* broche impulsions routage 1*/
const int pulsePin2 = 26;    /* broche impulsions routage 2*/
const int Relay1 = 13;

dimmerLamp dimmer1(pulsePin1, zeroCrossPin);
dimmerLamp dimmer2(pulsePin2, zeroCrossPin);
WiFiClient client;

///////////// MQTT with Home Assistant /////////////
#ifdef USE_MQTT

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
#endif

unsigned long lastMqttPublishAt = 0;
unsigned long lastPrint = 0;
float lastTemp = 0;

// Multi-core
TaskHandle_t Task1;
TaskHandle_t Task2;

/////////////////// S E T U P /////////////////////////////
void setup() {
  Serial.begin(115200);
  Serial2.begin(MK194_SPEED, SERIAL_8N1, RXD2, TXD2);  // Connection to JSY-MK-194
  delay(300);
#ifdef USE_SCREEN
  u8g2.begin();            // ECRAN OLED
  u8g2.enableUTF8Print();  // nécessaire pour écrire des caractères accentués
#endif
  dimmer1.begin(NORMAL_MODE, ON);
  pinMode(Relay1, OUTPUT);
  digitalWrite(Relay1, LOW);
  // WiFi connection
  WiFi.mode(WIFI_STA);  // Optional
  WiFi.begin(ssid, password);
#ifdef USE_DASHBOARD
  server.begin();
#endif
  delay(100);
#ifdef USE_MQTT
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
#endif
  // Test JSY-MK-194 reading
  while (!sensor_speed()) {
    sensor_speed();
  }
  timeClient.begin();  // Init client NTP
  // Code pour créer un Task Core 0//
  xTaskCreatePinnedToCore(
    Task1PowerMonitoring, /* Task function. */
    "Task1",              /* name of task. */
    10000,                /* Stack size of task */
    NULL,                 /* parameter of the task */
    1,                    /* priority of the task */
    &Task1,               /* Task handle to keep track of created task */
    0);                   /* pin task to core 0 */
  delay(500);

  // Code pour créer un Task Core 1//
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

bool sensor_speed() {
  for (int i = 0; i < numBaudRates; i++) {
    delay(1000);
    Serial.print("Testing baud rate ");
    Serial.print(baudRates[i]);
    Serial.println("...");
    Serial2.begin(baudRates[i], SERIAL_8N1, RXD2, TXD2);  // set the baud rate
    delay(100);                                           // wait for the sensor to stabilize
    if (ReadPowerMeterSensor()) {
      Serial.print("Sensor is working at speed ");
      Serial.println(baudRates[i]);
      if (baudRates[i] == MK194_SPEED) {
        return true;
      }
      for (int i = 0; i < sizeof(SensorData); i++) {
        Serial.print(SensorData[i], HEX);
        Serial.print("-");
      }
      if (baudRates[i] != 38400) {
        set_sensor_speed();
      }
    }
    Serial.println("end Serial");  // add a blank line for readability
    Serial2.end();
  }
}

void set_sensor_speed() {
  byte msg[] = { 0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x08, 0xA7, 0x82 };  // Switch to 38 400 bps (MK194_SPEED)
  for (int i = 0; i < sizeof(msg); i++) {
    Serial2.write(msg[i]);
    delay(60);
  }
  Serial2.end();
  Serial2.begin(MK194_SPEED, SERIAL_8N1, RXD2, TXD2);
}


bool ReadPowerMeterSensor() {
  // Modbus RTU message to get the data
  byte msg[] = { 0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18 };
  int len = sizeof(msg);

  // Reset data
  for (int i = 0; i < sizeof(SensorData); i++) {
    SensorData[i] = 0x00;
  }

  // Send message
  for (int i = 0; i < len; i++) {
    Serial2.write(msg[i]);
  }
  delay(500);
  // Get data from JSY-MK-194
  int a = 0;
  while (Serial2.available()) {
    SensorData[a] = Serial2.read();
    a++;
  }
  // Sanity Checks
  if (SensorData[0] != 0x01) {
    Serial.print("ERROR - ReadPowerMeterSensor() - Message received do not start with 0x01, message length is ");
    Serial.println(a);
    for (int i = 0; i < a; i++) {
      Serial.print(SensorData[i], HEX);
      Serial.print("-");
    }
    Serial.println(".");
    return false;
  }

  if (a != 61) {
    Serial.print("ERROR - ReadPowerMeterSensor() - Message received have wrong length: ");
    Serial.println(a);
    for (int i = 0; i < a; i++) {
      Serial.print(i);
      Serial.print("=");
      Serial.print(SensorData[i], HEX);
      Serial.print(" ");
    }
    Serial.println(".");
    return false;
  }
  return true;
}

// programme utilisant le Core 1 de l'ESP32//
void ReadPowerMeter() {
  delay(60);
  if (!ReadPowerMeterSensor()) {
    return;
  }
  //////// Extract response /////////////////
  ByteData[1] = SensorData[3] * 16777216 + SensorData[4] * 65536 + SensorData[5] * 256 + SensorData[6];       // Tension en Volts
  ByteData[2] = SensorData[7] * 16777216 + SensorData[8] * 65536 + SensorData[9] * 256 + SensorData[10];      // Intensité 1 en Ampères
  ByteData[3] = SensorData[11] * 16777216 + SensorData[12] * 65536 + SensorData[13] * 256 + SensorData[14];   // Puissance 1 en Watts
  ByteData[4] = SensorData[15] * 16777216 + SensorData[16] * 65536 + SensorData[17] * 256 + SensorData[18];   // Energie 1 en kwh
  ByteData[7] = SensorData[27];                                                                               // sens 1 du courant
  ByteData[9] = SensorData[28];                                                                               // sens 2 du courant
  ByteData[8] = SensorData[31] * 16777216 + SensorData[32] * 65536 + SensorData[33] * 256 + SensorData[34];   // Fréquence en hz
  ByteData[10] = SensorData[39] * 16777216 + SensorData[40] * 65536 + SensorData[41] * 256 + SensorData[42];  // Intensité 2 en Ampères
  ByteData[11] = SensorData[43] * 16777216 + SensorData[44] * 65536 + SensorData[45] * 256 + SensorData[46];  // Puissance 2 en Watts
  ByteData[12] = SensorData[47] * 16777216 + SensorData[48] * 65536 + SensorData[49] * 256 + SensorData[50];  // Energie 2 en kwh

  ///////// Normalize values ///////////////
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
  if ((millis() - lastPrint) > 3000) {
    Serial.print("INFO - ReadPowerMeter() - Power1:");
    Serial.print(Power1);
    Serial.print("W Power2:");
    Serial.print(Power2);
    Serial.print("W Voltage:");
    Serial.println(Voltage);
    Serial.println(timeClient.getFormattedTime());
    lastPrint = millis();
  }
}

void Task1PowerMonitoring(void *pvParameters) {
  for (;;) {
#ifdef USE_DASHBOARD
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
#endif
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

// Core 2 of ESP32
void Task2ExternalInterface(void *pvParameters) {
  for (;;) {
    timeClient.update();
    
    if (timeClient.getHours() == 23 & timeClient.getMinutes() == 59 & timeClient.getSeconds() == 59) {
      EnergySavedDaily = 0;
      Start = 1;
    }
#ifdef USE_DASHBOARD
    // affichage page web //
    consommationsurplus.update(ajustePuissance);
    voltage.update(Voltage);
    frequency.update(Frequency);
    intensite1.update(Intensite1);
    intensite2.update(Intensite2);
    puissance.update(Power1);
    power2.update(Power2);
    energy1.update(Energy1);
    energyj.update(EnergySavedDaily);
    energy2.update(Energy2);
    valdim1.update(valDim1);
    valdim2.update(valDim2);
    button.update(Auto);
    dashboard.sendUpdates();
#endif
    delay(50);
#ifdef USE_SCREEN
    // Screen display //

    u8g2.clearBuffer();  // on efface ce qui se trouve déjà dans le buffer

    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(25, 10);     // position du début du texte
    u8g2.print("Le Profes'S");  // écriture de texte
    u8g2.setFont(u8g2_font_unifont_t_symbols);
    u8g2.drawGlyph(70, 13, 0x2600);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(81, 10);      // position du début du texte
    u8g2.print("laire  v6.31");  // écriture de texte

    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(40, 30);
    u8g2.print(Power1);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(110, 30);
    u8g2.print("W");  // écriture de texte
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10, 47);
    u8g2.print(ajustePuissance);  // injection ou surplus //
    u8g2.setCursor(10, 64);
    u8g2.print("Sauvés : ");  // écriture de texte


    u8g2.setCursor(115, 64);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.print("KWH");  // écriture de texte
    u8g2.drawRFrame(30, 15, 70, 22, 7);
    u8g2.setCursor(10, 47);
    u8g2.setCursor(75, 47);
    u8g2.print(WiFi.localIP());
    ecran = ecran + 1;

    if (ecran == 1 || ecran == 2 || ecran == 3 || ecran == 4) {
      u8g2.setFont(u8g2_font_4x6_tf);
      u8g2.setCursor(10, 64);
      u8g2.print("Sauvés T : ");  // écriture de texte
      u8g2.setCursor(85, 64);
      u8g2.setFont(u8g2_font_7x13B_tf);
      u8g2.print(Energy1);  // écriture de texte
      delay(500);
    }
    if (ecran == 5 || ecran == 6 || ecran == 7) {
      u8g2.setFont(u8g2_font_4x6_tf);
      u8g2.setCursor(10, 64);
      u8g2.print("Sauvés J : ");  // écriture de texte
      u8g2.setCursor(85, 64);
      u8g2.setFont(u8g2_font_7x13B_tf);
      u8g2.print(EnergySavedDaily);  // écriture de texte
      delay(500);
    }

    if (ecran == 8) {
      u8g2.setFont(u8g2_font_4x6_tf);
      u8g2.setCursor(10, 64);
      u8g2.print("Sauvés J : ");  // écriture de texte
      u8g2.setCursor(85, 64);
      u8g2.setFont(u8g2_font_7x13B_tf);
      u8g2.print(EnergySavedDaily);  // écriture de texte
      delay(500);
      ecran = 0;
    }
    if (Power1 > 20) {
      u8g2.setFont(u8g2_font_emoticons21_tr);
      u8g2.drawGlyph(12, 36, 0x0036);

    }

    else {
      u8g2.setFont(u8g2_font_emoticons21_tr);
      u8g2.drawGlyph(12, 36, 0x0026);
    }

    u8g2.sendBuffer();  // display data on screen
#endif

#ifdef USE_MQTT
    mqtt.loop();
    // Send values to Home Assistant every 3 seconds
    if ((millis() - lastMqttPublishAt) > 3000) {
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
      lastMqttPublishAt = millis();
      lastTemp += 0.5;
    }
#endif
    if (Start == 1) {
      ReadPowerMeter();
      EnergyInit = Energy1;
      Start = 0;
    }

    EnergySavedDaily = Energy1 - EnergyInit;
  }  // end for loop
}

bool compare_array(byte arr1[], byte arr2[]) {
  bool isEqual = true;
  if (sizeof(arr1) != sizeof(arr2)) {
    return false;
  }
  for (int i = 0; i < sizeof(arr1); i++) {
    if (arr1[i] != arr2[i]) {
      isEqual = false;
      break;
    }
  }
  return isEqual;
}


void loop() {}
