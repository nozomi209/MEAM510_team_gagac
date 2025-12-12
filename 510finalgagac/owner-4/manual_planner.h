/*
 * manual_planner.h — Owner 侧规划/导航模块接口（对应 manual_planner.cpp）
 *
 * 作用：
 * - 对外提供“装载路径/规划目标点/执行一步/停止/调参”等 API，
 *   供 `owner-4.ino` 在不同模式下调用，并把返回的 "F/L/R/S" 指令下发到 Servant。
 *
 * 典型使用方式：
 * 1) 规划/装载路径：
 *    - `mp_planTargets(startX,startY, targets, count, margin)`：规划 1~3 个目标点（轴对齐绕障）
 *    - 或 `mp_setRoute(wp, cnt)` / `mp_loadRouteString()`：直接装载路点序列
 * 2) 在 loop 中持续调用：
 *    - `String cmd = mp_step(viveX, viveY, viveAngle);`
 * 3) 查询与停止：
 *    - `mp_isActive()` / `mp_stop()`
 * 4) 运行时调参（网页/串口通过 Owner 转发 `MP_PARAM:`）：
 *    - `mp_updateParam("MP_SPEED_FAR", 70)` 等
 */

#pragma once

#include <Arduino.h>
#include "manual_planner_types.h"

// 参数/状态（供其它模块读取）
extern float MP_PATH_MARGIN;
extern uint8_t mp_routeCount;

// 配置接口
void mp_setObstacleBox(float left, float right, float top, float bottom);
void mp_setObstacleEnabled(bool enabled);
void mp_setPathMargin(float margin);
void mp_setBoundary(float xmin, float xmax, float ymax, float ymin);
void mp_setBoundaryEnabled(bool enabled);

// 预设/外部路径
void mp_setDefaultRoute();
void mp_setRoute(const Waypoint* wp, uint8_t cnt);
bool mp_loadRouteString(const String& s);

// 运行时控制
void mp_stop();
bool mp_isActive();
void mp_updateParam(const String& key, float val);
String mp_step(float x, float y, float angleDeg);

// 新模式：轴对齐规划（可规划 1~3 个目标点）
bool mp_planTargets(float startX, float startY, const float targetList[][2], uint8_t targetCount, float margin);

