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