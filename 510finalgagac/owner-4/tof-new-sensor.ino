#include <Arduino.h>
#include <Wire.h>
#include <vl53l4cx_class.h>

//pin
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 9;

constexpr int XSHUT_PIN1 = 10;
constexpr int XSHUT_PIN2 = 11;
constexpr int XSHUT_PIN3 = 12;


uint8_t SENSOR_ADDR[3] = {0x2A << 1, 0x2B << 1, 0x2C << 1};

constexpr uint8_t NUM_SENSORS = 3;

VL53L4CX sensor1(&Wire, XSHUT_PIN1);
VL53L4CX sensor2(&Wire, XSHUT_PIN2);
VL53L4CX sensor3(&Wire, XSHUT_PIN3);

VL53L4CX *sensors[] = {&sensor1, &sensor2, &sensor3};
int xshutPins[] = {XSHUT_PIN1, XSHUT_PIN2, XSHUT_PIN3};

// keeo last distance
uint16_t last_distances[3] = {0, 0, 0};

void ToF_init();
bool ToF_read(uint16_t d[3]);

/*
void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n VL53L4CX SYSTEM START");

  ToF_init();
}

void loop() {
  uint16_t distances[3]; 
  
  // read and print
  if (ToF_read(distances)) {
     
  }

  
  delay(10); 
}
*/

// tof init

void ToF_init() {
  // 1. init I2C 
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); 

  // 2.low
  Serial.println(">>> Resetting sensors...");
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(xshutPins[i], OUTPUT);
    digitalWrite(xshutPins[i], LOW);
  }
  delay(50);

  // 3.start one by one
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    // wake up
    digitalWrite(xshutPins[i], HIGH);
    delay(50); // wait to wake up

    // init and change addy
    int status = sensors[i]->InitSensor(SENSOR_ADDR[i]);

    if (status == 0) {
      Serial.print("Sensor "); Serial.print(i + 1);
      Serial.print(" Init OK (Addr 0x");
      //pint the 7 bits addy
      Serial.print(SENSOR_ADDR[i] >> 1, HEX); 
      Serial.println(")");
      
      sensors[i]->VL53L4CX_StartMeasurement();
    } 
    else {
      Serial.print("Sensor "); Serial.print(i + 1);
      Serial.print(" FAILED error: "); Serial.println(status);
      digitalWrite(xshutPins[i], LOW); //shut if low
    }
    
    delay(20); 
  }
  Serial.println(">>> All Sensors Ready.");
}

//tof reading

bool ToF_read(uint16_t d[3]) {
  VL53L4CX_MultiRangingData_t MultiRangingData;
  uint8_t NewDataReady = 0;
  int status = 0;
  
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    if (digitalRead(xshutPins[i]) == HIGH) {
      status = sensors[i]->VL53L4CX_GetMeasurementDataReady(&NewDataReady);
  
      if (NewDataReady) {
        status = sensors[i]->VL53L4CX_GetMultiRangingData(&MultiRangingData);
        sensors[i]->VL53L4CX_ClearInterruptAndStartMeasurement();
  
        if (status == 0 && MultiRangingData.NumberOfObjectsFound > 0) {
           uint8_t rangeStatus = MultiRangingData.RangeData[0].RangeStatus;
           if (rangeStatus == 0) {
              last_distances[i] = MultiRangingData.RangeData[0].RangeMilliMeter;
           }
        }
      }
    }
  }

  d[0] = last_distances[0];
  d[1] = last_distances[1];
  d[2] = last_distances[2];

  Serial.print("F: ");  Serial.print(d[0]); 
  Serial.print(" mm   R1: "); Serial.print(d[1]); 
  Serial.print(" mm   R2: "); Serial.print(d[2]); Serial.println(" mm");

  return true;
}
