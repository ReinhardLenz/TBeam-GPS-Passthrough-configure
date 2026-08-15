
## T-beam GPS communication

the big problem with a T-Beam is, that there is no "direct access" from the computer to the UBLOX GPS NEO M8N chip. But when I want to define and go deeper into the communication between ESP32 and UBLOX GPS NEO M8N chip , I need to use PYGPSClient or the U-Center analyzing program. Normally, with this PYGPSClient, I need to directly connect the computer to UBLOX GPS NEO M8N chip. But in T-Beam (which is mostly made for Meshtastic) I cannot do it. Here, I need a "Passthrough" program, so the everything transmitted from UBLOX GPS NEO M8N to ESP32, is directly forwarded to computer where the signal can be analyzed with PYGPSClient program. But there is a complication: the configuration signals from PYGPSClient to UBLOX GPS NEO M8N  cannot be forwarded by this program. So, anything to do with configuration must be done in the setup part of the program. This program is a "crutch"  or some kind of "walking aid" for the PYGPSClient program.


The program is installed on a T-Beam (a small box containing an ESP32, a NEO-M8N GPS receiver, and an AXP2101 power management chip). Its purpose is twofold:

---

## What it does
To configure the GPS by sending it special commands (USBX protocol) to disable the NMEA messages it sends by default and keep only a single useful message (position, speed, altitude, etc.).

To act as a bridge between your computer (connected via USB) and the GPS: everything you type on the computer's serial port is transmitted to the GPS, and everything the GPS sends back is displayed on your screen.

### 1. At startup (setup)
Power supply check:
The program attempts to communicate with the AXP2101 chip (which manages the voltages). If it finds a signal, it activates the output that powers the GPS (3.3V). Otherwise, it displays a warning.

Initializing the GPS serial port:
This opens a 9600 baud UART connection between the ESP32 and the GPS (pins 34 for receiving, 12 for transmitting).

Configuring the GPS (the most important part):
By default, the GPS sends many NMEA sentences (GGA, GSA, RMC, etc.). The program disables them all one by one by sending UBX commands.

Then it activates a single UBX message called NAV-PVT (which contains the position, speed, time, altitude, etc.).
For each command, it waits for the GPS acknowledgment response (ACK or NAK) and displays whether it was successful.

Displaying a brief summary:
After all these commands, it reads what the GPS sent in the meantime and displays it in hexadecimal on the USB port.

### 2. Looping
The program simply shuttles between the following:

GPS Serial → USB: Everything the GPS sends is immediately copied to your computer (serial port).
USB Serial → GPS: Everything you type on the serial monitor is sent directly to the GPS.

This allows you to communicate directly with the GPS while the program is running.

### 3. Hidden Functions

flushGpsInput: Clears the GPS's input buffer for a certain period (to avoid reading old messages before a command).

sendUBX: Sends a UBX packet (binary format) to the GPS.

ubxChecksum: Calculates the checksum that validates the UBX packet.

waitForAck: Waits for the GPS's response to a command (either "OK", "rejected", or "timeout").
axpWrite / axpRead: Controls the power chip via I2C.

enableGPSPower: Turns on the GPS power supply (3.3V) by writing to the AXP2101 registers.

Visual summary

Computer (USB) ←→ ESP32 (bridge) ←→ NEO-M8N GPS (UART)
Initial configuration: Disables NMEA, enables UBX-NAV-PVT
Once configured, the GPS only sends the UBX-NAV-PVT message (full navigation data), and the program forwards it to the computer.

In short: This program turns on the GPS, configures it to communicate only in UBX mode (a single type of message), and then acts as a transparent cable between the computer and the GPS, allowing you to read the data or send commands manually.



## Hardware / Components Used

### Boards
- **2× LILYGO T-Beam V1.2**
  - MCU: **ESP32**
  - LoRa radio: **SX1262**
  - GPS: **NEO-M8N**
  - PMU: **AXP2101**
  - USB-UART: **CH9102**
  - Flash: 4MB, PSRAM: 8MB
  - Marking: *LILYGO 868/915 MHz Model: LORA32 SX1262*

## Dependencies / Libraries Used
 - Arduino framework (ESP32)
 - Wire

## Prerequisites
- Install VS Code
- ✅ Install the PlatformIO extension
- Connect your T-Beam via USB (CH9102 driver may be required depending on your OS)
- Compile & Upload
   
- Open the sender project and run:
- Build
- Upload
- Monitor (Serial Monitor at 115200 baud)
-Repeat for the receiver project.

-Serial Monitor Settings
-Baud rate: 115200

## Acknowledgements
-	iforce2d

## Link

https://wiki.paparazziuav.org/wiki/Sensors/GPS

## License
-	This project is licensed under the MIT License. See the LICENSE file for details.

## Images
1. 
![Diagram](images/photo.jpg)
2. 
![Diagram](images/dimensions.jpg)
3.

![Diagram](images/el-pin-meanings.jpg)