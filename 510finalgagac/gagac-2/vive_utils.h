/*
 * vive_utils.h — Vive 工具函数（角度近似 + 坐标处理）
 *
 * 本文件主要提供：
 * - 快速 atan/atan2 近似：用于降低 MCU 上的计算成本
 * - 坐标处理 `processViveData(tracker, x, y)`：
 *   - 读取 tracker 坐标
 *   - 做坐标校准偏移（VIVE_CALIBRATION_X/Y）
 *   - 做限幅（VIVE_X/Y_MIN/MAX）
 *   - 在无信号时清零并触发 `synchronize()`（基础容错）
 *
 * 说明：
 * - 更复杂的滤波（中值/离群/EMA）曾在该文件中使用过；当前实现更偏“保真”以便调试。
 * - `gagac-2.ino` 在计算中心坐标与朝向前，会先对前后两个 tracker 分别调用 `processViveData()`。
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

// Coordinate constraints 坐标限幅，避免脏数据（调低下限防止被锁 1000）
#define VIVE_X_MIN 0
#define VIVE_X_MAX 8000
#define VIVE_Y_MIN 0
#define VIVE_Y_MAX 8000

// Filtering parameters 滤波参数
// 若相邻帧跳变超过该阈值则视为离群，丢弃本帧（单位 mm）
#define VIVE_OUTLIER_THRESHOLD 800
// 一阶低通系数 alpha = VIVE_EMA_ALPHA_NUM / VIVE_EMA_ALPHA_DEN
#define VIVE_EMA_ALPHA_NUM 1
#define VIVE_EMA_ALPHA_DEN 4

// Fast arctangent approximation 快速 arctan 近似
float fastArctan(float x);

// Fast atan2 implementation 快速 atan2 近似
float fastAtan2(float y, float x);

// Median filter for coordinate smoothing 三值中值滤波
uint32_t medianFilter(uint32_t a, uint32_t b, uint32_t c);

// Process VIVE tracker data with filtering and calibration
void processViveData(ViveTracker& tracker, uint16_t& x, uint16_t& y);

#endif // VIVE_UTILS_H

