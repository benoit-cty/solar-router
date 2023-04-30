"""
Debug code to communicate with the sensor.
You need to connect it with a USB to Serial converter.
This script find the speed automaticly and change it to 38 400 bps.
"""
import serial

SERIAL_PORT = '/dev/ttyUSB0'

speeds = [4800,9600,19200, 38400]
default_speed = 4800
for speed in speeds:
    print(f"Trying at {speed} bps")
    ser = serial.Serial(SERIAL_PORT, speed, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE, timeout=1)
    device_address = [0x01]
    function_code = [0x03] # 0x03 : Read Multiple Holding Registers , 0x10 Write
    # data_segment = [ 0x00, 0x48, 0x00, 0x0E ] # Read 14 (0x0E) registers starting from 0x00 0x48
    # crc_checksum = [0x44, 0x18 ]
    # Ask for "Model 1", the answer must be 0x
    data_segment = [ 0x00, 0x00, 0x00, 0x02 ] # Read 2 registers starting from 0x00
    crc_checksum = [0xC4, 0x0B ]
    values = bytearray( device_address + function_code + data_segment + crc_checksum )
    print("\tWriting...")
    ser.write(values)
    print("\tReading...")
    answer = ser.read(500)
    if answer == b'':
        print("\tNo answer !")
    else:
        print("\tResponse length: ",len(answer))
        # 1 address + 1 fonction + 1 longueur + 14 * 4 + 2crc = 61
        if len(answer) == 9 and answer == b'\x01\x03\x04\x01\x94\x01\x14\xbb\xbc':
            # TODO: Expected answeer 0x01 0x03 0x38 ....
            print(f"😋 OK, Found a JSY sensor model 0x01 0x94 running at {speed} bps.")
            default_speed = speed
            print("\tResponse: ",answer)
            break
        else:
            print("\tUnknown response: ",answer)

try:
    import minimalmodbus
except Exception as e:
    print("You need to install minimalmodbus", e)
    exit(1)


instrument = minimalmodbus.Instrument(SERIAL_PORT, 1, debug=True)  # port name, slave address (in decimal)
instrument.serial.baudrate = default_speed
# Default value 0.05 is too small for 4 800bps !
instrument.serial.timeout=1
print(instrument)

# Read model
model = instrument.read_registers(0x00, 1)  # Registernumber, number of decimals
print("💡Model:", hex(model[0]))
if hex(model[0]) != "0x194":
    print(f"ERROR, model found : {hex(model[0])}, was expecting 0x194 !")
    exit(2)
# Read speed
config = instrument.read_registers(0x04, 1)  # Registernumber, number of decimals
print("💡config=",config, hex(config[0]))
# Default [261] 0x105 => 4800bps
if config[0] == 261:
    # # Change Speed
    bps = 0x08 # The low 4 bits of low byte are baud rate 8=38400
    instrument.write_registers(0x04, [ 0x0108 ])
    """
    MinimalModbus debug mode. Will write to instrument (expecting 8 bytes back): 01 10 00 04 00 01 02 01 08 A7 82 (11 bytes)
    MinimalModbus debug mode. Clearing serial buffers for port /dev/ttyUSB0
    MinimalModbus debug mode. Sleeping 7.53 ms before sending. Minimum silent period: 8.02 ms, time since read: 0.49 ms.
    MinimalModbus debug mode. Response from instrument: 01 10 00 04 00 01 40 08 (8 bytes), roundtrip time: 0.1 ms. Timeout for reading: 0.0 ms.
    """
## Read all
measures = instrument.read_registers(0x48, 14)  # Registernumber, number of decimals
print("💡 Data", measures)
