//  mise à jour de la vitesse du port TTL du capteur JSY - 194 de 9600 à 38400 //
//  * ESP32 + JSY-MK-194 (16 et 17)

#include <HardwareSerial.h>

#define MK194_FACTORY_SPEED 4800
#define MK194_SPEED 38400
#define RXD2 16 // For ESP32 - ESP8266 RXD2 = GPIO 13
#define TXD2 17 // For ESP32 -ESP8266 = GPIO 15

void setup() {
  Serial.begin(115200);
  Serial2.begin(MK194_FACTORY_SPEED, SERIAL_8N1, RXD2, TXD2); //PORT DE CONNEXION AVEC LE CAPTEUR JSY-MK-194
}

int len=11;
void loop() {

  byte msg[] = {0x00,0x10,0x00,0x04,0x00,0x01,0x02,0x01,0x08,0xA7,0x82}; // passage du port TTL en 38400
  //byte msg[] = { 0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x05, 0x66, 0x47 };  // Switch to 4800 bps
  // 00 10 00 04 00 01 02 01 06 2B D6 pour passer à 9600
  // 00 10 00 04 00 01 02 01 08 AA 12 pour passer à 38400

   //  envoie la requête pour passer le port de 9600 à 38400 //
   for(int i = 0 ; i < len ; i++)
   {
     Serial2.write(msg[i]);
     delay (60);
   }
   len = 0;
   Serial.println("Speed changed");
}     