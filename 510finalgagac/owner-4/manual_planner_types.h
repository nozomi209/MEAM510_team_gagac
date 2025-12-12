#pragma once

#include <Arduino.h>

struct PlannerPoint { float x; float y; };
struct PlannerSegment { PlannerPoint a; PlannerPoint b; };
struct ObstacleBox { float left; float right; float top; float bottom; };

struct Waypoint {
  float x;
  float y;
  float headingDeg;  // 目标朝向
  uint8_t bumps;     // 到达后向前撞击次数（0 为不撞）
};

