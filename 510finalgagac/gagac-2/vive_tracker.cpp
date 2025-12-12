/*
 * vive_tracker.cpp — Vive Tracker 接口实现
 *
 * 本文件主要包含：
 * - 中断 wrapper：`viveInterruptHandler(void* trackerInstance)`
 *   用于把 ESP32 的 `attachInterruptArg` 回调路由到指定 `ViveTracker` 实例
 *
 * - `ViveTracker` 的实现：
 *   - 通过 GPIO CHANGE 中断记录上升/下降沿时间
 *   - 判别同步脉冲与扫描脉冲类型（J=Y, K=X）
 *   - 在有效接收状态下解析脉宽并更新坐标
 *
 * 线程/中断安全：
 * - 使用 `portMUX_TYPE viveMutex` 做 ISR 临界区保护，避免共享变量读写撕裂。
 */

#include "vive_tracker.h"

// Mutex for interrupt-safe operations（保护中断共享变量）
portMUX_TYPE viveMutex = portMUX_INITIALIZER_UNLOCKED;

// Global interrupt handler wrapper（封装 attachInterruptArg 回调）
void IRAM_ATTR viveInterruptHandler(void* trackerInstance) {
    portENTER_CRITICAL_ISR(&viveMutex);
    static_cast<ViveTracker*>(trackerInstance)->interruptHandler(micros());
    portEXIT_CRITICAL_ISR(&viveMutex);
}

// Constructor（指定信号引脚）
ViveTracker::ViveTracker(int pin) {
    m_signalPin = pin;
    m_trackingStatus = VIVE_STATUS_NO_SIGNAL;
    m_xCoordinate = 0;
    m_yCoordinate = 0;
    m_risingEdgeTime = 0;
    m_fallingEdgeTime = 0;
    m_currentPulseType = 0;
    m_sweepWidthThreshold = 50;
    m_lastFallingEdge = 0;
    m_spuriousPulseCount = 0;
}

// Initialize with default pin
void ViveTracker::initialize() {
    pinMode(m_signalPin, INPUT);
    startTracking();
}

// Initialize with new pin（重新指定引脚）
void ViveTracker::initialize(int pin) {
    m_signalPin = pin;
    pinMode(m_signalPin, INPUT);
    startTracking();
}

// Start tracking (attach interrupt)
void ViveTracker::startTracking() {
    attachInterruptArg(digitalPinToInterrupt(m_signalPin), 
                      viveInterruptHandler, 
                      this, 
                      CHANGE);
}

// Stop tracking (detach interrupt)
void ViveTracker::stopTracking() {
    detachInterrupt(digitalPinToInterrupt(m_signalPin));
}

// Interrupt service routine：记录脉冲边沿时间，收到同步后开始解析扫描脉冲
void IRAM_ATTR ViveTracker::interruptHandler(uint32_t microsecondTime) {
    if (digitalRead(m_signalPin) == HIGH) {
        m_risingEdgeTime = microsecondTime;
    } else {
        m_fallingEdgeTime = microsecondTime;
    }
    
    // Process pulse if we're receiving valid signals
    if (m_trackingStatus == VIVE_STATUS_RECEIVING) {
        analyzePulse();
    }
}

// Check if pulse is K-type（K 脉冲对应 X）
bool ViveTracker::isKPulseType(uint32_t pulseWidth) {
    // K-type pulses have specific width ranges
    if (pulseWidth < 75) return false;
    if (pulseWidth > 85 && pulseWidth < 95) return false;
    if (pulseWidth > 106 && pulseWidth < 117) return false;
    if (pulseWidth > 127 && pulseWidth < 137) return false;
    return true;
}

// Check if pulse is J-type（J 脉冲对应 Y）
bool ViveTracker::isJPulseType(uint32_t pulseWidth) {
    // J-type pulses are the complement of K-type
    if (pulseWidth < 75) return true;
    if (pulseWidth > 85 && pulseWidth < 95) return true;
    if (pulseWidth > 106 && pulseWidth < 117) return true;
    if (pulseWidth > 127 && pulseWidth < 137) return true;
    return false;
}

// Analyze incoming pulse：区分同步/扫描，更新坐标或判定丢失
void ViveTracker::analyzePulse() {
    if (m_lastFallingEdge != m_fallingEdgeTime) {
        uint32_t pulseWidth = m_fallingEdgeTime - m_risingEdgeTime;
        
        // Check if this is a sweep pulse (narrow) or sync pulse (wide)
        if (pulseWidth > m_sweepWidthThreshold) {
            // This is a sync pulse
            if (pulseWidth > 140) {
                // Pulse too long, likely spurious
                m_currentPulseType = 0;
                m_spuriousPulseCount++;
            } else if (isKPulseType(pulseWidth)) {
                m_currentPulseType = VIVE_PULSE_TYPE_K;
            } else if (isJPulseType(pulseWidth)) {
                m_currentPulseType = VIVE_PULSE_TYPE_J;
            }
        } else {
            // This is a sweep pulse - extract coordinate
            if (m_currentPulseType == VIVE_PULSE_TYPE_J) {
                m_yCoordinate = m_risingEdgeTime - m_lastFallingEdge;
            }
            if (m_currentPulseType == VIVE_PULSE_TYPE_K) {
                m_xCoordinate = m_risingEdgeTime - m_lastFallingEdge;
            }
            m_spuriousPulseCount = 0;
        }
        
        // Check for too many spurious pulses
        if (m_spuriousPulseCount > 60) {
            m_trackingStatus = VIVE_STATUS_SYNC_ONLY;
        }
        
        m_lastFallingEdge = m_fallingEdgeTime;
    }
}

// Get X coordinate
uint16_t ViveTracker::getXCoordinate() {
    return m_xCoordinate;
}

// Get Y coordinate
uint16_t ViveTracker::getYCoordinate() {
    return m_yCoordinate;
}

// Get tracking status
int ViveTracker::getStatus() {
    return m_trackingStatus;
}

// Synchronize with VIVE base stations：统计一定时间内脉冲数，推断状态
uint32_t ViveTracker::synchronize(int expectedPulseCount) {
    uint32_t startTime = millis();
    uint32_t lastFalling = m_fallingEdgeTime;
    int pulseCount = 0;
    
    // Wait for expected number of pulses within time window
    // VIVE base stations operate at 120Hz, so we wait for (expectedPulseCount+1) cycles
    uint32_t timeout = (expectedPulseCount + 1) * 1000 / 120;
    
    while ((millis() - startTime) < timeout) {
        if (lastFalling != m_fallingEdgeTime) {
            lastFalling = m_fallingEdgeTime;
            pulseCount++;
        }
        yield();
    }
    
    // Determine status based on pulse count
    if (pulseCount == 0) {
        m_trackingStatus = VIVE_STATUS_NO_SIGNAL;
    } else if (pulseCount < 2 * expectedPulseCount) {
        m_trackingStatus = VIVE_STATUS_SYNC_ONLY;
    } else {
        m_trackingStatus = VIVE_STATUS_RECEIVING;
    }
    
    return m_trackingStatus;
}

