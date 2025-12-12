// 手动规划模板（含朝向与撞击动作）：实现已迁移至 manual_planner.cpp
// 这样可避免 Arduino 自动生成函数原型导致自定义类型不可见的问题。
// 使用方法：
// 1) mp_setDefaultRoute() 或 mp_loadRouteString() 载入路点
// 2) 循环调用 mp_step(x,y,angle) 获取动作指令
// 3) mp_planTargets(...) 可按起点/目标列表规划 1~3 个点

#include "manual_planner.h"

