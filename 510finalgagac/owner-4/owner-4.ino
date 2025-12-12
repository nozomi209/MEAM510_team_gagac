/*
 * owner-4.ino — Owner 板主控（模式调度 + ToF 巡墙/避障 + 导航命令生成）
 *
 * 本文件主要包含：
 * 1) 与 Servant 板的 UART 通信与命令分发
 *    - 从 Servant 接收字符串命令（来源：网页/上位机转发、或 Servant 周期上报 VIVE）
 *    - 解析并更新：
 *      - VIVE 数据：`VIVE:x,y,a`（更新 viveX/viveY/viveAngle/hasViveFix）
 *      - 模式控制：AUTO_ON/OFF、GOTO:、PLAN*、MP_* 等
 *
 * 2) ToF 三路距离读取与巡墙行为
 *    - `ToF_init()` / `ToF_read(d[3])` 的实现位于 `tof-new-sensor.ino`
 *    - 本文件在 `setup()` 初始化 ToF；在 `loop()` 周期读取并做标定 `applyToFCal()`
 *    - 巡墙决策函数位于 `behavior-wall.ino`：`decideWallFollowing(F,R1,R2)` 返回 "F/L/R/S..."
 *
 * 3) 基于 VIVE 的点对点/路径规划模式（输出底盘命令给 Servant）
 *    - 简单点对点：本文件内 `decideViveGoto()`（先对角再前进）
 *    - 轴对齐路径规划 + 执行器：`manual_planner.cpp/.h`
 *      - `mp_planTargets(...)` 规划 1~3 个目标点
 *      - `mp_step(viveX,viveY,viveAngle)` 每次输出一条 "F/L/R/S" 指令
 *
 * 4) 输出给 Servant 的命令格式
 *    - 统一使用 `sendToServant(cmd)` 下发（如 "F70", "L80", "R80", "S"）
 *
 * 说明：
 * - 本文件是 Owner 的“调度中心”，真正的巡墙算法在 `behavior-wall.ino`，
 *   ToF 传感器驱动在 `tof-new-sensor.ino`，规划/导航核心在 `manual_planner.cpp`。
 */

// Owner 板（模式控制 + ToF 巡墙）：接收 Web/Servant 指令，读取 ToF，生成运动命令
// UART between owner and servant
// Right Wall Following with Auto Switch

#include <HardwareSerial.h>

// ToF function prototypes（在 tof.cpp 中实现）
void ToF_init();
bool ToF_read(uint16_t d[3]);

#include "manual_planner.h"

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
bool isPathPlanMode = false; // 新模式：单点/多点轴对齐规划
bool planStartFixed = false;
float planStartX = 0.0f, planStartY = 0.0f;

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

  // 默认外部边界（mm）：X 3920~5100, Y 1390~5700
  mp_setBoundary(3920.0f, 5100.0f, 5700.0f, 1390.0f);
  mp_setBoundaryEnabled(true);

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
    // 设置外部边界: PLAN_BOUND:xmin,xmax,ymax,ymin
    else if (webCmd.startsWith("PLAN_BOUND:")) {
      String payload = webCmd.substring(11);
      float vals[4]; uint8_t cnt = 0;
      int start = 0;
      while (start < payload.length() && cnt < 4) {
        int c = payload.indexOf(',', start);
        String item = (c == -1) ? payload.substring(start) : payload.substring(start, c);
        item.trim();
        if (item.length() > 0) vals[cnt++] = item.toFloat();
        if (c == -1) break;
        start = c + 1;
      }
      if (cnt == 4) {
        mp_setBoundary(vals[0], vals[1], vals[2], vals[3]);
        mp_setBoundaryEnabled(true);
        Serial.printf(">>> PLAN_BOUND set xmin=%.1f xmax=%.1f ymax=%.1f ymin=%.1f\n",
                      vals[0], vals[1], vals[2], vals[3]);
      } else {
        Serial.println(">>> PLAN_BOUND 格式: PLAN_BOUND:xmin,xmax,ymax,ymin");
      }
    }
    // 新：设置障碍框（left,right,top,bottom[,margin]），用于轴对齐规划
    else if (webCmd.startsWith("PLAN_OBS:")) {
      String payload = webCmd.substring(9);
      float vals[5]; uint8_t cnt = 0;
      int start = 0;
      while (start < payload.length() && cnt < 5) {
        int c = payload.indexOf(',', start);
        String item = (c == -1) ? payload.substring(start) : payload.substring(start, c);
        item.trim();
        if (item.length() > 0) vals[cnt++] = item.toFloat();
        if (c == -1) break;
        start = c + 1;
      }
      if (cnt >= 4) {
        mp_setObstacleBox(vals[0], vals[1], vals[2], vals[3]);
        mp_setObstacleEnabled(true);
        if (cnt >= 5) mp_setPathMargin(vals[4]);
        Serial.printf(">>> PLAN_OBS set L=%.1f R=%.1f T=%.1f B=%.1f margin=%.1f\n",
                      vals[0], vals[1], vals[2], vals[3], MP_PATH_MARGIN);
      } else {
        Serial.println(">>> PLAN_OBS 格式: PLAN_OBS:left,right,top,bottom[,margin]");
      }
    }
    else if (webCmd == "PLAN_OBS_OFF") {
      mp_setObstacleEnabled(false);
      Serial.println(">>> PLAN_OBS disabled");
    }
    // 锁定起点：PLAN_SET_START 或 PLAN_SET_START:x,y
    else if (webCmd.startsWith("PLAN_SET_START")) {
      int c = webCmd.indexOf(':');
      if (c > 0) {
        planStartX = webCmd.substring(c + 1, webCmd.indexOf(',', c + 1)).toFloat();
        planStartY = webCmd.substring(webCmd.indexOf(',', c + 1) + 1).toFloat();
        planStartFixed = true;
        Serial.printf(">>> PLAN start locked: (%.1f, %.1f)\n", planStartX, planStartY);
      } else if (hasViveFix) {
        planStartX = viveX; planStartY = viveY; planStartFixed = true;
        Serial.printf(">>> PLAN start locked from VIVE: (%.1f, %.1f)\n", planStartX, planStartY);
      } else {
        Serial.println(">>> PLAN_SET_START 失败：无 VIVE 坐标且未提供坐标");
      }
    }
    else if (webCmd == "PLAN_CLEAR_START") {
      planStartFixed = false;
      Serial.println(">>> PLAN start cleared (will use live VIVE)");
    }
    // 新：单点规划命令，格式 PLAN1:x,y
    else if (webCmd.startsWith("PLAN1:")) {
      int c = webCmd.indexOf(',');
      if (c > 6) {
        float tx = webCmd.substring(6, c).toFloat();
        float ty = webCmd.substring(c + 1).toFloat();
        if (!hasViveFix) {
          Serial.println(">>> PLAN1 失败：无 VIVE 坐标");
        } else {
          float list[1][2] = {{tx, ty}};
          bool ok = mp_planTargets(viveX, viveY, list, 1, MP_PATH_MARGIN);
          if (ok) {
            isPathPlanMode = true;
            isManualPlan = false;
            isAutoRunning = false;
            isViveGoto = false;
            Serial.printf(">>> PLAN1 规划成功 -> 目标(%.1f, %.1f)\n", tx, ty);
          } else {
            Serial.println(">>> PLAN1 规划失败：路径被障碍挡住或路点超限");
          }
        }
      } else {
        Serial.println(">>> PLAN1 格式: PLAN1:x,y");
      }
    }
    // 新：使用锁定起点规划执行，格式 PLAN_GO:x,y
    else if (webCmd.startsWith("PLAN_GO:")) {
      int c = webCmd.indexOf(',');
      if (c > 7) {
        float tx = webCmd.substring(7, c).toFloat();
        float ty = webCmd.substring(c + 1).toFloat();
        if (!planStartFixed && !hasViveFix) {
          Serial.println(">>> PLAN_GO 失败：无锁定起点且无 VIVE 坐标");
        } else {
          float sx = planStartFixed ? planStartX : viveX;
          float sy = planStartFixed ? planStartY : viveY;
          float list[1][2] = {{tx, ty}};
          bool ok = mp_planTargets(sx, sy, list, 1, MP_PATH_MARGIN);
          if (ok) {
            isPathPlanMode = true;
            isManualPlan = false;
            isAutoRunning = false;
            isViveGoto = false;
            Serial.printf(">>> PLAN_GO 成功: start(%.1f, %.1f) -> (%.1f, %.1f)\n", sx, sy, tx, ty);
          } else {
            Serial.println(">>> PLAN_GO 规划失败：路径被障碍挡住或路点超限");
          }
        }
      } else {
        Serial.println(">>> PLAN_GO 格式: PLAN_GO:x,y");
      }
    }
    else if (webCmd == "PLAN_STOP") {
      isPathPlanMode = false;
      mp_stop();
      sendToServant("S");
      Serial.println(">>> PLAN 停止");
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

  // 4. 新模式：轴对齐规划（基于 VIVE 坐标的单点/多点路径）
  if (isPathPlanMode && mp_isActive() && hasViveFix) {
    String cmd = mp_step(viveX, viveY, viveAngle);
    sendToServant(cmd);
    if (!mp_isActive()) { // 完成后自动停并退出模式
      isPathPlanMode = false;
      sendToServant("S");
      Serial.println(">>> PLAN 完成");
    }
  }

  // 5. 手动规划（路点序列 + 撞击）
  if (isManualPlan && mp_isActive() && hasViveFix) {
    String cmd = mp_step(viveX, viveY, viveAngle);
    sendToServant(cmd);
  }

  // 6. 独立的 ToF 串口监视输出（不依赖 auto 模式）
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