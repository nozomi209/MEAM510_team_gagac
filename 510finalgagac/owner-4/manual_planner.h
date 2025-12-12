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

