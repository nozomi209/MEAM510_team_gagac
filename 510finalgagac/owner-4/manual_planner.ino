// 手动规划模板（含朝向与撞击动作）：支持预设路线和运行时下发路线
// 使用方法：
// 1) 如使用预设路线，调用 mp_setDefaultRoute()。
// 2) 如需运行时更新，使用 MP_ROUTE:x,y,h,b;... 字符串调用 mp_loadRouteString()。
// 3) 调用 mp_step(x,y,angle) 每次返回一条 "F/L/R/S"，angle 为 -180~180，0° 对应 +Y。

#include <Arduino.h>

struct Waypoint {
  float x;
  float y;
  float headingDeg;  // 目标朝向
  uint8_t bumps;     // 到达后向前撞击次数（0 为不撞）
};

// 路径存储（可被运行时覆盖）
static const uint8_t MP_MAX_ROUTE = 12;
static Waypoint routeBuf[MP_MAX_ROUTE];
uint8_t mp_routeCount = 0;

// 参数（可按车速/场景调整，可被运行时更新）
float MP_DIST_TOL     = 50.0f;   // 到点距离阈值 (mm)
float MP_ANGLE_TOL    = 15.0f;   // 朝向角容差 (deg)
float MP_SPEED_FAR    = 70.0f;   // 远距离速度
float MP_SPEED_NEAR   = 40.0f;   // 近距离减速
float MP_TURN_RATE    = 80.0f;   // 原地转向力度
uint16_t MP_BUMP_FWD_MS  = 500;  // 撞击前冲时间
uint16_t MP_BUMP_STOP_MS = 300;  // 撞后停顿时间

// 状态
static uint8_t mp_idx = 0;
static bool mp_active = false;
static bool mp_inBump = false;
static uint8_t mp_bumpDone = 0;
static unsigned long mp_bumpT0 = 0;

static float mp_normDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// 预设路线：用户提供的完整动作序列
void mp_setDefaultRoute() {
  Waypoint preset[] = {
    {5120.0f, 4080.0f,   90.0f, 0}, // keypoint 1
    {4661.0f, 4164.0f,  180.0f, 0}, // keypoint 2
    {4661.0f, 4164.0f,  -90.0f, 1}, // 转向 -90 后撞击 1 次
    {4500.0f, 5800.0f,   90.0f, 4}  // 末段撞击 4 次
  };
  uint8_t cnt = sizeof(preset) / sizeof(preset[0]);
  mp_routeCount = (cnt > MP_MAX_ROUTE) ? MP_MAX_ROUTE : cnt;
  for (uint8_t i = 0; i < mp_routeCount; i++) routeBuf[i] = preset[i];
  mp_idx = 0; mp_active = (mp_routeCount > 0); mp_inBump = false; mp_bumpDone = 0;
}

// 启动新的路点序列（从外部传入）
void mp_setRoute(const Waypoint* wp, uint8_t cnt) {
  if (wp != nullptr) {
    mp_routeCount = (cnt > MP_MAX_ROUTE) ? MP_MAX_ROUTE : cnt;
    for (uint8_t i = 0; i < mp_routeCount; i++) routeBuf[i] = wp[i];
  } else {
    // 仅重置起步，不改路点
    mp_routeCount = (mp_routeCount > MP_MAX_ROUTE) ? MP_MAX_ROUTE : mp_routeCount;
  }
  mp_idx = 0; mp_active = (mp_routeCount > 0); mp_inBump = false; mp_bumpDone = 0;
}

// 运行时解析路点字符串："x,y,h,b; x,y,h,b; ..."
bool mp_loadRouteString(const String& s) {
  mp_routeCount = 0;
  int start = 0;
  while (start < s.length() && mp_routeCount < MP_MAX_ROUTE) {
    int sep = s.indexOf(';', start);
    String item = (sep == -1) ? s.substring(start) : s.substring(start, sep);
    item.trim();
    if (item.length() == 0) { if (sep == -1) break; start = sep + 1; continue; }
    int c1 = item.indexOf(',');
    int c2 = item.indexOf(',', c1 + 1);
    int c3 = item.indexOf(',', c2 + 1);
    if (c1 < 0 || c2 < 0 || c3 < 0) { if (sep == -1) break; start = sep + 1; continue; }
    Waypoint w;
    w.x = item.substring(0, c1).toFloat();
    w.y = item.substring(c1 + 1, c2).toFloat();
    w.headingDeg = item.substring(c2 + 1, c3).toFloat();
    w.bumps = (uint8_t)item.substring(c3 + 1).toInt();
    routeBuf[mp_routeCount++] = w;
    if (sep == -1) break;
    start = sep + 1;
  }
  mp_idx = 0;
  mp_active = (mp_routeCount > 0);
  mp_inBump = false;
  mp_bumpDone = 0;
  return mp_active;
}

// 停止
void mp_stop() {
  mp_active = false;
}

bool mp_isActive() {
  return mp_active;
}

// 运行时参数更新：键=值
void mp_updateParam(const String& key, float val) {
  String k = key;
  k.toUpperCase();
  if (k == "MP_DIST_TOL") MP_DIST_TOL = val;
  else if (k == "MP_ANGLE_TOL") MP_ANGLE_TOL = val;
  else if (k == "MP_SPEED_FAR") MP_SPEED_FAR = val;
  else if (k == "MP_SPEED_NEAR") MP_SPEED_NEAR = val;
  else if (k == "MP_TURN_RATE") MP_TURN_RATE = val;
  else if (k == "MP_BUMP_FWD_MS") MP_BUMP_FWD_MS = (uint16_t)val;
  else if (k == "MP_BUMP_STOP_MS") MP_BUMP_STOP_MS = (uint16_t)val;
}

// 生成下一步指令；返回 "S/Lxx/Rxx/Fxx"。到达最后一个路点后返回 "S" 并停机。
String mp_step(float x, float y, float angleDeg) {
  if (!mp_active || mp_routeCount == 0) {
    return "S";
  }

  Waypoint target = routeBuf[mp_idx];

  // 撞击阶段处理
  if (mp_inBump) {
    unsigned long now = millis();
    if (now - mp_bumpT0 < MP_BUMP_FWD_MS) {
      return "F60"; // 前冲
    } else if (now - mp_bumpT0 < (MP_BUMP_FWD_MS + MP_BUMP_STOP_MS)) {
      return "S";   // 停顿
    } else {
      mp_bumpDone++;
      if (mp_bumpDone >= target.bumps) {
        mp_inBump = false;
        mp_bumpDone = 0;
        if (mp_idx + 1 < mp_routeCount) {
          mp_idx++;
        } else {
          mp_active = false;
          return "S";
        }
      }
      mp_bumpT0 = millis();
      return "S";
    }
  }

  // 位置与朝向控制
  float dx = target.x - x;
  float dy = target.y - y;
  float dist = sqrtf(dx * dx + dy * dy);

  // 先到点，再对准朝向，再撞击
  if (dist > MP_DIST_TOL) {
    // 目标朝向：指向路点
    float desired = mp_normDeg((180.0f / PI) * atan2f(dy, dx) + 90.0f);
    float err = mp_normDeg(desired - angleDeg);
    if (fabsf(err) > MP_ANGLE_TOL) {
      return (err > 0) ? "R" + String((int)MP_TURN_RATE) : "L" + String((int)MP_TURN_RATE);
    }
    float spd = (dist < 150.0f) ? MP_SPEED_NEAR : MP_SPEED_FAR;
    return "F" + String((int)spd);
  }

  // 到点后对准指定朝向
  float headingErr = mp_normDeg(target.headingDeg - angleDeg);
  if (fabsf(headingErr) > MP_ANGLE_TOL) {
    return (headingErr > 0) ? "R" + String((int)MP_TURN_RATE) : "L" + String((int)MP_TURN_RATE);
  }

  // 进入撞击阶段（若需要）
  if (target.bumps > 0) {
    mp_inBump = true;
    mp_bumpDone = 0;
    mp_bumpT0 = millis();
    return "F60";
  }

  // 无撞击则切到下一个路点
  if (mp_idx + 1 < mp_routeCount) {
    mp_idx++;
    return "S";
  } else {
    mp_active = false;
    return "S";
  }
}

// ============ 示例：如何在 loop() 中使用 ============
// 假设已从 Servant 收到 VIVE 坐标 (viveX,viveY,viveAngle)
// 在初始化时调用 mp_setRoute(route, ROUTE_COUNT);
// 在 loop 中：
// if (mp_isActive()) {
//   String cmd = mp_step(viveX, viveY, viveAngle);
//   sendToServant(cmd);
// }

