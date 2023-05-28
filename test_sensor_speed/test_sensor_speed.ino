//  mise à jour de la vitesse du port TTL du capteur JSY - 194 de 9600 à 38400 //
//  * ESP32 + JSY-MK-194 (16 et 17)

#include <HardwareSerial.h>

// JSY-MK-194 serial speed
#define MK194_FACTORY_SPEED 4800
#define MK194_SPEED 38400

#define RXD2 16       // For ESP32 - ESP8266 RXD2 = GPIO 13
#define TXD2 17       // For ESP32 -ESP8266 = GPIO 15
byte SensorData[61];  // Message received

long baudRates[] = { 4800, 9600, 19200, 38400, 57600, 115200 };  // array of baud rates to test
int numBaudRates = 6;                                      // number of baud rates to test


void setup() {
  Serial.begin(115200);
  // Serial2.begin(MK194_SPEED, SERIAL_8N1, RXD2, TXD2);

  // long holdingRegisterRead(int id, int address);
  //result = master.readHoldingRegisters(0x01, 0x00);

  if (ReadPowerMeterSensor()) {
    for (int i = 0; i < sizeof(SensorData); i++) {
      Serial.print(SensorData[i], HEX);
      Serial.print("-");
    }
  }
  Serial2.end();
  Serial2.begin(MK194_FACTORY_SPEED, SERIAL_8N1, RXD2, TXD2);  //PORT DE CONNEXION AVEC LE CAPTEUR JSY-MK-194
  /*
  The low 4 bits of low byte are baud rate,
  3-1200bps,4-2400bps, 5-4800bps,
  6-9600bps,7-19200bps,8-38400bps
  */
  // byte msg[] = { 0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x08, 0xA7, 0x82 };  // Switch to 38 400 bps

  byte msg[] = { 0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x05, 0x66, 0x47 };  // Switch to 4800 bps
  // 00 10 00 04 00 01 02 01 06 2B D6 pour passer à 9600
  // 00 10 00 04 00 01 02 01 08 AA 12 pour passer à 38400

  //  envoie la requête pour passer le port de MK194_FACTORY_SPEED à MK194_SPEED //
  for (int i = 0; i < sizeof(msg); i++) {
    Serial2.write(msg[i]);
    delay(60);
  }
  while (Serial2.available()) {
    Serial.println(Serial2.read());
  }
  Serial.println("Speed changed");
}


void loop() {
  for (int i = 0; i < numBaudRates; i++) {
    delay(3000);
    Serial.print("Testing baud rate ");
    Serial.print(baudRates[i]);
    Serial.println("...");
    Serial2.begin(baudRates[i], SERIAL_8N1, RXD2, TXD2);  // set the baud rate
    delay(100);                                           // wait for the sensor to stabilize
    if (ReadPowerMeterSensor()) {
      set_sensor_speed(); // XXXXXXXXXXXXX REMOVE
      Serial.print("Sensor is working at speed ");
      Serial.println(baudRates[i]);
      for (int i = 0; i < sizeof(SensorData); i++) {
        Serial.print(SensorData[i], HEX);
        Serial.print("-");
      }
      Serial.println("");
      if(baudRates[i] != 38400){
        set_sensor_speed();
      }
    }
    Serial.println("end Serial");  // add a blank line for readability
    Serial2.end();
  }
}


void set_sensor_speed(){
  delay(500);
  // 0x08 => 38 400 bps
  //byte msg[] = { 0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x08, 0xA7, 0x82 };  // Switch to 38 400 bps (MK194_SPEED)
  // 0x05 => 4800bps
  byte msg[] = { 0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x05, 0x66, 0x47 };  // Switch to 4800 bps
  for (int i = 0; i < sizeof(msg); i++) {
    Serial2.write(msg[i]);
    delay(60);
  }
  Serial.println("Speed changed !");
  Serial2.end();
  Serial2.begin(MK194_SPEED, SERIAL_8N1, RXD2, TXD2);
}

bool ReadPowerMeterSensor() {
  // Modbus RTU message to get the data

  // Reset data
  for (int i = 0; i < sizeof(SensorData); i++) {
    SensorData[i] = 0x00;
  }

  byte msg[] = { 0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18 };

  int len = sizeof(msg);
  Serial2.flush();
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

  return true;
}
