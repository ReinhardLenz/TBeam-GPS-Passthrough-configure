#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA       21
#define I2C_SCL       22

#define GPS_RX_PIN 34 // Connected to NEO-M8N TX
#define GPS_TX_PIN 12 // Connected to NEO-M8N RX


#define AXP2101_ADDR  0x34

#define AXP2101_DLDO1_VOL  0x99
#define AXP2101_DLDO_EN    0x9C

#define USB_BAUD  115200


HardwareSerial GPSSerial(1);


void sendUBX(const uint8_t *msg, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) GPSSerial.write(msg[i]);
  GPSSerial.flush();
}

// UBX checksum (8-bit Fletcher)
void ubxChecksum(const uint8_t *payload, uint16_t len, uint8_t &ckA, uint8_t &ckB) {
  ckA = 0; ckB = 0;
  for (uint16_t i = 0; i < len; i++) {
    ckA = ckA + payload[i];
    ckB = ckB + ckA;
  }
}


void enableNMEAonUART1(uint8_t nmeaMsgId, uint8_t rate) {
  // UBX-CFG-MSG payload (8 bytes): msgClass, msgID, rateI2C, rateUART1, rateUART2, rateUSB, rateSPI, reserved
  uint8_t payload[8] = {0xF0, nmeaMsgId, 0, rate, 0, 0, 0, 0};

  uint8_t ckA, ckB;
  ubxChecksum(payload, sizeof(payload), ckA, ckB);

  uint8_t msg[16]; // 2 sync + 2 class/id + 2 len + 8 payload + 2 cksum = 16
  msg[0] = 0xB5; msg[1] = 0x62;
  msg[2] = 0x06; msg[3] = 0x01;       // CFG-MSG
  msg[4] = 0x08; msg[5] = 0x00;       // length = 8
  memcpy(&msg[6], payload, 8);
  msg[14] = ckA; msg[15] = ckB;

  sendUBX(msg, sizeof(msg));
}


// Send UBX-CFG-MSG to set rate for a given message on UART1
// For NMEA messages on u-blox M8: msgClass = 0xF0, msgID selects sentence (e.g. GSV)
void setNMEAMessageRate_UART1(uint8_t nmeaMsgId, uint8_t rate) {
  // Payload for UBX-CFG-MSG (M8 variant with per-port rates) is commonly 8 bytes:
  // [msgClass, msgID, rateI2C, rateUART1, rateUART2, rateUSB, rateSPI, rateReserved]
  uint8_t payload[8] = {0xF0, nmeaMsgId, 0, rate, 0, 0, 0, 0};

  uint8_t ckA, ckB;
  ubxChecksum(payload, sizeof(payload), ckA, ckB);

  uint8_t msg[2 + 2 + 2 + sizeof(payload) + 2];
  // Sync chars
  msg[0] = 0xB5; msg[1] = 0x62;
  // Class, ID
  msg[2] = 0x06; msg[3] = 0x01; // CFG-MSG
  // Length (little endian)
  msg[4] = sizeof(payload) & 0xFF;
  msg[5] = (sizeof(payload) >> 8) & 0xFF;
  // Payload
  memcpy(&msg[6], payload, sizeof(payload));
  // Checksum
  msg[6 + sizeof(payload)] = ckA;
  msg[7 + sizeof(payload)] = ckB;

  sendUBX(msg, sizeof(msg));
}


// ------------------------------------------------------------
// AXP2101 helpers
// ------------------------------------------------------------

void axpWrite(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(AXP2101_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}


uint8_t axpRead(uint8_t reg)
{
    Wire.beginTransmission(AXP2101_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);

    Wire.requestFrom((uint8_t)AXP2101_ADDR, (uint8_t)1);

    return Wire.available() ? Wire.read() : 0xFF;
}


void enableGPSPower()
{
    // DLDO1 = 3.3 V
    axpWrite(AXP2101_DLDO1_VOL, 0x1C);

    delay(20);

    // Enable DLDO1, preserving the other bits
    uint8_t reg = axpRead(AXP2101_DLDO_EN);

    axpWrite(AXP2101_DLDO_EN, reg | 0x01);

    delay(200);
}


void setup() {
  // Serial to PC (USB)


  Serial.begin(USB_BAUD);

  Wire.begin(I2C_SDA, I2C_SCL);

  delay(50);

  Wire.beginTransmission(AXP2101_ADDR);

  if (Wire.endTransmission() == 0) {

      Serial.println();
      Serial.println(
          "AXP2101 detected -> enabling GPS power..."
      );

      enableGPSPower();

      Serial.println("GPS power enabled.");

  } else {

      Serial.println();
      Serial.println(
          "WARNING: AXP2101 not detected at 0x34."
      );

      Serial.println(
          "GPS may be unpowered."
      );
  }
  
  // Serial to GPS module (NEO-M8N default is usually 9600 baud)
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); 
  delay(200);
  // Enable NMEA GSV on UART1 at rate 1 (once per navigation solution)
  // NMEA IDs in u-blox M8: GGA=0x00, GLL=0x01, GSA=0x02, GSV=0x03, RMC=0x04, VTG=0x05, etc.
  // Enable GSV + GSA on UART1

  enableNMEAonUART1(0x03, 1); // GSV
  enableNMEAonUART1(0x02, 1); // GSA (recommended for satellite status/DOP)

  // Optional: if you want to reduce clutter, you can disable others by setting rate=0
  // enableNMEAonUART1(0x00, 1); // GGA keep
  // enableNMEAonUART1(0x04, 1); // RMC keep


  //  setNMEAMessageRate_UART1(0x03, 1);

  // (Optional) also enable GSA if you want DOP/active satellites
//  setNMEAMessageRate_UART1(0x02, 1);


}

void loop() {
  // Pass data from GPS -> PC
  while (GPSSerial.available()) {
    Serial.write(GPSSerial.read());
  }
  
  // Pass commands from PC (u-center) -> GPS
  while (Serial.available()) {
    GPSSerial.write(Serial.read());
  }
}