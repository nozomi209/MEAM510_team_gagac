/*
 * VIVE Tracker Interface Library Implementation
 * For ESP32-based VIVE tracking system
 */

#include "vive_tracker.h"

// Mutex for interrupt-safe operations
portMUX_TYPE viveMutex = portMUX_INITIALIZER_UNLOCKED;

// Global interrupt handler wrapper
void IRAM_ATTR viveInterruptHandler(void* trackerInstance) {
    portENTER_CRITICAL_ISR(&viveMutex);
    static_cast<ViveTracker*>(trackerInstance)->interruptHandler(micros());
    portEXIT_CRITICAL_ISR(&viveMutex);
}

// Constructor
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

// Initialize with new pin
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

// Interrupt service routine
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

// Check if pulse is K-type
bool ViveTracker::isKPulseType(uint32_t pulseWidth) {
    // K-type pulses have specific width ranges
    if (pulseWidth < 75) return false;
    if (pulseWidth > 85 && pulseWidth < 95) return false;
    if (pulseWidth > 106 && pulseWidth < 117) return false;
    if (pulseWidth > 127 && pulseWidth < 137) return false;
    return true;
}

// Check if pulse is J-type
bool ViveTracker::isJPulseType(uint32_t pulseWidth) {
    // J-type pulses are the complement of K-type
    if (pulseWidth < 75) return true;
    if (pulseWidth > 85 && pulseWidth < 95) return true;
    if (pulseWidth > 106 && pulseWidth < 117) return true;
    if (pulseWidth > 127 && pulseWidth < 137) return true;
    return false;
}

// Analyze incoming pulse
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

// Synchronize with VIVE base stations
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

