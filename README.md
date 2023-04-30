# solar-router
ESP32 code for a diy Solar Energy Router to manage overproduction

Based on the code from [Le Profes'Solaire](https://sites.google.com/view/le-professolaire/routeur-professolaire).

My goal is to add support of MQTT to connect it to Home Assistant.

## Debug tools
### Modbus

The ESP32 send the message _0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18_, which mean:

| Adress | Function | Address of first register to read (16-bit) | Number of registers to read (16-bit) | Cyclic Redundancy Check (CRC)  |
|---|---|---|---|---|
| 1 | 3 (Read) | 0x48 (72) | 0x0E (14) | 0x44, 0x18 |

[ModbusMechanic](https://github.com/SciFiDryer/ModbusMechanic) is a Java multi-platform tool that allow to communicate with ModBus.

Here is the config to talk directly with the JSY-MK-194 :

![image](https://user-images.githubusercontent.com/6603048/235350896-8841b012-caef-417c-914a-896d1a3f1642.png)

But it does not work with the message to read power measures. It is not a problem as it will works in the pv router.
