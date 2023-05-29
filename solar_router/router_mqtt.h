#include <ArduinoHA.h>  // Download it from https://github.com/dawidchyrzynski/arduino-home-assistant/releases/tag/2.0.0
// And add PubSubClient (by Nick O'Leary) from library manager
// Add also "ArduinoJson"  from library manager

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
HASensorNumber ha_PowerFactor2("power_factor_2", HASensorNumber::PrecisionP3);
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

unsigned long lastMqttPublishAt = 0;

void setup_mqtt() {
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
}

void mqtt_loop(float Voltage, float Intensite1, float Power1, float Energy1, float Frequency, float PowerFactor2, float Intensite2, float Power2,
               float Energy2, float ajustePuissance, float valDim1, float valDim2, boolean Auto) {
  // Send values to Home Assistant every 10 seconds
  if ((millis() - lastMqttPublishAt) > 10000) {
    ha_voltage.setValue(Voltage);
    ha_current1.setValue(Intensite1);
    ha_power1.setValue(Power1);
    ha_energy1.setValue(Energy1);
    ha_frequency.setValue(Frequency);
    ha_PowerFactor2.setValue(PowerFactor2);
    ha_current2.setValue(Intensite2);
    ha_power2.setValue(Power2);
    ha_energy2.setValue(Energy2);
    ha_energy_saved.setValue(ajustePuissance);
    ha_dimmer1_output.setValue(valDim1);
    ha_dimmer2_output.setValue(valDim2);
    ha_mode.setValue(Auto);
    lastMqttPublishAt = millis();
    mqtt.loop();
  }
}