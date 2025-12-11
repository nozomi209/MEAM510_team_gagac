// Owner 板（模式控制 + ToF 巡墙）：接收 Web/Servant 指令，读取 ToF，生成运动命令
// UART between owner and servant 
// Right Wall Following with Auto Switch

#include <HardwareSerial.h>

// ToF function prototypes（在 tof.cpp 中实现）
void ToF_init();
bool ToF_read(uint16_t d[3]);

//~~~~~~~~~~wifi config~~~~~~~~~~~~~~~~
//const char* SSID     = "MoXianBao";
//const char* PASSWORD = "olivedog";

/* ！！！use in lab！！！！！！！
const char* SSID     = "TP-Link_8A8C";
const char* PASSWORD = "12488674";
IPAddress localIP(192, 168, 1, 120);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
*/

//remember to un- comment another line at void setup() if using lab wifi


// Wall-following behavior
String decideWallFollowing(uint16_t F, uint16_t R1, uint16_t R2);

HardwareSerial ServantSerial(1);
uint16_t tofDist[3];

// 自动模式开关，默认关闭
bool isAutoRunning = false;

// ToF 校准（单位: mm），R2 实际测距偏小可在此调正
int16_t TOF_OFFSET_F  = 0;
int16_t TOF_OFFSET_R1 = 0;
int16_t TOF_OFFSET_R2 = 15;   // 如仍偏小，可再增大 (例 20~30)
float   TOF_SCALE_F   = 1.03; // R0/Front 远距离偏小 -> 稍放大
float   TOF_SCALE_R1  = 1.00;
float   TOF_SCALE_R2  = 1.00; // 若需要比例修正，可调至 1.05 等

static uint16_t applyToFCal(uint16_t raw, int16_t offset, float scale = 1.0f) {
  int32_t v = (int32_t)((float)raw * scale) + offset;
  if (v < 0) v = 0;
  return (uint16_t)v;
}

void sendToServant(const String &cmd) {
  ServantSerial.println(cmd);
  Serial.println(cmd);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===== OWNER BOARD (Right Wall Logic) =====");
  
  ToF_init();

  // Owner RX=GPIO18, TX=GPIO17 （与 Servant 交叉连接；Servant TX=17 -> Owner RX=18）
  ServantSerial.begin(115200, SERIAL_8N1, 18, 17);
  Serial.println("UART to servant ready. Waiting for Start...");
}

void loop() {
  static uint32_t lastToFPrint = 0;
  uint16_t tofMon[3];

  // 1. 处理来自 Servant 的 Web/上位机指令
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
    // 处理参数更新命令: PARAM:参数名=值
    else if (webCmd.startsWith("PARAM:")) {
      String paramStr = webCmd.substring(6); // 去掉"PARAM:"
      int eqPos = paramStr.indexOf('=');
      if (eqPos > 0) {
        String paramName = paramStr.substring(0, eqPos);
        float paramValue = paramStr.substring(eqPos + 1).toFloat();
        updateWallParam(paramName, paramValue);
        Serial.printf(">>> 参数更新: %s = %.2f\n", paramName.c_str(), paramValue);
      }
    }
  }

  // 2. auto mode 开了才跑巡墙，并打印 ToF 读数到串口监视器
  if (isAutoRunning) {
    if (ToF_read(tofDist)) {
      uint16_t F  = applyToFCal(tofDist[0], TOF_OFFSET_F, TOF_SCALE_F);
      uint16_t R1 = applyToFCal(tofDist[1], TOF_OFFSET_R1, TOF_SCALE_R1);
      uint16_t R2 = applyToFCal(tofDist[2], TOF_OFFSET_R2, TOF_SCALE_R2);

      // 串口监视器输出 ToF 距离（mm）
      Serial.printf("ToF: F=%u mm, R1=%u mm, R2=%u mm\n", F, R1, R2);

      String cmd = decideWallFollowing(F, R1, R2);
      sendToServant(cmd);
    } else {
       Serial.println("ToF: no data");
       sendToServant("S");
    }
  } 

  // 3. 独立的 ToF 串口监视输出（不依赖 auto 模式）
  if (millis() - lastToFPrint >= 200) { // 每 200ms 一次
    lastToFPrint = millis();
    if (ToF_read(tofMon)) {
      uint16_t Fm  = applyToFCal(tofMon[0], TOF_OFFSET_F, TOF_SCALE_F);
      uint16_t R1m = applyToFCal(tofMon[1], TOF_OFFSET_R1, TOF_SCALE_R1);
      uint16_t R2m = applyToFCal(tofMon[2], TOF_OFFSET_R2, TOF_SCALE_R2);
      Serial.printf("[ToF MON] F=%u mm, R1=%u mm, R2=%u mm\n", Fm, R1m, R2m);
    } else {
      Serial.println("[ToF MON] no data");
    }
  }

  delay(50);
}