// Owner 板（模式控制 + ToF 巡墙）：接收 Web/Servant 指令，读取 ToF，生成运动命令
// UART between owner and servant 
// Right Wall Following with Auto Switch

#include <HardwareSerial.h>

// ToF function prototypes（在 tof.cpp 中实现）
void ToF_init();
bool ToF_read(uint16_t d[3]);

// 手动规划（基于 VIVE 路点）
struct Waypoint; // 在 manual_planner.ino 中定义
void mp_setRoute(const Waypoint* wp, uint8_t cnt);
void mp_setDefaultRoute();
bool mp_loadRouteString(const String& s);
String mp_step(float x, float y, float angleDeg);
void mp_stop();
bool mp_isActive();
void mp_updateParam(const String& key, float val);
extern uint8_t mp_routeCount;

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

// Vive 坐标与点对点导航
float viveX = 0.0f, viveY = 0.0f, viveAngle = 0.0f;
bool hasViveFix = false;
bool isViveGoto = false;
float gotoTargetX = 0.0f, gotoTargetY = 0.0f;
const float GOTO_DIST_TOL = 50.0f;   // 到点距离阈值 (mm)
const float GOTO_ANGLE_TOL = 20.0f;  // 航向角容差 (deg)
const float GOTO_SPEED_FAR = 70.0f;  // 远距离前进速度
const float GOTO_SPEED_NEAR = 40.0f; // 近距离减速
const float GOTO_TURN_RATE = 80.0f;  // 原地转向力度

// 手动规划开关
bool isManualPlan = false;

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

// 归一化角度到 [-180, 180]
static float normDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// 简单点对点决策：先对角，再前进，支持减速
static bool decideViveGoto(String &cmd) {
  if (!hasViveFix) { cmd = "S"; return false; }
  float dx = gotoTargetX - viveX;
  float dy = gotoTargetY - viveY;
  float dist = sqrtf(dx * dx + dy * dy);

  // 目标航向（坐标系：0° 为 +Y）
  float desired = normDeg((180.0f / PI) * atan2f(dy, dx) + 90.0f);
  float err = normDeg(desired - viveAngle);

  if (dist < GOTO_DIST_TOL) {
    cmd = "S";
    isViveGoto = false;
    return true; // reached
  }

  if (fabsf(err) > GOTO_ANGLE_TOL) {
    cmd = (err > 0) ? "R" + String((int)GOTO_TURN_RATE) : "L" + String((int)GOTO_TURN_RATE);
    return false;
  }

  float spd = (dist < 150.0f) ? GOTO_SPEED_NEAR : GOTO_SPEED_FAR;
  cmd = "F" + String((int)spd);
  return false;
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
    else if (webCmd == "MP_ON") {
      isManualPlan = true;
      isAutoRunning = false;
      isViveGoto = false;
      // 使用当前路点；若未动态下发，则加载默认路线
      if (mp_routeCount == 0) mp_setDefaultRoute();
      mp_setRoute(nullptr, mp_routeCount); // 重新起步
      Serial.println(">>> MANUAL PLANNER STARTED <<<");
    }
    else if (webCmd == "MP_OFF") {
      isManualPlan = false;
      mp_stop();
      sendToServant("S");
      Serial.println(">>> MANUAL PLANNER STOPPED <<<");
    }
    else if (webCmd.startsWith("MP_ROUTE:")) {
      String payload = webCmd.substring(8);
      if (mp_loadRouteString(payload)) {
        Serial.printf(">>> MP_ROUTE loaded, count=%d\n", mp_routeCount);
      } else {
        Serial.println(">>> MP_ROUTE load failed (format: x,y,h,b;...)");
      }
    }
    else if (webCmd.startsWith("MP_PARAM:")) {
      // 形如 MP_PARAM:MP_SPEED_FAR=70
      String kv = webCmd.substring(9);
      int eq = kv.indexOf('=');
      if (eq > 0) {
        String key = kv.substring(0, eq);
        float val = kv.substring(eq + 1).toFloat();
        mp_updateParam(key, val);
        Serial.printf(">>> MP_PARAM %s = %.2f\n", key.c_str(), val);
      }
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
    // 解析 VIVE 数据: "VIVE:x,y,a"
    else if (webCmd.startsWith("VIVE:")) {
      int c1 = webCmd.indexOf(',');
      int c2 = webCmd.indexOf(',', c1 + 1);
      if (c1 > 5 && c2 > c1) {
        viveX = webCmd.substring(5, c1).toFloat();
        viveY = webCmd.substring(c1 + 1, c2).toFloat();
        viveAngle = webCmd.substring(c2 + 1).toFloat();
        viveAngle = normDeg(viveAngle);
        hasViveFix = true;
      }
    }
    // 设置/启动 VIVE 点对点: "GOTO:x,y"
    else if (webCmd.startsWith("GOTO:")) {
      int c = webCmd.indexOf(',');
      if (c > 5) {
        gotoTargetX = webCmd.substring(5, c).toFloat();
        gotoTargetY = webCmd.substring(c + 1).toFloat();
        isViveGoto = true;
        isAutoRunning = false;
        Serial.printf(">>> VIVE GOTO start: target=(%.1f, %.1f)\n", gotoTargetX, gotoTargetY);
      }
    }
    else if (webCmd == "GOTO_OFF") {
      isViveGoto = false;
      sendToServant("S");
      Serial.println(">>> VIVE GOTO stopped");
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

  // 3. VIVE 点对点（独立于巡墙）
  if (isViveGoto) {
    String cmd;
    decideViveGoto(cmd);
    sendToServant(cmd);
  }

  // 4. 手动规划（路点序列 + 撞击）
  if (isManualPlan && mp_isActive() && hasViveFix) {
    String cmd = mp_step(viveX, viveY, viveAngle);
    sendToServant(cmd);
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