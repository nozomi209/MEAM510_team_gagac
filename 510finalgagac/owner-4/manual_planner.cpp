/*
 * manual_planner.cpp — Owner 侧路径规划与执行（导航核心）
 *
 * 本文件主要包含：
 * 1) 轴对齐（Manhattan）路径规划（带矩形障碍绕行 + 场地边界约束）
 *    - `plan_path(S, T, margin, outSegs)`：在 X/Y 轴对齐的前提下，生成 2~3 段折线路径
 *    - 支持一个矩形障碍框（可加安全边距 `MP_PATH_MARGIN`）以及可选的场地边界约束
 *
 * 2) 将规划段转换为可执行路点并装载
 *    - `mp_planTargets(startX, startY, targetList, targetCount, margin)`：
 *      支持 1~3 个目标点；每个目标点会被拆成若干“轴对齐路点”
 *
 * 3) 导航执行器：根据当前 Vive 位姿输出底盘指令
 *    - `mp_step(x, y, angleDeg)`：输入当前 (x,y,angle)，返回一条运动命令字符串：
 *        "Fxx" 前进、"Lxx"/"Rxx" 原地转向、"S" 停车（xx 为强度/速度百分比）
 *      到达最后路点后会自动返回 "S" 并停机（`mp_isActive()` 变为 false）
 *
 * 4) 低频“定位—再规划—执行”导航模式（默认启用，用于对抗 Vive 角度/位置偏差）
 *    - 运行逻辑：停车定位（多次采样判稳）→ 以稳定位置重规划剩余路径 → 执行一小段时间 → 再停车
 *    - 相关参数支持通过 `mp_updateParam("键", 值)` 在运行时调整（Owner 通过 `MP_PARAM:` 下发）
 *      典型键：MP_LF_EXEC_MS、MP_LF_STOP_MIN_MS、MP_LF_FIX_N、MP_LF_POS_STD_MAX、MP_LF_ANG_STD_MAX
 *
 * 主要对外接口（见 manual_planner.h）：
 * - `mp_planTargets(...)` / `mp_setRoute(...)`：装载路径
 * - `mp_step(...)`：每个 loop 调用一次得到下一条运动指令
 * - `mp_stop()` / `mp_isActive()`：停止与查询状态
 * - `mp_updateParam(key,val)`：动态调参（速度/阈值/低频段长等）
 */

#include <Arduino.h>
#include "manual_planner_types.h"
#include "manual_planner.h"

// 绕障配置
// 注意：边界已考虑小车尺寸，margin 设为 0
float MP_PATH_MARGIN = 0.0f; // 安全边距 (mm)，边界已预留
static bool mp_obstacleEnabled = true;
// 障碍框：X 4072~4950, Y 3120~4257（left, right, top, bottom）
static ObstacleBox mp_obstacleBox = {4072.0f, 4950.0f, 4257.0f, 3120.0f};

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
// 置信区间：允许小误差范围内视为"到达"
float MP_DIST_TOL     = 50.0f;   // 到点距离阈值 (mm)，50mm 内视为到达
float MP_ANGLE_TOL    = 45.0f;   // 朝向角容差 (deg)，角度要求不严格
float MP_SPEED_FAR    = 60.0f;   // 远距离速度
float MP_SPEED_NEAR   = 35.0f;   // 近距离减速
float MP_TURN_RATE    = 65.0f;   // 原地转向力度
uint16_t MP_BUMP_FWD_MS  = 500;  // 撞击前冲时间
uint16_t MP_BUMP_STOP_MS = 300;  // 撞后停顿时间
static bool MP_DEBUG = false;    // 串口调试输出（通过 MP_PARAM:MP_DEBUG=1 开启）

// 状态
static uint8_t mp_idx = 0;
static bool mp_active = false;
static bool mp_inBump = false;
static uint8_t mp_bumpDone = 0;
static unsigned long mp_bumpT0 = 0;

// =============== 低频"定位-再规划-执行"滚动控制（抗 Vive 偏差） ===============
// 思路：执行一小段时间 -> 停车 -> 累积多次 Vive 采样并判定稳定 -> 以稳定位置重新规划 -> 再执行
// 默认关闭低频，使用中频模式
static bool MP_LF_ENABLED = false;
static uint16_t MP_LF_EXEC_MS = 900;     // 每段执行时长（ms）- 低频模式
static uint16_t MP_LF_STOP_MIN_MS = 250; // 每段定位最短停车时间（ms）
static uint8_t  MP_LF_FIX_N = 8;         // 定位阶段需要的采样数
static float    MP_LF_POS_STD_MAX = 30.0f;  // 稳定判定：位置标准差阈值（mm）
static float    MP_LF_ANG_STD_MAX = 12.0f;  // 稳定判定：角度标准差阈值（deg）

// =============== 中频导航模式（默认启用，更快响应） ===============
// 相比低频：执行周期更短、停车时间更短、采样数更少
static bool MP_MF_ENABLED = true;        // 中频模式开关（默认启用）
static uint16_t MP_MF_EXEC_MS = 400;     // 中频每段执行时长（ms）
static uint16_t MP_MF_STOP_MIN_MS = 100; // 中频停车定位时间（ms）
static uint8_t  MP_MF_FIX_N = 4;         // 中频采样数
static float    MP_MF_POS_STD_MAX = 40.0f;  // 中频位置稳定阈值
static float    MP_MF_ANG_STD_MAX = 15.0f;  // 中频角度稳定阈值

static bool mp_hasTargetsForReplan = false;
static uint8_t mp_targetCount = 0;
static float mp_targets[3][2];

enum MpLfState : uint8_t { LF_STOP_LOCATE = 0, LF_REPLAN = 1, LF_EXEC = 2 };
static MpLfState mp_lfState = LF_STOP_LOCATE;
static unsigned long mp_lfT0 = 0;
static unsigned long mp_lfExecT0 = 0;

struct MpPoseSample { float x; float y; float a; };
static const uint8_t MP_LF_MAX_SAMPLES = 20;
static MpPoseSample mp_lfSamples[MP_LF_MAX_SAMPLES];
static uint8_t mp_lfSampleCount = 0;

static void mp_lfResetSamples() { mp_lfSampleCount = 0; }

static void mp_lfAddSample(float x, float y, float aDeg) {
  if (mp_lfSampleCount < MP_LF_MAX_SAMPLES) {
    mp_lfSamples[mp_lfSampleCount++] = {x, y, aDeg};
  } else {
    // 简单滚动：丢最老的，整体左移
    for (uint8_t i = 1; i < MP_LF_MAX_SAMPLES; i++) mp_lfSamples[i - 1] = mp_lfSamples[i];
    mp_lfSamples[MP_LF_MAX_SAMPLES - 1] = {x, y, aDeg};
  }
}

static float mp_degToRad(float d) { return d * (PI / 180.0f); }
static float mp_radToDeg(float r) { return r * (180.0f / PI); }
static float mp_normDeg(float a);

// 计算稳定的位姿（均值 + 标准差阈值）
static bool mp_lfComputeStablePose(float& outX, float& outY, float& outAdeg, float& outPosStd, float& outAngStd) {
  if (mp_lfSampleCount < MP_LF_FIX_N || MP_LF_FIX_N == 0) return false;

  // 使用最近 N 个样本
  uint8_t n = MP_LF_FIX_N;
  if (n > mp_lfSampleCount) n = mp_lfSampleCount;
  uint8_t start = mp_lfSampleCount - n;

  float sumX = 0.0f, sumY = 0.0f;
  float sumSin = 0.0f, sumCos = 0.0f;
  for (uint8_t i = start; i < mp_lfSampleCount; i++) {
    sumX += mp_lfSamples[i].x;
    sumY += mp_lfSamples[i].y;
    float ar = mp_degToRad(mp_lfSamples[i].a);
    sumSin += sinf(ar);
    sumCos += cosf(ar);
  }
  float meanX = sumX / n;
  float meanY = sumY / n;
  float meanA = mp_radToDeg(atan2f(sumSin / n, sumCos / n));
  meanA = mp_normDeg(meanA);

  float varX = 0.0f, varY = 0.0f, varA = 0.0f;
  for (uint8_t i = start; i < mp_lfSampleCount; i++) {
    float dx = mp_lfSamples[i].x - meanX;
    float dy = mp_lfSamples[i].y - meanY;
    varX += dx * dx;
    varY += dy * dy;
    float da = mp_normDeg(mp_lfSamples[i].a - meanA);
    varA += da * da;
  }
  varX /= n; varY /= n; varA /= n;
  float stdX = sqrtf(varX);
  float stdY = sqrtf(varY);
  float posStd = sqrtf(stdX * stdX + stdY * stdY);
  float angStd = sqrtf(varA);

  outX = meanX; outY = meanY; outAdeg = meanA;
  outPosStd = posStd; outAngStd = angStd;
  return (posStd <= MP_LF_POS_STD_MAX && angStd <= MP_LF_ANG_STD_MAX);
}

static float mp_normDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// 将角度误差映射成转向力度（0~MP_TURN_RATE），用于减少过冲/抖动
static float mp_turnRateFromErr(float absErrDeg) {
  // absErrDeg 已经是 |error|（度）
  float maxRate = MP_TURN_RATE;
  // 小误差时用一个较小的最小转向，避免“转不动”
  float minRate = min(25.0f, maxRate);
  // 误差达到该值时使用最大转向力度
  const float FULL_ERR_DEG = 60.0f;
  float denom = (FULL_ERR_DEG - MP_ANGLE_TOL);
  if (denom < 1e-3f) return maxRate;
  float t = (absErrDeg - MP_ANGLE_TOL) / denom;
  t = constrain(t, 0.0f, 1.0f);
  return minRate + (maxRate - minRate) * t;
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
  mp_lfState = LF_STOP_LOCATE;
  mp_lfT0 = millis();
  mp_lfExecT0 = 0;
  mp_lfResetSamples();
  mp_hasTargetsForReplan = false;
  mp_targetCount = 0;
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
  // 记录目标点，便于低频滚动重规划
  mp_targetCount = targetCount;
  for (uint8_t i = 0; i < targetCount; i++) {
    mp_targets[i][0] = targetList[i][0];
    mp_targets[i][1] = targetList[i][1];
  }
  mp_hasTargetsForReplan = true;

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
  // 低频模式：从“停车定位”开始跑（避免一上来就吃到角度/位置偏差）
  mp_lfState = LF_STOP_LOCATE;
  mp_lfT0 = millis();
  mp_lfExecT0 = 0;
  mp_lfResetSamples();
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
  else if (k == "MP_DEBUG") MP_DEBUG = (val >= 0.5f);
  // 低频滚动参数
  // 注意：为了避免误切回旧模式，这里不允许关闭低频；只接受开启（val>=0.5）
  else if (k == "MP_LF_ENABLED") { MP_LF_ENABLED = (val >= 0.5f); }
  else if (k == "MP_LF_EXEC_MS") MP_LF_EXEC_MS = (uint16_t)max(0.0f, val);
  else if (k == "MP_LF_STOP_MIN_MS") MP_LF_STOP_MIN_MS = (uint16_t)max(0.0f, val);
  else if (k == "MP_LF_FIX_N") MP_LF_FIX_N = (uint8_t)constrain((int)val, 1, (int)MP_LF_MAX_SAMPLES);
  else if (k == "MP_LF_POS_STD_MAX") MP_LF_POS_STD_MAX = max(0.0f, val);
  else if (k == "MP_LF_ANG_STD_MAX") MP_LF_ANG_STD_MAX = max(0.0f, val);
  // 中频滚动参数
  else if (k == "MP_MF_ENABLED") {
    MP_MF_ENABLED = (val >= 0.5f);
    if (MP_MF_ENABLED) MP_LF_ENABLED = false; // 中频开启时自动关闭低频
  }
  else if (k == "MP_MF_EXEC_MS") MP_MF_EXEC_MS = (uint16_t)max(100.0f, val);
  else if (k == "MP_MF_STOP_MIN_MS") MP_MF_STOP_MIN_MS = (uint16_t)max(0.0f, val);
  else if (k == "MP_MF_FIX_N") MP_MF_FIX_N = (uint8_t)constrain((int)val, 1, (int)MP_LF_MAX_SAMPLES);
  else if (k == "MP_MF_POS_STD_MAX") MP_MF_POS_STD_MAX = max(0.0f, val);
  else if (k == "MP_MF_ANG_STD_MAX") MP_MF_ANG_STD_MAX = max(0.0f, val);
}

// 生成下一步指令；返回 "S/Lxx/Rxx/Fxx"。到达最后一个路点后返回 "S" 并停机。
String mp_step(float x, float y, float angleDeg) {
  if (!mp_active || mp_routeCount == 0) {
    return "S";
  }

  // 低频/中频滚动模式：停车->稳定定位->重规划->执行一段
  // 中频模式优先级高于低频
  bool useFreqMode = (MP_MF_ENABLED || MP_LF_ENABLED) && mp_hasTargetsForReplan;
  if (useFreqMode) {
    unsigned long now = millis();
    // 根据模式选择参数
    uint16_t execMs = MP_MF_ENABLED ? MP_MF_EXEC_MS : MP_LF_EXEC_MS;
    uint16_t stopMinMs = MP_MF_ENABLED ? MP_MF_STOP_MIN_MS : MP_LF_STOP_MIN_MS;
    uint8_t fixN = MP_MF_ENABLED ? MP_MF_FIX_N : MP_LF_FIX_N;
    float posStdMax = MP_MF_ENABLED ? MP_MF_POS_STD_MAX : MP_LF_POS_STD_MAX;
    float angStdMax = MP_MF_ENABLED ? MP_MF_ANG_STD_MAX : MP_LF_ANG_STD_MAX;

    switch (mp_lfState) {
      case LF_STOP_LOCATE: {
        // 停车并累积采样
        mp_lfAddSample(x, y, angleDeg);
        float sx, sy, sa, posStd, angStd;
        // 临时覆盖采样数阈值
        uint8_t oldFixN = MP_LF_FIX_N;
        float oldPosMax = MP_LF_POS_STD_MAX;
        float oldAngMax = MP_LF_ANG_STD_MAX;
        MP_LF_FIX_N = fixN;
        MP_LF_POS_STD_MAX = posStdMax;
        MP_LF_ANG_STD_MAX = angStdMax;
        bool stable = mp_lfComputeStablePose(sx, sy, sa, posStd, angStd);
        MP_LF_FIX_N = oldFixN;
        MP_LF_POS_STD_MAX = oldPosMax;
        MP_LF_ANG_STD_MAX = oldAngMax;
        if ((now - mp_lfT0) >= stopMinMs && stable) {
          // 进入重规划阶段
          mp_lfState = LF_REPLAN;
        }
        return "S";
      }
      case LF_REPLAN: {
        float sx = x, sy = y, sa = angleDeg, posStd = 999.0f, angStd = 999.0f;
        (void)mp_lfComputeStablePose(sx, sy, sa, posStd, angStd); // 若不稳定则退化用实时值
        bool ok = mp_planTargets(sx, sy, mp_targets, mp_targetCount, MP_PATH_MARGIN);
        // mp_planTargets 会把 state 重置到 STOP_LOCATE；这里强制进入 EXEC
        if (ok) {
          mp_lfState = LF_EXEC;
          mp_lfExecT0 = now;
          mp_lfResetSamples();
        } else {
          // 规划失败：保持停车，继续等待更稳定的定位
          mp_lfState = LF_STOP_LOCATE;
          mp_lfT0 = now;
          mp_lfResetSamples();
        }
        return "S";
      }
      case LF_EXEC: {
        if ((now - mp_lfExecT0) >= execMs) {
          mp_lfState = LF_STOP_LOCATE;
          mp_lfT0 = now;
          mp_lfResetSamples();
          return "S";
        }
        // 继续走原有高频路点控制
        // 注意：mp_step 内部会消耗路点并在完成后 mp_active=false
        break;
      }
      default:
        mp_lfState = LF_STOP_LOCATE;
        mp_lfT0 = now;
        mp_lfResetSamples();
        return "S";
    }
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
      float rate = mp_turnRateFromErr(fabsf(err));
      String cmd = (err > 0) ? "R" + String((int)rate) : "L" + String((int)rate);
      if (MP_DEBUG) {
        Serial.printf("[MP] idx=%u dist=%.1f desired=%.1f ang=%.1f err=%.1f rate=%.1f -> %s\n",
                      (unsigned)mp_idx, dist, desired, angleDeg, err, rate, cmd.c_str());
      }
      return cmd;
    }
    float spd = (dist < 150.0f) ? MP_SPEED_NEAR : MP_SPEED_FAR;
    String cmd = "F" + String((int)spd);
    if (MP_DEBUG) {
      Serial.printf("[MP] idx=%u dist=%.1f desired=%.1f ang=%.1f -> %s\n",
                    (unsigned)mp_idx, dist, desired, angleDeg, cmd.c_str());
    }
    return cmd;
  }

  // 到点后对准指定朝向
  float headingErr = mp_normDeg(target.headingDeg - angleDeg);
  if (fabsf(headingErr) > MP_ANGLE_TOL) {
    float rate = mp_turnRateFromErr(fabsf(headingErr));
    String cmd = (headingErr > 0) ? "R" + String((int)rate) : "L" + String((int)rate);
    if (MP_DEBUG) {
      Serial.printf("[MP] idx=%u atPoint headingTgt=%.1f ang=%.1f err=%.1f rate=%.1f -> %s\n",
                    (unsigned)mp_idx, target.headingDeg, angleDeg, headingErr, rate, cmd.c_str());
    }
    return cmd;
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

