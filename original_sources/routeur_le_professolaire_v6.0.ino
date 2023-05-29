/* Routeur solaire développé par le Profes'Solaire v6.31 - 10-05-2023 - professolaire@gmail.com
- 2 sorties 16A / 3000 watts
- 1 relais
- 1 serveur web Dash Lite avec On / Off
- support MQTT Mosquito - Home Assistant
- heure NTP
 * ESP32 + JSY-MK-194 (16 et 17) + Dimmer1 24A-600V (35 ZC et 25 PW) + Dimmer 2 24A-600V ( 35 ZC et 26 PW) + écran Oled (22 : SCK et 21 SDA) + relay (13)
 Utilisation des 2 Cores de l'Esp32
*/



///////// CONFIGURATION ///// PARTIE A MODIFIER POUR VOTRE RESEAU /////////

const char* ssid = "xxxxxxxxxxxxxxxxxx";                                     // nom de votre réseau wifi
const char* password = "xxxxxxxxxxxxxxxxxx";                             // mot de passe de votre réseau wifi
boolean mqtt = 0;                                              // activer ou désactiver MQTT Mosquitto pour Home Assistant : 0 ou 1
#define mqtt_server "192.168.1.100"                             // adresse de votre serveur mqtt //
#define mqtt_user "log"                                  // utilisateur mqtt //
#define mqtt_password "pass"                              // mot de passe mqtt //
///////////////////////////////////////////////////////////////////////////



// Librairies //
#include <HardwareSerial.h>
#include <RBDdimmer.h> // gestion des Dimmers  https://github.com/RobotDynOfficial/RBDDimmer //
#include <U8g2lib.h> // gestion affichage écran Oled  https://github.com/olikraus/u8g2 //
#include <Wire.h> 
#include <WiFi.h>
#include <ESPDash.h> // page web Dash  https://github.com/ayushsharma82/ESP-DASH //
#include <AsyncTCP.h>   //  https://github.com/me-no-dev/AsyncTCP  ///
#include <ESPAsyncWebServer.h>  // https://github.com/me-no-dev/ESPAsyncWebServer  et https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h> //mqtt Home Assistant  https://github.com/knolleary/pubsubclient //
#include <NTPClient.h> // gestion de l'heure https://github.com/arduino-libraries/NTPClient //

WiFiUDP ntpUDP;
/*
* Choix du serveur NTP pour récupérer l'heure, 3600 =1h est le fuseau horaire et 60000=60s est le * taux de rafraichissement
*/
NTPClient temps(ntpUDP, "fr.pool.ntp.org", 7200, 60000);


#define RXD2 16
#define TXD2 17

// configuration MQTT Home Assistant //

#define topic "mosquitto/power1"
#define will_topic "mosquitto/power1/state"
#define will_msg "Wemos Sel Deconnecté"
#define hello_msg "Wemos Sel Connecté
#define probe_name "ESP_Power1"
#define will_qos 0
#define will_retain 0


String jsontomqtt;


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
boolean Auto = 1; // mise en route en automatique //
int Value1;
int Value2;
float relayOn = 1000; // puissance du surplus pour déclencher le relai //
float relayOff = 800; // puissance du surplus pour stopper le relai //
float EnergyJ = 0; // énergie sauvées le jour J et remise à zéro tous les jours //
float EnergyInit;
int Start = 1; // variable de démarrage du programme //
byte ecran = 0; // variable affichage kwh sauvés jour / total ///


///  configuration wifi ///

AsyncWebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
ESPDash dashboard(&server);
Card button(&dashboard, BUTTON_CARD, "Auto");
Card slider1(&dashboard, SLIDER_CARD, "Puissance sortie 1 en %", "", 0, 95);
Card slider2(&dashboard, SLIDER_CARD, "Puissance sortie 2 en %", "", 0, 95);
Card consommationsurplus(&dashboard, GENERIC_CARD, "surplus / consommation", "watts");
Card puissance(&dashboard, GENERIC_CARD, "puissance envoyée au ballon", "watts");
Card energy1(&dashboard, GENERIC_CARD, "énergie sauvée totale", "kwh");
Card energyj(&dashboard, GENERIC_CARD, "énergie sauvée aujourd'hui", "kwh");
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
  client.setServer(mqtt_server, 1883);
  delay(100);
  temps.begin(); //Intialisation du client NTP
  

  //Code pour créer un Task Core 0//
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //Code pour créer un Task Core 1//
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
  delay(500); 
          
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(probe_name, mqtt_user, mqtt_password, will_topic, will_qos, will_retain, will_msg)) 
      {
        Serial.println("MQTT OK");
        //client.publish(will_topic, hello_msg);
        break;
      } 
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 0.2 seconds");
    delay(200);
  }
}

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

//programme utilisant le Core 1 de l'ESP32//

void Task1code( void * pvParameters )
{
for(;;) {


///////////////////////////////////////////////////////////////////////////

if ( Auto == 0)
    { 
      Datas ();
      valDim1 = Value1;
      dimmer1.setState(ON);
      delay(100);
      dimmer1.setPower(Value1);
      valDim2 = Value2;
      dimmer2.setState(ON);
      delay(100);
      dimmer2.setPower(Value2);
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

void Task2code( void * pvParameters ){

for(;;){

 
 
///  affichage heure ///

  //Update de l'heure  
  temps.update();
//L'heure est envoyée sur le port serie au format 00:00:00 en 1 fois 
  Serial.println(temps.getFormattedTime());
//L'heure est envoyée en 3 fois (heure puis minutes puis secondes)
//  Serial.print(temps.getHours());
//  Serial.print(":");
//  Serial.print(temps.getMinutes());
//  Serial.print(":");
//  Serial.println(temps.getSeconds());

//// reboot EnergyJ tous les jours à 00h00mn00 ////

  if (temps.getHours() == 23 & temps.getMinutes() == 59 & temps.getSeconds() == 59)
      {
        EnergyJ = 0;
        Start = 1;
      }

// boutons page web //

  button.attachCallback([&](bool value){
  Auto = value;
  button.update(Auto);
  dashboard.sendUpdates();
  });

  slider1.attachCallback([&](int value){
  Value1 = value;  
  slider1.update(value);
  dashboard.sendUpdates();
  });

  slider2.attachCallback([&](int value){
  Value2 = value;  
  slider2.update(value);
  dashboard.sendUpdates();
  });

// Mosquitto //

client.loop();
  if (!client.connected() & mqtt == 1) 
    {
      reconnect();
    }
  jsontomqtt = "{\"surplus\": " + String(Power1) +" , \"energie\": " + String(Energy1) +" , \"energytoday\": " + String(EnergyJ) +"}";
  client.publish(topic, (char*) jsontomqtt.c_str(), true);
  

// affichage page web //

        consommationsurplus.update(ajustePuissance);
        puissance.update(Power1);
        energy1.update(Energy1);
        energyj.update(EnergyJ);
        valdim1.update(valDim1);
        valdim2.update(valDim2);
        button.update(Auto);
  
        dashboard.sendUpdates();
        //delay(50);

// affichage écran //

        u8g2.clearBuffer(); // on efface ce qui se trouve déjà dans le buffer
               
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(25, 10); // position du début du texte
        u8g2.print("Le Profes'S"); // écriture de texte
        u8g2.setFont(u8g2_font_unifont_t_symbols);
        u8g2.drawGlyph(70, 13, 0x2600);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(81, 10); // position du début du texte
        u8g2.print("laire  v6.31"); // écriture de texte
        
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(40, 30);
        u8g2.print(Power1);  
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(110, 30);
        u8g2.print("W"); // écriture de texte
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(10, 47);
        u8g2.print(ajustePuissance); // injection ou surplus //
        u8g2.setCursor(10,64);
        u8g2.print("Sauvés : "); // écriture de texte
        

        u8g2.setCursor(115, 64);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.print("KWH"); // écriture de texte
        u8g2.drawRFrame(30,15,70,22,7);
        u8g2.setCursor(10, 47);
        u8g2.setCursor(75, 47);
        u8g2.print(WiFi.localIP());
        ecran = ecran + 1;

/// alternance kwh sauvés par jour vs total /////

if (ecran == 1 || ecran == 2 || ecran == 3 || ecran == 4)
  {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10,64);
    u8g2.print("Sauvés T : "); // écriture de texte
    u8g2.setCursor(85, 64);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(Energy1); // écriture de texte
    delay (500);
  }
if (ecran == 5 || ecran == 6 || ecran == 7 )
  {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10,64);
    u8g2.print("Sauvés J : "); // écriture de texte
    u8g2.setCursor(85, 64);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(EnergyJ); // écriture de texte
    delay (500);
  }

if (ecran == 8 )
  {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10,64);
    u8g2.print("Sauvés J : "); // écriture de texte
    u8g2.setCursor(85, 64);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(EnergyJ); // écriture de texte
    delay (500);
    ecran = 0;
  }

///////////////////////////////////////////////////

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


///  initialisation énergie du jour ////

 if ( Start == 1 )
      {
        Datas();
        EnergyInit = Energy1;
        Start = 0;
      }

    EnergyJ = Energy1 - EnergyInit;
    Serial.println(EnergyInit);
    Serial.println(EnergyJ);
/////////////////////////////////////////


}
}

  
void loop() {
       
                   
}
