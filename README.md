# solar-router
ESP32 code for a diy Solar Energy Router to manage overproduction

Based on the code from [Le Profes'Solaire](https://sites.google.com/view/le-professolaire/routeur-professolaire).

My goal is to add support of MQTT to connect it to Home Assistant.

## Debug tools
### Modbus

[ModbusMechanic](https://github.com/SciFiDryer/ModbusMechanic) is a Java multi-platform tool that allow to communicate with ModBus.

Here is the config to talk directly with the JSY-MK-194 :

![image](https://user-images.githubusercontent.com/6603048/235350896-8841b012-caef-417c-914a-896d1a3f1642.png)
