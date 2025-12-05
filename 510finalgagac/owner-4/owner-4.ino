// owner-4.ino
// UART between owner and servant 
// Right Wall Following with Auto Switch

#include <HardwareSerial.h>

// ToF function prototypes
void ToF_init();
bool ToF_read(uint16_t d[3]);

// Wall-following behavior
String decideWallFollowing(uint16_t F, uint16_t R1, uint16_t R2);

HardwareSerial ServantSerial(1);
uint16_t tofDist[3];

// [新增] 自动模式开关，默认关闭
bool isAutoRunning = false;

void sendToServant(const String &cmd) {
  ServantSerial.println(cmd);
  Serial.println(cmd);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===== OWNER BOARD (Right Wall Logic) =====");
  
  ToF_init();

  // Owner RX=18, TX=17
  ServantSerial.begin(115200, SERIAL_8N1, 18, 17);
  Serial.println("UART to servant ready. Waiting for Start...");
}

void loop() {
  // 1. check web controller 发过来的指令
  if (ServantSerial.available()) {
    String webCmd = ServantSerial.readStringUntil('\n');
    webCmd.trim();

    if (webCmd == "AUTO_ON") {
      isAutoRunning = true;
      Serial.println(">>> AUTO MODE STARTED <<<");
    } 
    else if (webCmd == "AUTO_OFF") {
      isAutoRunning = false;
      sendToServant("S"); // 立刻停车
      Serial.println(">>> AUTO MODE STOPPED <<<");
    }
  }

  // 2. auto mode 开了才跑巡墙
  if (isAutoRunning) {
    if (ToF_read(tofDist)) {
      uint16_t F  = tofDist[0];
      uint16_t R1 = tofDist[1];
      uint16_t R2 = tofDist[2];

      String cmd = decideWallFollowing(F, R1, R2);
      sendToServant(cmd);
    } else {
       sendToServant("S");
    }
  } 
  
  delay(50);
}