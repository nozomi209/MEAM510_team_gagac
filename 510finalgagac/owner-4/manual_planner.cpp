#include <Arduino.h>
#include "manual_planner_types.h"
#include "manual_planner.h"

// 绕障配置
float MP_PATH_MARGIN = 150.0f; // 安全边距 (mm)
static bool mp_obstacleEnabled = true;
static ObstacleBox mp_obstacleBox = {3920.0f, 5100.0f, 5700.0f, 1390.0f};

// 外部边界（默认以场地边界生效，可通过接口调整）
bool mp_boundEnabled = true;
float MP_BOUND_X_MIN = 3920.0f;
float MP_BOUND_X_MAX = 5100.0f;
float MP_BOUND_Y_MIN = 1390.0f;
float MP_BOUND_Y_MAX = 5700.0f;

void mp_setObstacleBox(float left, float right, float top, float bottom) {
  mp_obstacleBox.left = left; mp_obstacleBox.right = right;
  mp_obstacleBox.top = top;  mp_obstacleBox.bottom = bottom;
  mp_obstacleEnabled = true;
}
void mp_setObstacleEnabled(bool enabled) { mp_obstacleEnabled = enabled; }
void mp_setPathMargin(float margin) { if (margin > 0.0f) MP_PATH_MARGIN = margin; }
void mp_setBoundary(float xmin, float xmax, float ymax, float ymin) {
  MP_BOUND_X_MIN = xmin; MP_BOUND_X_MAX = xmax;
  MP_BOUND_Y_MIN = ymin; MP_BOUND_Y_MAX = ymax;
  mp_boundEnabled = true;
}
void mp_setBoundaryEnabled(bool enabled) { mp_boundEnabled = enabled; }

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

// =============== 伪代码实现：轴对齐路径规划（独立模式） ===============
static PlannerPoint mp_clampPoint(const PlannerPoint& p) {
  if (!mp_boundEnabled) return p;
  PlannerPoint r = p;
  r.x = constrain(r.x, MP_BOUND_X_MIN, MP_BOUND_X_MAX);
  r.y = constrain(r.y, MP_BOUND_Y_MIN, MP_BOUND_Y_MAX);
  return r;
}

static bool mp_insideBound(const PlannerPoint& p) {
  if (!mp_boundEnabled) return true;
  return (p.x >= MP_BOUND_X_MIN && p.x <= MP_BOUND_X_MAX &&
          p.y >= MP_BOUND_Y_MIN && p.y <= MP_BOUND_Y_MAX);
}

static ObstacleBox mp_normalizeBox(const ObstacleBox& b, float margin) {
  ObstacleBox n;
  float l = min(b.left, b.right), r = max(b.left, b.right);
  float btm = min(b.bottom, b.top), tp = max(b.bottom, b.top);
  n.left = l - margin; n.right = r + margin;
  n.bottom = btm - margin; n.top = tp + margin;
  return n;
}

static bool mp_pointInsideBox(const PlannerPoint& p, const ObstacleBox& box) {
  return (p.x >= box.left && p.x <= box.right && p.y >= box.bottom && p.y <= box.top);
}

static bool mp_segmentIntersectsObstacle(const PlannerSegment& s, float margin) {
  if (!mp_obstacleEnabled) return false;
  ObstacleBox box = mp_normalizeBox(mp_obstacleBox, margin);
  PlannerPoint p1 = s.a, p2 = s.b;
  if (mp_pointInsideBox(p1, box) || mp_pointInsideBox(p2, box)) return true;

  bool horizontal = fabsf(p1.y - p2.y) < 1e-3f;
  if (horizontal) {
    float y = p1.y;
    if (y >= box.bottom && y <= box.top) {
      float minx = min(p1.x, p2.x), maxx = max(p1.x, p2.x);
      if (maxx >= box.left && minx <= box.right) return true;
    }
  } else {
    float x = p1.x;
    if (x >= box.left && x <= box.right) {
      float miny = min(p1.y, p2.y), maxy = max(p1.y, p2.y);
      if (maxy >= box.bottom && miny <= box.top) return true;
    }
  }
  return false;
}

static bool mp_pathDoesNotIntersectObstacle(const PlannerSegment* segs, uint8_t cnt, float margin) {
  for (uint8_t i = 0; i < cnt; i++) {
    if (!mp_insideBound(segs[i].a) || !mp_insideBound(segs[i].b)) return false; // 越界即视为无效
    if (mp_segmentIntersectsObstacle(segs[i], margin)) return false;
  }
  return true;
}

// 依据伪代码：返回段数（0=失败）
uint8_t plan_path(PlannerPoint S, PlannerPoint T, float margin, PlannerSegment outSegs[3]) {
  // 起点/终点先约束到边界内
  S = mp_clampPoint(S);
  T = mp_clampPoint(T);

  if (!mp_obstacleEnabled) {
    outSegs[0] = {S, {T.x, S.y}};
    outSegs[1] = {{T.x, S.y}, T};
    return 2;
  }

  PlannerSegment p1 = {S, {T.x, S.y}};
  PlannerSegment p2 = {{T.x, S.y}, T};
  PlannerSegment xy[2] = {p1, p2};
  if (mp_pathDoesNotIntersectObstacle(xy, 2, margin)) { outSegs[0] = p1; outSegs[1] = p2; return 2; }

  p1 = {S, {S.x, T.y}};
  p2 = {{S.x, T.y}, T};
  PlannerSegment yx[2] = {p1, p2};
  if (mp_pathDoesNotIntersectObstacle(yx, 2, margin)) { outSegs[0] = p1; outSegs[1] = p2; return 2; }

  ObstacleBox base = mp_normalizeBox(mp_obstacleBox, 0.0f);
  float Y_top = base.top + margin, Y_bottom = base.bottom - margin;
  float X_left = base.left - margin, X_right = base.right + margin;
  // 绕障拐点也做一次边界约束
  Y_top = constrain(Y_top, MP_BOUND_Y_MIN, MP_BOUND_Y_MAX);
  Y_bottom = constrain(Y_bottom, MP_BOUND_Y_MIN, MP_BOUND_Y_MAX);
  X_left = constrain(X_left, MP_BOUND_X_MIN, MP_BOUND_X_MAX);
  X_right = constrain(X_right, MP_BOUND_X_MIN, MP_BOUND_X_MAX);
  PlannerSegment detours[4][3] = {
    { {S, {S.x, Y_top}},    {{S.x, Y_top}, {T.x, Y_top}},    {{T.x, Y_top}, T} },
    { {S, {S.x, Y_bottom}}, {{S.x, Y_bottom}, {T.x, Y_bottom}}, {{T.x, Y_bottom}, T} },
    { {S, {X_left, S.y}},   {{X_left, S.y}, {X_left, T.y}},   {{X_left, T.y}, T} },
    { {S, {X_right, S.y}},  {{X_right, S.y}, {X_right, T.y}}, {{X_right, T.y}, T} }
  };
  for (uint8_t i = 0; i < 4; i++) {
    if (mp_pathDoesNotIntersectObstacle(detours[i], 3, margin)) {
      for (uint8_t j = 0; j < 3; j++) outSegs[j] = detours[i][j];
      return 3;
    }
  }
  return 0;
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

// =============== 新模式：将规划段转为路点并装载 ===============
static float mp_headingFromSegment(const PlannerSegment& seg) {
  float dx = seg.b.x - seg.a.x;
  float dy = seg.b.y - seg.a.y;
  if (fabsf(dx) > fabsf(dy)) return (dx >= 0.0f) ? 90.0f : -90.0f; // 沿 X 轴
  return (dy >= 0.0f) ? 0.0f : 180.0f; // 沿 Y 轴
}

static uint8_t mp_segmentsToWaypoints(const PlannerSegment* segs, uint8_t segCnt, Waypoint* out, uint8_t maxOut) {
  uint8_t added = 0;
  for (uint8_t i = 0; i < segCnt; i++) {
    if (added >= maxOut) return 0;
    out[added].x = segs[i].b.x;
    out[added].y = segs[i].b.y;
    out[added].headingDeg = mp_headingFromSegment(segs[i]);
    out[added].bumps = 0;
    added++;
  }
  return added;
}

// 计划任意 1~3 目标点（使用当前 mp_setRoute，便于独立模式调用）
bool mp_planTargets(float startX, float startY, const float targetList[][2], uint8_t targetCount, float margin) {
  if (targetCount == 0 || targetCount > 3) return false;
  PlannerPoint cur { startX, startY };
  Waypoint tmp[MP_MAX_ROUTE];
  uint8_t wpCount = 0;
  PlannerSegment segBuf[3];

  for (uint8_t i = 0; i < targetCount; i++) {
    PlannerPoint tgt { targetList[i][0], targetList[i][1] };
    uint8_t segCnt = plan_path(cur, tgt, margin, segBuf);
    if (segCnt == 0) return false;
    uint8_t added = mp_segmentsToWaypoints(segBuf, segCnt, tmp + wpCount, MP_MAX_ROUTE - wpCount);
    if (added == 0) return false;
    wpCount += added;
    cur = tgt;
  }

  mp_setRoute(tmp, wpCount);
  return true;
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

