// owner-4.ino
// UART between owner and servant 
//

//ToF from [tof-3.ino] 
//  void ToF_init();
//  bool ToF_read(uint16_t d[3]);

//servant recieves commands as strings like: F70, L50, R50

//接线：
// tof 3个：
//        F: 前（粉色）；
//        L1: 左前（绿色）
//        L2: 左后（蓝色）
//        左右不要装反掉

//owner的18接servant的17；owner的17接servant的18
//默认两个大轮子朝前


#include <HardwareSerial.h>

// ToF function prototypes (tof-3.ino)
void ToF_init();
bool ToF_read(uint16_t d[3]);

// Wall-following behavior (behavior-wall.ino)
String decideWallFollowing(uint16_t F, uint16_t R1, uint16_t R2);


//Owner: init UART1 on GPIO18 (RX) and GPIO17 (TX)
// 接的时候千万要owner的18接servant的17；owner的17接servant的18！！！！
HardwareSerial ServantSerial(1);

//TOF reading: d[0]=Fromt, d[1]=L1, d[2]=L2
uint16_t tofDist[3];

//sebnd command to servant
void sendToServant(const String &cmd) {
  ServantSerial.println(cmd);

  //Serial.print("[TX to servant] ");
  Serial.println(cmd);
}



void setup() {
  // USB serial
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("===== OWNER BOARD (ToF + UART to servant) =====");

  // init TOF (tof-3.ino)
  ToF_init();

  // Servant: init UART1 on GPIO18 (RX) and GPIO17 (TX)
  // 接的时候千万要owner的18接servant的17；owner的17接servant的18！
  ServantSerial.begin(115200, SERIAL_8N1, 18, 17);
  Serial.println("UART to servant ready");
}

void loop() {
  //read tof distance
  if (ToF_read(tofDist)) {
    //tofDist[0] = 前
    //tofDist[1] = 左前
    //tofDist[2] = 左后
    uint16_t F  = tofDist[0]; //粉色
    uint16_t R1 = tofDist[1]; //绿色
    uint16_t R2 = tofDist[2]; //蓝色

    // decide behavior, returns command string
    String cmd = decideWallFollowing(F, R1, R2);

    // send commands to servant
    sendToServant(cmd);

  } else {
    //if sensor timeout，stop car
    sendToServant("S");
  }


  delay(50);
}

