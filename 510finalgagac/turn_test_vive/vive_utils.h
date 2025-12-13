/*
 * vive_utils.h — Vive 工具函数（角度近似 + 坐标处理）
 *
 * 说明：
 * - 该文件来自现有工程 gagac-2，用于 turn_test_vive 独立草图直接编译
 */

#ifndef VIVE_UTILS_H
#define VIVE_UTILS_H

#include <arduino.h>
#include "vive_tracker.h"

// Mathematical constants
#define VIVE_PI 3.14159265

// Calibration offsets (adjust based on your setup) 坐标校准偏移
#define VIVE_CALIBRATION_X 70
#define VIVE_CALIBRATION_Y 500

// Coordinate constraints 坐标限幅
#define VIVE_X_MIN 0
#define VIVE_X_MAX 8000
#define VIVE_Y_MIN 0
#define VIVE_Y_MAX 8000

// Fast arctangent approximation 快速 arctan 近似
float fastArctan(float x);

// Fast atan2 implementation 快速 atan2 近似
float fastAtan2(float y, float x);

// Median filter for coordinate smoothing 三值中值滤波
uint32_t medianFilter(uint32_t a, uint32_t b, uint32_t c);

// Process VIVE tracker data with calibration/constraints
void processViveData(ViveTracker& tracker, uint16_t& x, uint16_t& y);

#endif // VIVE_UTILS_H


