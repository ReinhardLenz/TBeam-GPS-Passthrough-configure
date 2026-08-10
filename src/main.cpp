#include <Arduino.h>

#define GPS_RX_PIN 34 // Connected to NEO-M8N TX
#define GPS_TX_PIN 12 // Connected to NEO-M8N RX

HardwareSerial GPSSerial(1);

void setup() {
  // Serial to PC (USB)
  Serial.begin(115200); 
  
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