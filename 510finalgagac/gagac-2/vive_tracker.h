/*
 * VIVE Tracker Interface Library
 * For ESP32-based VIVE tracking system
 * 
 * This library handles VIVE lighthouse pulse signal processing
 * to determine X and Y coordinates
 */

#ifndef VIVE_TRACKER_H
#define VIVE_TRACKER_H

#include <arduino.h>

// VIVE tracking status codes
#define VIVE_STATUS_NO_SIGNAL    0
#define VIVE_STATUS_SYNC_ONLY    1
#define VIVE_STATUS_RECEIVING    2

// Pulse type identifiers
#define VIVE_PULSE_TYPE_J        1
#define VIVE_PULSE_TYPE_K        2

class ViveTracker {
private:
    // Pin configuration
    int m_signalPin;
    
    // Timing data (updated by interrupt)
    volatile uint32_t m_risingEdgeTime;
    volatile uint32_t m_fallingEdgeTime;
    
    // Coordinate data
    uint16_t m_xCoordinate;
    uint16_t m_yCoordinate;
    
    // Status tracking
    int m_trackingStatus;
    int m_currentPulseType;
    
    // Pulse processing parameters
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

