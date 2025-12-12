/*
 * manual_planner.ino — Arduino “占位/入口”文件（实现迁移说明）
 *
 * 说明：
 * - 实际实现已迁移至 `manual_planner.cpp`，该 .ino 仅用于：
 *   1) 作为 Arduino 工程的编译入口之一
 *   2) 避免 Arduino 对 .ino 自动生成函数原型时，导致自定义类型/声明顺序问题
 *
 * 你真正需要看的/改的代码在：
 * - `manual_planner.cpp`：规划与执行核心
 * - `manual_planner.h` / `manual_planner_types.h`：对外接口与类型
 *
 * 常用 API：
 * - `mp_planTargets(...)` / `mp_setRoute(...)`：装载路径
 * - `mp_step(viveX, viveY, viveAngle)`：每个 loop 输出一条 "F/L/R/S" 指令
 * - `mp_stop()` / `mp_isActive()`：停止与查询
 */

#include "manual_planner.h"

