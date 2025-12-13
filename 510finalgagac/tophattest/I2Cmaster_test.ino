// ESP32-C3 I2C Master Code
// test code writes and receives
#include <Wire.h>

#define I2C_SLAVE_ADDR 0x28
#define SDA_PIN 5
#define SCL_PIN 4

void send_I2C_byte(uint8_t data) {
  // Send data to slave
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(data);  // Send some test data
  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    Serial.println("Data sent successfully");
    rgbLedWrite(2, 0, 20, 0);  // green
  } else {
    Serial.printf("Error sending data: %d\n", error);
    rgbLedWrite(2, 20, 0, 0);  // red
  }
}

uint8_t receive_I2C_byte() {  // data should have space declared from caller
  // Request data from slave
  uint8_t bytesReceived = Wire.requestFrom(I2C_SLAVE_ADDR, 1);
  uint8_t byteIn = 0;

  if (bytesReceived > 0) {
    Serial.print("Received from slave: ");
    while (Wire.available()) {
      byteIn = Wire.read();
      Serial.printf("0x%02X ", byteIn);
    }
    Serial.println();
  } else return 0;
  return byteIn;  // return number of bytes read
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C master
  Wire.begin(SDA_PIN, SCL_PIN, 40000);
  Serial.println("ESP32-C3 I2C Master initialized");
  Serial.printf("SDA: %d, SCL: %d\n", SDA_PIN, SCL_PIN);
}

void loop() {
  send_I2C_byte(0x55);
  receive_I2C_byte();       // loop back

  delay(500);               // Wait a second before next transmission
  rgbLedWrite(2, 0, 0, 0);  // off
  delay(500);               // Wait a second before next transmission
}