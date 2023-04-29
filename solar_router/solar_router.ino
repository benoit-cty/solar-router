/* Routeur solaire développé par le Profes'Solaire v5.20 - 12-04-2023 - professolaire@gmail.com
- 2 sorties 16A / 3000 watts
- 1 relais
- 1 serveur web Dash Lite avec On / Off
 * ESP32 + JSY-MK-194 (16 et 17) + Dimmer1 24A-600V (35 ZC et 25 PW) + Dimmer 2 24A-600V ( 35 ZC et 26 PW) + écran Oled (22 : SCK et 21 SDA) + relay (13)
 Utilisation des 2 Cores de l'Esp32
*/
// Librairies //
#include "config.h" // Local file to store your config, like  Wifi Password
#include <HardwareSerial.h>
#include <RBDdimmer.h> // Download from https://github.com/RobotDynOfficial/RBDDimmer
#include <U8g2lib.h> // Install "U8g2" from library manager 
#include <Wire.h>
#include <WiFi.h>
#include <ESPDash.h> // Install "ESP-DASH" from library manager 
#include <AsyncTCP.h> // Install "AsyncTCP" from library manager 
#include <ESPAsyncWebServer.h> // Download from https://github.com/me-no-dev/ESPAsyncWebServer
// Add also "ArduinoJson"  from library manager
#include <ArduinoHA.h> // Download it from https://github.com/dawidchyrzynski/arduino-home-assistant/releases/tag/2.0.0
// And add PubSubClient (by Nick O'Leary) from library manager

#define RXD2 16
#define TXD2 17
// Size of a MAC-address or BSSID
#define WL_MAC_ADDR_LENGTH 6

byte ByteArray[250];
int ByteData[20];
 
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


//déclaration des variables//

float routagePuissance = -30; /* puissance d'injection réseau en watts : pour plus de précision, ajuster suivant votre charge branchée "exemple pour 3000w => -30 / pour 1000w => -10 / pour 500w => -5 */
int ajustePuissance = 0; /* réglage puissance */
float puissanceRoutage = 0;
float mini_puissance  = 0;
float max_puissance  = 3000;
float pas_dimmer1  = 1;
float pas_dimmer2  = 0.1;
float pas_dimmer3  = 5;
float valDim1 = 0;
float valDim2 = 0;
float maxDimmer1 = 95;
float minDimmer1 = 0;
float Voltage,Intensite1,Energy1,Frequency,PowerFactor1,Intensite2,Energy2,Sens1,Sens2;
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
Card consommationsurplus(&dashboard, GENERIC_CARD, "surplus / consommation", "watts");
Card puissance(&dashboard, GENERIC_CARD, "puissance envoyée au ballon", "watts");
Card energy1(&dashboard, GENERIC_CARD, "énergie sauvée", "kwh");
Card valdim1(&dashboard, PROGRESS_CARD, "sortie 1", "%", 0, 95);
Card valdim2(&dashboard, PROGRESS_CARD, "sortie 2", "%", 0, 95);

////////////// Fin connexion wifi //////////


TaskHandle_t Task1;
TaskHandle_t Task2;

/* Broches utilisées */
const int zeroCrossPin = 35; /* broche utilisée pour le zéro crossing */
const int pulsePin1 = 25; /* broche impulsions routage 1*/
const int pulsePin2 = 26; /* broche impulsions routage 2*/
const int Relay1 = 13;


dimmerLamp dimmer1(pulsePin1, zeroCrossPin);
dimmerLamp dimmer2(pulsePin2, zeroCrossPin);

/* MQTT with Home Assistant
 * From https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/hvac/hvac.ino
 */
WiFiClient client;
// Unique ID must be set for MQTT
HADevice device(MQTT_TOPIC_NAME);
HAMqtt mqtt(client, device);
/* MQTT Devices */
// Mode Auto ON/OFF

// Power 1 in %
HASensorNumber power1("Power output 1 in %", HASensorNumber::PrecisionP1);
// PV Usage in W
HASensorNumber production("surplus / consommation", HASensorNumber::PrecisionP1);
// Power send to boiler in W
HASensorNumber power_boiler("puissance envoyée au ballon", HASensorNumber::PrecisionP1);
// Energy saved
HASensorNumber energy_saved("Energy saved", HASensorNumber::PrecisionP1);
// Dimmer 1 output in %
HASensorNumber dimmer_output("Dimmer output", HASensorNumber::PrecisionP1);


// By default HAHVAC supports only reporting of the temperature.
// You can enable feature you need using the second argument of the constructor.
// Please check the documentation of the HAHVAC class.
// HAHVAC hvac(
//   "boiler",
//   HAHVAC::TargetTemperatureFeature | HAHVAC::PowerFeature | HAHVAC::ModesFeature
// );

unsigned long lastTempPublishAt = 0;
float lastTemp = 0;

// void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender) {
//     float temperatureFloat = temperature.toFloat();

//     Serial.print("Target temperature: ");
//     Serial.println(temperatureFloat);

//     sender->setTargetTemperature(temperature); // report target temperature back to the HA panel
// }

// void onPowerCommand(bool state, HAHVAC* sender) {
//   if (state) {
//     Serial.println("Power on");
//   } else {
//     Serial.println("Power off");
//   }
// }


// void onModeCommand(HAHVAC::Mode mode, HAHVAC* sender) {
//     Serial.print("Mode: ");
//     if (mode == HAHVAC::OffMode) {
//         Serial.println("off");
//     } else if (mode == HAHVAC::AutoMode) {
//         Serial.println("auto");
//     } else if (mode == HAHVAC::CoolMode) {
//         Serial.println("cool");
//     } else if (mode == HAHVAC::HeatMode) {
//         Serial.println("heat");
//     } else if (mode == HAHVAC::DryMode) {
//         Serial.println("dry");
//     } else if (mode == HAHVAC::FanOnlyMode) {
//         Serial.println("fan only");
//     }

//     sender->setMode(mode); // report mode back to the HA panel
// }

void setup() {

  Serial.begin(115200);
  Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2); //PORT DE CONNEXION AVEC LE CAPTEUR JSY-MK-194
  delay(300);
  u8g2.begin(); // ECRAN OLED
  u8g2.enableUTF8Print(); //nécessaire pour écrire des caractères accentués
  dimmer1.begin(NORMAL_MODE, ON); 
  pinMode(Relay1, OUTPUT);
  digitalWrite(Relay1, LOW);




  WiFi.mode(WIFI_STA); //Optional
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
  // Power 1 in %
  // power1.setIcon("mdi:home");
  //   power1.setName("Analog voltage");
  //   power1.setUnitOfMeasurement("V");
  // PV Usage in W
  // HASensorNumber production("surplus / consommation", HASensorNumber::PrecisionP1);
  // Power send to boiler in W
  //HASensorNumber power_boiler("puissance envoyée au ballon", HASensorNumber::PrecisionP1);
  // Energy saved
  //HASensorNumber energy_saved("Energy saved", HASensorNumber::PrecisionP1);
  energy_saved.setIcon("mdi:home");
  energy_saved.setName("Energy saved");
  energy_saved.setUnitOfMeasurement("kWh");
  // Dimmer 1 output in %
  //HASensorNumber dimmer_output("Dimmer output", HASensorNumber::PrecisionP1);



  // assign callbacks (optional)
  // hvac.onTargetTemperatureCommand(onTargetTemperatureCommand);
  // hvac.onPowerCommand(onPowerCommand);
  // hvac.onModeCommand(onModeCommand);

  // configure HVAC (optional)
  // hvac.setName("My HVAC");
  // hvac.setMinTemp(10);
  // hvac.setMaxTemp(30);
  // hvac.setTempStep(0.5);

  // You can set retain flag for the HA commands
  // hvac.setRetain(true);

  // You can choose which modes should be available in the HA panel
  // hvac.setModes(HAHVAC::OffMode | HAHVAC::CoolMode);
  mqtt.begin(MQTT_BROKER_IP, MQTT_USER, MQTT_PASSWORD);

  //Code pour créer un Task Core 0//
  xTaskCreatePinnedToCore(
                    Task1PowerMonitoring,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //Code pour créer un Task Core 1//
  xTaskCreatePinnedToCore(
                    Task2ExternalInterface,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
  delay(500); 
          
}

//programme utilisant le Core 1 de l'ESP32//


void Datas ()
{

delay (60);

byte msg[] = {0x01,0x03,0x00,0x48,0x00,0x0E,0x44,0x18};

 int i;
 int len=8; 
               

////// Envoie des requêtes Modbus RTU sur le Serial port 2 

for(i = 0 ; i < len ; i++)
{
      Serial2.write(msg[i]); 
         
}
 len = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////

      

////////// Reception  des données Modbus RTU venant du capteur JSY-MK-194 ////////////////////////


int a = 0;
 while(Serial2.available()) 
 {
ByteArray[a] = Serial2.read();
 a++;
 }

int b = 0;
 String registros;
    for(b = 0 ; b < a ; b++){      

}
////////////////////////////////////////////////////////////////////////////////////////////////////


//////// Conversion HEX /////////////////


ByteData[1] = ByteArray[3] * 16777216 + ByteArray[4] * 65536 + ByteArray[5] * 256 + ByteArray[6]; // Tension en Volts
ByteData[2] = ByteArray[7] * 16777216 + ByteArray[8] * 65536 + ByteArray[9] * 256 + ByteArray[10]; // Intensité 1 en Ampères
ByteData[3] = ByteArray[11] * 16777216 + ByteArray[12] * 65536 + ByteArray[13] * 256 + ByteArray[14]; // Puissance 1 en Watts
ByteData[4] = ByteArray[15] * 16777216 + ByteArray[16] * 65536 + ByteArray[17] * 256 + ByteArray[18]; // Energie 1 en kwh
ByteData[7] = ByteArray[27] ; // sens 1 du courant
ByteData[9] = ByteArray[28] ; // sens 2 du courant
ByteData[8] = ByteArray[31] * 16777216 + ByteArray[32] * 65536 + ByteArray[33] * 256 + ByteArray[34]; // Fréquence en hz
ByteData[10] = ByteArray[39] * 16777216 + ByteArray[40] * 65536 + ByteArray[41] * 256 + ByteArray[42]; // Intensité 2 en Ampères
ByteData[11] = ByteArray[43] * 16777216 + ByteArray[44] * 65536 + ByteArray[45] * 256 + ByteArray[46]; // Puissance 2 en Watts
ByteData[12] = ByteArray[47] * 16777216 + ByteArray[48] * 65536 + ByteArray[49] * 256 + ByteArray[50]; // Energie 2 en kwh


////////////////////////////////////////////////////////////////////////////////////////////////////
  

///////// Normalisation des valeurs ///////////////

Voltage = ByteData[1] * 0.0001;     // Tension
Intensite1 = ByteData[2] * 0.0001;     // Intensité 1
Power1 = ByteData[3] * 0.0001;     // Puissance 1
Energy1 = ByteData[4] * 0.0001;     // Energie 1
Sens1 = ByteData[7];     // Sens 1
Sens2 = ByteData[9];     // Sens 2
Frequency = ByteData[8] * 0.01;     // Fréquence
Intensite2 = ByteData[10] * 0.0001;     // Intensité 2
Power2 = ByteData[11] * 0.0001;     // Puissance 2
Energy2 = ByteData[12] * 0.0001;     // Energie 2

if (Sens2 == 1)
   { ajustePuissance = -Power2;
   }

if (Sens2 == 0)
   { ajustePuissance = Power2;
   }


////////////////////////////////////////////////////////////////////////////////////////////////////


}



void Task1PowerMonitoring( void * pvParameters )
{
for(;;) {



  button.attachCallback([&](bool value){
  Auto = value;
  button.update(Auto);
  dashboard.sendUpdates();
  });

  slider.attachCallback([&](int value){
  Value = value;  
  slider.update(value);
  dashboard.sendUpdates();
  });


if ( Auto == 0)
    { 
      Datas ();
      valDim1 = Value;
      dimmer1.setState(ON);
      delay(100);
      dimmer1.setPower(Value);
    }

if ( Auto == 1 )
    {
      Datas ();
    

// calcul triacs ///
// réglages Dimmer 1 ///

if ( valDim1 < 1 )
    {
      dimmer1.setState(OFF);
    }

if ( Power2 == 0 && Power1 == 0)
    {
      valDim1 = 0;
      dimmer1.setPower(valDim1);
      dimmer1.setState(OFF);
    }

if ( ajustePuissance <= -400 && valDim1 < 90 )
    {
      valDim1 = valDim1 + pas_dimmer3;
      dimmer1.setState(ON);
      delay(200);
      dimmer1.setPower(valDim1);
    }
if ( ajustePuissance > -400 && ajustePuissance <= -150 && valDim1 < 94 )
    {
      valDim1 = valDim1 + pas_dimmer1;
      dimmer1.setState(ON);
      delay(200);
      dimmer1.setPower(valDim1);
    }


if ( ajustePuissance <= routagePuissance && ajustePuissance > -150 && valDim1 < 94.99)
    {
      valDim1 = valDim1 + pas_dimmer2;
      dimmer1.setState(ON);
      delay(200);
      dimmer1.setPower(valDim1);
    }
  

if ( ajustePuissance > routagePuissance && ajustePuissance <= 30  && valDim1 > 0.01 )
    {
      valDim1 = valDim1 - pas_dimmer2;
      dimmer1.setState(ON);
      delay(200);
      dimmer1.setPower(valDim1);
    }      
      
if ( ajustePuissance > 30 && valDim1 > 1 && ajustePuissance <= 150 )                       
    {
      valDim1 = valDim1 - pas_dimmer1;
      dimmer1.setState(ON);
      delay(200);
      dimmer1.setPower(valDim1);
    }
      
if ( ajustePuissance > 150 && valDim1 > 5 )
    {
      valDim1 = valDim1 - pas_dimmer3;
      dimmer1.setState(ON);
      delay(200);
      dimmer1.setPower(valDim1);
    }

// réglages Dimmer 2 ///

if ( valDim1 >= 90 )
    {
     if ( valDim2 < 1 )
         {
           dimmer2.setState(OFF);
         }

     if ( Power2 == 0 && Power1 == 0)
         {
           valDim2 = 0;
           dimmer2.setPower(valDim2);
           dimmer2.setState(OFF);
         }

      if ( ajustePuissance <= -400 && valDim2 < 90 )
          {
            valDim2 = valDim2 + pas_dimmer3;
            dimmer2.setState(ON);
            delay(200);
            dimmer2.setPower(valDim2);
          }
      if ( ajustePuissance > -400 && ajustePuissance <= -150 && valDim2 < 94 )
          {
            valDim2 = valDim2 + pas_dimmer1;
            dimmer2.setState(ON);
            delay(200);
            dimmer2.setPower(valDim2);
          }


      if ( ajustePuissance <= routagePuissance && ajustePuissance > -150 && valDim2 < 94.99)
          {
            valDim2 = valDim2 + pas_dimmer2;
            dimmer2.setState(ON);
            delay(200);
            dimmer2.setPower(valDim2);
          }
  

      if ( ajustePuissance > routagePuissance && ajustePuissance <= 30  && valDim2 > 0.01 )
          {
            valDim2 = valDim2 - pas_dimmer2;
            dimmer2.setState(ON);
            delay(200);
            dimmer2.setPower(valDim2);
          }      
      
      if ( ajustePuissance > 30 && valDim2 > 1 && ajustePuissance <= 150 )                       
           {
            valDim2 = valDim2 - pas_dimmer1;
            dimmer2.setState(ON);
            delay(200);
            dimmer2.setPower(valDim2);
           }
      
      if ( ajustePuissance > 150 && valDim2 > 5 )
           {
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



   

// réglage relay //

if ( Power1 > relayOn) 
  {
    digitalWrite(Relay1, HIGH);
  }
  
  
if ( Power1 < relayOff) 
  { 
    digitalWrite(Relay1, LOW);
  }

        }
  }

}




//programme utilisant le Core 2 de l'ESP32//

void Task2ExternalInterface( void * pvParameters ){

for(;;){


// affichage page web //

        consommationsurplus.update(ajustePuissance);
        puissance.update(Power1);
        energy1.update(Energy1);
        valdim1.update(valDim1);
        valdim2.update(valDim2);
        button.update(Auto);
  
        dashboard.sendUpdates();
        delay(50);

// affichage écran //

        u8g2.clearBuffer(); // on efface ce qui se trouve déjà dans le buffer
               
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(10, 10); // position du début du texte
        u8g2.print("Le Profes'S"); // écriture de texte
        u8g2.setFont(u8g2_font_unifont_t_symbols);
        u8g2.drawGlyph(55, 13, 0x2600);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(66, 10); // position du début du texte
        u8g2.print("laire"); // écriture de texte
        
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(40, 30);
        u8g2.print(Power1);  
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(110, 30);
        u8g2.print("W"); // écriture de texte
        u8g2.setCursor(10,62);
        u8g2.print("Sauvés : "); // écriture de texte
        u8g2.setCursor(80, 60);
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.print(Energy1); // écriture de texte
        u8g2.setCursor(110, 60);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.print("KWH"); // écriture de texte
        u8g2.drawRFrame(30,15,70,22,7);
        u8g2.setCursor(10, 45);
        u8g2.print(ajustePuissance);
        u8g2.setCursor(10, 55);
        u8g2.print(WiFi.localIP());

if (Power1 > 20)
  {
        u8g2.setFont(u8g2_font_emoticons21_tr);
        u8g2.drawGlyph(12, 36, 0x0036);
        
  }

else 
  {
        u8g2.setFont(u8g2_font_emoticons21_tr);
        u8g2.drawGlyph(12, 36, 0x0026);
  }

        u8g2.sendBuffer();  // l'image qu'on vient de construire est affichée à l'écran

    mqtt.loop();
    // Send temp every 3 seconds
    if ((millis() - lastTempPublishAt) > 3000) {
        energy_saved.setValue(lastTemp);
        // hvac.setCurrentTemperature(lastTemp);
        lastTempPublishAt = millis();
        lastTemp += 0.5;
    }

}
}

  
void loop() {
       
                   
}
