// tof-3.ino
//void ToF_init();
//bool ToF_read(uint16_t d[3]);

#include <Wire.h>
#include <VL53L1X.h>

//I2C pins
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 9;

//XSHUT pins for 3 sensors
constexpr uint8_t XSHUT_PINS[3]  = {10, 11, 12};
constexpr uint8_t SENSOR_ADDR[3] = {0x2A, 0x2B, 0x2C};

constexpr uint8_t NUM_SENSORS = 3;

VL53L1X sensors[NUM_SENSORS];

// init TOF
void ToF_init() {

  Serial.println("Init 3x VL53L1X (Sensor 1, 2, 3)");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); 

  //lower XSHUT
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    pinMode(XSHUT_PINS[i], OUTPUT);
    digitalWrite(XSHUT_PINS[i], LOW);
  }
  delay(10);

  //init one by one 
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {

    digitalWrite(XSHUT_PINS[i], HIGH);
    delay(10);

    sensors[i].setTimeout(500);
    if (!sensors[i].init()) {
      Serial.print("Init fail on Sensor ");
      Serial.println(i + 1);
      while (true) {
        Serial.println("VL53L1X init failed. HALT.");
        delay(1000);
      }
    }

    sensors[i].setAddress(SENSOR_ADDR[i]);

    sensors[i].setDistanceMode(VL53L1X::Medium);
    sensors[i].setMeasurementTimingBudget(50000);
    sensors[i].startContinuous(50);
  }

  Serial.println("Sensors 1–3 ready.");
}


bool ToF_read(uint16_t d[3]) {

  bool timeout = false;

  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    d[i] = sensors[i].read();
    if (sensors[i].timeoutOccurred()) {
      timeout = true;
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.println(" timeout");
    }
  }

  if (timeout) {
    Serial.println("Timeout on one or more sensors");
    return false;
  }

  // R1, R2
  Serial.print("F: ");  Serial.print(d[0]); Serial.print(" mm   ");
  Serial.print("R1: "); Serial.print(d[1]); Serial.print(" mm   ");
  Serial.print("R2: "); Serial.print(d[2]); Serial.println(" mm");

  return true;
}