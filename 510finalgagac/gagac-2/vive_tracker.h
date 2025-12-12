/*
 * vive_tracker.h — Vive Tracker 接口库（ESP32）
 *
 * 作用：
 * - 对单个 Vive Tracker 信号脚进行中断采样
 * - 解析 Lighthouse 基站的同步/扫描脉冲（J/K），计算出 X/Y 坐标
 * - 提供状态机：无信号 / 仅同步 / 正常接收
 *
 * 典型调用：
 * - `ViveTracker t(pin); t.initialize();`
 * - 在主循环读取：`t.getXCoordinate()` / `t.getYCoordinate()` / `t.getStatus()`
 * - 可用 `synchronize(pulseCount)` 做快速同步（无信号时恢复）
 *
 * 注意：
 * - 中断回调通过 `attachInterruptArg` 绑定到实例（见 vive_tracker.cpp 的 wrapper）
 * - 坐标单位与 Vive Lighthouse 脉冲解码一致（在上层再做校准/滤波）
 */

#ifndef VIVE_TRACKER_H
#define VIVE_TRACKER_H

#include <arduino.h>

// VIVE tracking status codes（跟踪状态）
#define VIVE_STATUS_NO_SIGNAL    0
#define VIVE_STATUS_SYNC_ONLY    1
#define VIVE_STATUS_RECEIVING    2

// Pulse type identifiers（脉冲类型：J=Y轴，K=X轴）
#define VIVE_PULSE_TYPE_J        1
#define VIVE_PULSE_TYPE_K        2

class ViveTracker {
private:
    // Pin configuration（信号输入脚）
    int m_signalPin;
    
    // Timing data (updated by interrupt)（中断记录脉冲时间）
    volatile uint32_t m_risingEdgeTime;
    volatile uint32_t m_fallingEdgeTime;
    
    // Coordinate data（解析出的坐标）
    uint16_t m_xCoordinate;
    uint16_t m_yCoordinate;
    
    // Status tracking（状态机）
    int m_trackingStatus;
    int m_currentPulseType;
    
    // Pulse processing parameters（滤除异常脉冲的阈值/计数）
    int m_sweepWidthThreshold;
    uint32_t m_lastFallingEdge;
    int m_spuriousPulseCount;
    
    // Internal methods
    bool isKPulseType(uint32_t pulseWidth);
    bool isJPulseType(uint32_t pulseWidth);
    void analyzePulse();
    
public:
    // Constructor
    ViveTracker(int pin);
    
    // Initialization
    void initialize();
    void initialize(int pin);
    
    // Control
    void startTracking();
    void stopTracking();
    
    // Data access
    uint16_t getXCoordinate();
    uint16_t getYCoordinate();
    int getStatus();
    
    // Synchronization
    uint32_t synchronize(int pulseCount);
    
    // Interrupt service routine (must be public for attachInterrupt)
    void interruptHandler(uint32_t microsecondTime);
};

// Global interrupt handler wrapper (needed for ESP32 attachInterrupt)
void viveInterruptHandler(void* trackerInstance);

#endif // VIVE_TRACKER_H

