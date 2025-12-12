/*
 * vive_utils.cpp — Vive 工具函数实现
 *
 * 本文件实现：
 * - `fastArctan()` / `fastAtan2()`：多项式近似的 atan/atan2（速度优先）
 * - `medianFilter()`：三值中值滤波（当前主要用于保留工具能力）
 * - `processViveData()`：Vive 坐标读取 + 校准 + 限幅 + 基础容错（无信号时重同步）
 *
 * 调用关系：
 * - `gagac-2.ino` 会周期性对两个 tracker 调用 `processViveData()`，
 *   再计算中心点与朝向角，并通过 UART 上报给 Owner。
 */

#include "vive_utils.h"

// Fast arctangent approximation using polynomial 多项式 arctan 近似
float fastArctan(float x) {
    float xSquared = x * x;
    // Polynomial approximation: x * (0.995354 + x^2 * (-0.288679 + x^2 * 0.079331))
    return x * (0.995354 + xSquared * (-0.288679 + xSquared * 0.079331));
}

// Fast atan2 implementation：按象限选择近似，速度优先
float fastAtan2(float y, float x) {
    float angle;
    
    // Use different approximations based on quadrant
    if (abs(y) <= abs(x)) {
        angle = fastArctan(y / x);
    } else if ((abs(y) > abs(x)) && ((x / y) >= 0)) {
        angle = (VIVE_PI / 2.0) - fastArctan(x / y);
    } else if ((abs(y) > abs(x)) && ((x / y) < 0)) {
        angle = (-VIVE_PI / 2.0) - fastArctan(x / y);
    }
    
    // Adjust for correct quadrant
    if (((x >= 0.0) && (y >= 0.0)) || ((x >= 0.0) && (y < 0.0))) {
        return angle;
    } else if ((x < 0.0) && (y >= 0.0)) {
        return angle + VIVE_PI;
    } else if ((x < 0.0) && (y < 0.0)) {
        return angle - VIVE_PI;
    }
    
    return angle;
}

// Median filter for smoothing coordinate data 三值中值滤波，抑制跳变
uint32_t medianFilter(uint32_t a, uint32_t b, uint32_t c) {
    if ((a <= b) && (a <= c)) {
        return (b <= c) ? b : c;
    } else if ((b <= a) && (b <= c)) {
        return (a <= c) ? a : c;
    } else {
        return (a <= b) ? a : b;
    }
}

// Process VIVE tracker data with minimal filtering
// - 状态正常：原始 -> 校准 -> 限幅（去掉中值/离群/EMA，方便直接看原始）
// - 状态异常：清零并尝试重新同步
void processViveData(ViveTracker& tracker, uint16_t& x, uint16_t& y) {
    if (tracker.getStatus() == VIVE_STATUS_RECEIVING) {
        // 原始坐标 + 校准（防止减偏移下溢）
        int32_t rawX = (int32_t)tracker.getXCoordinate() - VIVE_CALIBRATION_X;
        int32_t rawY = (int32_t)tracker.getYCoordinate() - VIVE_CALIBRATION_Y;
        if (rawX < 0) rawX = 0;
        if (rawY < 0) rawY = 0;

        x = (uint16_t)rawX;
        y = (uint16_t)rawY;
        
        // Constrain to valid range
        x = constrain(x, VIVE_X_MIN, VIVE_X_MAX);
        y = constrain(y, VIVE_Y_MIN, VIVE_Y_MAX);
    } else {
        // No valid signal - reset coordinates
        x = 0;
        y = 0;
        // Attempt to resynchronize
        tracker.synchronize(5);
    }
}

