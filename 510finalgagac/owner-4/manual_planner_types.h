/*
 * manual_planner_types.h — 规划/导航模块的数据类型定义
 *
 * 本文件定义了 `manual_planner.cpp/.h` 使用的基础结构体：
 * - `PlannerPoint` / `PlannerSegment`：用于轴对齐路径规划的点/线段表达
 * - `ObstacleBox`：矩形障碍框（left/right/top/bottom）
 * - `Waypoint`：可执行路点（x,y,headingDeg,bumps）
 *
 * 说明：
 * - 之所以单独拆出 types，是为了避免 Arduino 工程在 .ino 自动生成函数原型时，
 *   出现“自定义类型不可见”的编译问题；Owner 侧多个文件可统一 include 该头文件。
 */

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

