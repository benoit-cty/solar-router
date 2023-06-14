/* Routeur solaire développé par le Profes'Solaire en avril 2023
Ce code est basé sur la v5.20 mais évolue depuis indépendemment.

- 2 sorties 16A / 3000 watts
- 1 relais
- 1 serveur web Dash Lite avec On / Off
 * ESP32 + JSY-MK-194 (16 et 17) + Dimmer1 24A-600V (35 ZC et 25 PW) + Dimmer 2
24A-600V ( 35 ZC et 26 PW) + écran Oled (22 : SCK et 21 SDA) + relay (13)
 Utilisation des 2 Cores de l'Esp32

 Le capteur 1, celui soudé à la carte, Power1,  est celui qui mesure le surplus envoyé au balon. => C'est donc lui qui peut mesurer l'énergie éconnomisée grâce au routeur.
 Le capteur 2, celui déporté, Power2, mesure la consommation globale pour détecter s'il y a un surplus.
*/
// Librairies //
#include "config.h"  // Local file to store your config, like  Wifi Password
#include <HardwareSerial.h>
#include <RBDdimmer.h>  // Download from https://github.com/RobotDynOfficial/RBDDimmer
#include <WiFi.h>
#include <Wire.h>

WiFiClient client;

#ifdef USE_SCREEN
#include "router_screen.h"
#endif

#ifdef USE_DASHBOARD
#include "router_dashboard.h"
#endif

#ifdef USE_MQTT
#include "router_mqtt.h"
#endif

#include <NTPClient.h>  // Add "NTPClient"  from library manager

// JSY-MK-194 serial speed
#define MK194_FACTORY_SPEED 4800
#define MK194_SPEED 38400

// Size of a MAC-address or BSSID
#define WL_MAC_ADDR_LENGTH 6

byte SensorData[62];
int ByteData[20];
long baudRates[] = { 38400, 4800, 9600, 19200, 57600, 115200 };  // array of baud rates to test
int numBaudRates = 6;                                            // number of baud rates to test


// déclaration des variables//

float routagePuissance = -20; /* puissance d'injection réseau en watts : pour plus de précision,
            ajuster suivant votre charge branchée "exemple pour 3000w => -30 /
            pour 1000w => -10 / pour 500w => -5 */
int ajustePuissance = 0;      /* réglage puissance */
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
float Voltage, Intensite1, Energy1, Frequency, PowerFactor1, PowerFactor2, Intensite2,
  Energy2, Sens1, Sens2;
int Power1;
int Power2;
boolean Auto = 1;
int Value;
float relayOn = 1000;
float relayOff = 800;
float EnergySavedDaily = 0;  // énergie sauvées le jour J et remise à zéro tous les jours //
float EnergyInit;
int Start = 1;  // variable de démarrage du programme //


// See config.h to set your Wifi config
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// NTP timeClient
WiFiUDP ntpUDP;
// 7200 : GMT+2
NTPClient timeClient(ntpUDP, "fr.pool.ntp.org", 7200, 60000);

/* Broches utilisées */
const int zeroCrossPin = 35; /* broche utilisée pour le zéro crossing */
const int pulsePin1 = 25;    /* broche impulsions routage 1*/
const int pulsePin2 = 26;    /* broche impulsions routage 2*/
const int Relay1 = 13;

dimmerLamp dimmer1(pulsePin1, zeroCrossPin);
dimmerLamp dimmer2(pulsePin2, zeroCrossPin);

unsigned long lastPrint = 0;

// Multi-core
TaskHandle_t Task1, Task2, Task3, Task4, Task5;

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
  setup_mqtt();
#endif
  // Test JSY-MK-194 reading
  while (!sensor_speed()) {
    sensor_speed();
  }
  timeClient.begin();  // Init client NTP
  // Code pour créer un Task Core 0//
  xTaskCreatePinnedToCore(
    Task_PowerMonitoring, /* Task function. */
    "Task1",              /* name of task. */
    10000,                /* Stack size of task */
    NULL,                 /* parameter of the task */
    1,                    /* priority of the task */
    &Task1,               /* Task handle to keep track of created task */
    0);                   /* pin task to core 0 */
  delay(500);

  // Code pour créer un Task Core 1//
  xTaskCreatePinnedToCore(
    Task_Compute, /* Task function. */
    "Compute",    /* name of task. */
    10000,        /* Stack size of task */
    NULL,         /* parameter of the task */
    1,            /* priority of the task */
    &Task2,       /* Task handle to keep track of created task */
    1);           /* pin task to core 1 */
#ifdef USE_SCREEN
  xTaskCreatePinnedToCore(
    Task_Screen, /* Task function. */
    "Screen",    /* name of task. */
    40000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    2,           /* priority of the task */
    &Task3,      /* Task handle to keep track of created task */
    1);          /* pin task to core 1 */
#endif
#ifdef USE_DASHBOARD
  xTaskCreatePinnedToCore(
    Task_Dashboard, /* Task function. */
    "Dashboard",    /* name of task. */
    40000,          /* Stack size of task */
    NULL,           /* parameter of the task */
    2,              /* priority of the task */
    &Task3,         /* Task handle to keep track of created task */
    1);             /* pin task to core 1 */
#endif
#ifdef USE_MQTT
  xTaskCreatePinnedToCore(
    Task_MQTT, /* Task function. */
    "MQTT",    /* name of task. */
    40000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    2,         /* priority of the task */
    &Task3,    /* Task handle to keep track of created task */
    1);        /* pin task to core 1 */
#endif
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
  delay(100);
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
  ByteData[1] = SensorData[3] * 16777216 + SensorData[4] * 65536 + SensorData[5] * 256 + SensorData[6];      // Tension en Volts
  ByteData[2] = SensorData[7] * 16777216 + SensorData[8] * 65536 + SensorData[9] * 256 + SensorData[10];     // Intensité 1 en Ampères
  ByteData[3] = SensorData[11] * 16777216 + SensorData[12] * 65536 + SensorData[13] * 256 + SensorData[14];  // Puissance 1 en Watts
  ByteData[4] = SensorData[15] * 16777216 + SensorData[16] * 65536 + SensorData[17] * 256 + SensorData[18];  // Energie 1 en kwh
  ByteData[5] = SensorData[19] * 16777216 + SensorData[20] * 65536 + SensorData[21] * 256 + SensorData[22];  // facteur de puissance
  // ByteData[6] = // 1 carbon dioxide emissions
  ByteData[7] = SensorData[27];                                                                               // sens 1 du courant
  ByteData[8] = SensorData[31] * 16777216 + SensorData[32] * 65536 + SensorData[33] * 256 + SensorData[34];   // Fréquence en hz
  ByteData[9] = SensorData[28];                                                                               // sens 2 du courant
  ByteData[10] = SensorData[39] * 16777216 + SensorData[40] * 65536 + SensorData[41] * 256 + SensorData[42];  // Intensité 2 en Ampères
  ByteData[11] = SensorData[43] * 16777216 + SensorData[44] * 65536 + SensorData[45] * 256 + SensorData[46];  // Puissance 2 en Watts
  ByteData[12] = SensorData[47] * 16777216 + SensorData[48] * 65536 + SensorData[49] * 256 + SensorData[50];  // Energie 2 en kwh
  ByteData[13] = SensorData[51] * 16777216 + SensorData[52] * 65536 + SensorData[53] * 256 + SensorData[54];  // facteur de puissance
  // ByteData[14] = // 1 carbon dioxide emissions

  ///////// Normalize values ///////////////
  Voltage = ByteData[1] * 0.0001;     // Tension
  Intensite1 = ByteData[2] * 0.0001;  // Intensité 1
  Power1 = ByteData[3] * 0.0001;      // Puissance 1
  Energy1 = ByteData[4] * 0.0001;     // Energie 1
  PowerFactor1 = ByteData[5] * 0.0001;
  Sens1 = ByteData[7];                 // Sens 1
  Sens2 = ByteData[9];                 // Sens 2
  Frequency = ByteData[8] * 0.01;      // Fréquence
  Intensite2 = ByteData[10] * 0.0001;  // Intensité 2
  Power2 = ByteData[11] * 0.0001;      // Puissance 2
  Energy2 = ByteData[12] * 0.0001;     // Energie 2
  PowerFactor2 = ByteData[13] * 0.0001;

  if (Sens2 == 1) {
    ajustePuissance = -Power2;
  } else {
    ajustePuissance = Power2;
  }
  if ((millis() - lastPrint) > 60 * 1000) {
    Serial.print(timeClient.getFormattedTime());
    Serial.print(" - INFO - ReadPowerMeter() - Power1:");
    Serial.print(Power1);
    Serial.print("W Power2:");
    Serial.print(Power2);
    Serial.print("W PowerFactor2:");
    Serial.print(PowerFactor2);
    Serial.print(" WiFi.status:");
    Serial.println(WiFi.status());
    /* WiFi.status :
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_WRONG_PASSWORD   = 6,
    WL_DISCONNECTED     = 7
    */
    // if WiFi is down, try reconnecting
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(" - WARNING - ReadPowerMeter() - WiFi Lost, reconnecting ...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
    lastPrint = millis();
  }
}

void Task_PowerMonitoring(void *pvParameters) {
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
void Task_Compute(void *pvParameters) {
  for (;;) {
    timeClient.update();

    if (timeClient.getHours() == 23 & timeClient.getMinutes() == 59 & timeClient.getSeconds() == 59) {
      EnergySavedDaily = 0;
      Start = 1;
    }

    delay(50);


    if (Start == 1) {
      ReadPowerMeter();
      EnergyInit = Energy1;
      Start = 0;
    }

    EnergySavedDaily = Energy1 - EnergyInit;
  }  // end for loop
}

#ifdef USE_SCREEN
// Screen display //
void Task_Screen(void *pvParameters) {
  for (;;) {
    screen_loop(Voltage, Intensite1, Power1, Energy1, EnergySavedDaily, Frequency, PowerFactor2, Intensite2, Power2,
                Energy2, ajustePuissance, valDim1, valDim2, Auto);
  }
}
#endif
#ifdef USE_DASHBOARD
// affichage page web //
void Task_Dashboard(void *pvParameters) {
  for (;;) {
    dashboard_loop(Voltage, Intensite1, Power1, Energy1, EnergySavedDaily, Frequency, PowerFactor2, Intensite2, Power2,
                   Energy2, ajustePuissance, valDim1, valDim2, Auto);
  }
}
#endif
#ifdef USE_MQTT
void Task_MQTT(void *pvParameters) {
  const TickType_t taskPeriod = 10 * 1000;  // In ms
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    mqtt_loop(Voltage, Intensite1, Power1, Energy1, Frequency, PowerFactor2, Intensite2, Power2,
              Energy2, ajustePuissance, valDim1, valDim2, Auto);

    vTaskDelayUntil(&xLastWakeTime, taskPeriod);
  }
}
#endif

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
