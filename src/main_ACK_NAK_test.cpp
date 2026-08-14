#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA       21
#define I2C_SCL       22

// Adjust to your pins / serial instance
HardwareSerial GPSSerial(1);
static const int GPS_RX_PIN = 34;   // GPS TX -> MCU RX
static const int GPS_TX_PIN = 12;   // GPS RX -> MCU TX

#define AXP2101_ADDR  0x34

#define AXP2101_DLDO1_VOL  0x99
#define AXP2101_DLDO_EN    0x9C

#define USB_BAUD  115200

// ---------- helpers ----------
void flushGpsInput(uint32_t ms)
{
  uint32_t start = millis();
  while (millis() - start < ms) {
    while (GPSSerial.available()) (void)GPSSerial.read();
    delay(1);
  }
}

void sendUBX(const uint8_t* msg, uint16_t len)
{
  GPSSerial.write(msg, len);
  GPSSerial.flush(); // ensure bytes are pushed out (ok on ESP32)
}

// UBX checksum over buffer bytes
void ubxChecksum(const uint8_t* data, uint16_t len, uint8_t &ckA, uint8_t &ckB)
{
  ckA = 0; ckB = 0;
  for (uint16_t i = 0; i < len; i++) {
    ckA = ckA + data[i];
    ckB = ckB + ckA;
  }
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



// Wait for UBX-ACK-ACK or UBX-ACK-NAK for a given (cls,id)
enum AckResult { ACK_OK, ACK_NAK, ACK_TIMEOUT };

AckResult waitForAck(uint8_t cls, uint8_t id, uint32_t timeoutMs)
{
  // We expect either:
  // B5 62 05 01 02 00 cls id ckA ckB  (ACK-ACK)
  // B5 62 05 00 02 00 cls id ckA ckB  (ACK-NAK)

  uint8_t buf[10];
  uint8_t idx = 0;
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (GPSSerial.available()) {
      uint8_t b = GPSSerial.read();

      // simple sync/state machine for 10-byte ACK frames
      if (idx == 0 && b != 0xB5) continue;
      if (idx == 1 && b != 0x62) { idx = 0; continue; }

      buf[idx++] = b;

      if (idx == 10) {
        idx = 0;

        // Validate header/class
        if (buf[0] != 0xB5 || buf[1] != 0x62 || buf[2] != 0x05) continue;
        if (!((buf[3] == 0x01) || (buf[3] == 0x00))) continue; // ACK or NAK
        if (buf[4] != 0x02 || buf[5] != 0x00) continue;        // length 2

        // Verify checksum over bytes 2..7 (class..payload)
        uint8_t ckA, ckB;
        ubxChecksum(&buf[2], 6, ckA, ckB);
        if (ckA != buf[8] || ckB != buf[9]) continue;

        // Check it matches the message we care about
        if (buf[6] == cls && buf[7] == id) {
          return (buf[3] == 0x01) ? ACK_OK : ACK_NAK;
        }
      }
    }
    delay(1);
  }
  return ACK_TIMEOUT;
}

// ---------- your function, corrected ----------
void sendUBX_CFG_MSG(uint8_t targetMsgClass, uint8_t targetMsgId, uint8_t rateUART1)
{
  // Payload (8 bytes)
  uint8_t payload[8] = {
    targetMsgClass, targetMsgId,
    0,              // rateI2C
    rateUART1,      // rateUART1
    0,              // rateUART2
    0,              // rateUSB
    0,              // rateSPI
    0               // reserved
  };

  // Build full UBX frame: sync(2) + class+id(2) + len(2) + payload(8) + ck(2) = 16
  uint8_t msg[16];
  msg[0] = 0xB5; msg[1] = 0x62;
  msg[2] = 0x06; msg[3] = 0x01;     // CFG-MSG
  msg[4] = 0x08; msg[5] = 0x00;     // length = 8
  memcpy(&msg[6], payload, 8);

  // ✅ checksum must cover msg[2]..msg[13] (class,id,len,payload) = 12 bytes
  uint8_t ckA, ckB;
  ubxChecksum(&msg[2], 12, ckA, ckB);
  msg[14] = ckA; msg[15] = ckB;

  sendUBX(msg, sizeof(msg));
}

// ---------- minimal test ----------
void setup()
{
  Serial.begin(115200);

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
  
  delay(1000);

  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(200);

  Serial.println("Starting minimal UBX ACK test...");



  flushGpsInput(200);
  sendUBX_CFG_MSG(0xF0, 0x04, 0); // RMC off
  AckResult r = waitForAck(0x06, 0x01, 4000);
  if (r == ACK_OK) Serial.println("✅ Got ACK-ACK for CFG-MSG");
  else if (r == ACK_NAK) Serial.println("❌ Got ACK-NAK for CFG-MSG");
  else Serial.println("⚠️ ACK TIMEOUT (no valid ACK/NAK seen)");





  flushGpsInput(200);

    // Enable UBX-NAV-SAT (01 35) on UART1, rate = 1
  sendUBX_CFG_MSG(0x01, 0x35, 1);

  // ACK is for CFG-MSG (06 01)
  AckResult r2 = waitForAck(0x06, 0x01, 1500);
  if (r2 == ACK_OK) Serial.println("✅ Got ACK-ACK enabling NAV-SAT");
  else if (r2 == ACK_NAK) Serial.println("❌ Got ACK-NAK enabling NAV-SAT");
  else Serial.println("⚠️ ACK TIMEOUT enabling NAV-SAT");
  flushGpsInput(200);
  // Disable all common NMEA messages on UART1
  sendUBX_CFG_MSG(0xF0, 0x00, 0); // GGA
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);
  sendUBX_CFG_MSG(0xF0, 0x01, 0); // GLL
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);
  sendUBX_CFG_MSG(0xF0, 0x02, 0); // GSA
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);
  sendUBX_CFG_MSG(0xF0, 0x03, 0); // GSV
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);
  sendUBX_CFG_MSG(0xF0, 0x04, 0); // RMC
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);
  sendUBX_CFG_MSG(0xF0, 0x05, 0); // VTG
  waitForAck(0x06, 0x01, 2000);
  flushGpsInput(200);

while (GPSSerial.available()) {
  uint8_t b = GPSSerial.read();
  if (b < 16) Serial.print('0');
  Serial.print(b, HEX);
  Serial.print(' ');
}
}

void loop() {}