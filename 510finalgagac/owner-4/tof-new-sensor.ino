/*
 * tof-new-sensor.ino — Owner 侧 ToF（VL53L4CX）三传感器驱动
 *
 * 本文件主要包含：
 * 1) 三个 VL53L4CX ToF 传感器的初始化与地址分配
 *    - 使用 3 个 XSHUT 引脚（XSHUT_PIN1/2/3）逐个唤醒传感器
 *    - 通过 `InitSensor(SENSOR_ADDR[i])` 为每个传感器设置不同 I2C 地址，避免同址冲突
 *    - 完成后对每个传感器调用 `VL53L4CX_StartMeasurement()` 开始连续测距
 *
 * 2) 统一的 ToF 接口（供 `owner-4.ino` / `behavior-wall.ino` 调用）
 *    - `ToF_init()`：初始化 I2C、复位/唤醒传感器、设置地址并启动测距
 *    - `ToF_read(uint16_t d[3])`：读三路距离（单位 mm），写入 d[0..2]
 *
 * 3) 三路距离的约定（d 数组含义）
 *    - d[0] = F  ：前向 ToF（Front）
 *    - d[1] = R1 ：右前/右侧 ToF（用于巡墙主控）
 *    - d[2] = R2 ：右后/右侧 ToF（用于平行/转角辅助）
 *   （具体物理安装方向以你们车上标号为准；Owner 侧会按 F/R1/R2 使用）
 *
 * 4) 数据有效性与“保持上一帧”的容错
 *    - 本实现用 `last_distances[]` 保存上一帧有效读数：
 *      当本帧 `RangeStatus!=0` 或无新数据时，不更新该路读数，从而减少瞬时跳变对控制的冲击
 *
 * ToF 功能在哪里被用到？
 * - `owner-4.ino`：在 `setup()` 调用 `ToF_init()`；在 `loop()` 中周期 `ToF_read()`，
 *   并把读数传入 `decideWallFollowing(F,R1,R2)` 等行为/避障逻辑。
 */

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
