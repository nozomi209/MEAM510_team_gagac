/*
 * VIVE Utilities Library Implementation
 * Helper functions for VIVE coordinate processing and calculations
 */

#include "vive_utils.h"

// Fast arctangent approximation using polynomial
float fastArctan(float x) {
    float xSquared = x * x;
    // Polynomial approximation: x * (0.995354 + x^2 * (-0.288679 + x^2 * 0.079331))
    return x * (0.995354 + xSquared * (-0.288679 + xSquared * 0.079331));
}

// Fast atan2 implementation
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

// Median filter for smoothing coordinate data
uint32_t medianFilter(uint32_t a, uint32_t b, uint32_t c) {
    if ((a <= b) && (a <= c)) {
        return (b <= c) ? b : c;
    } else if ((b <= a) && (b <= c)) {
        return (a <= c) ? a : c;
    } else {
        return (a <= b) ? a : b;
    }
}

// Process VIVE tracker data with filtering and calibration
void processViveData(ViveTracker& tracker, uint16_t& x, uint16_t& y) {
    // Static variables for filtering history
    static uint16_t x0, y0;
    static uint16_t x1, y1;
    static uint16_t x2, y2;
    
    if (tracker.getStatus() == VIVE_STATUS_RECEIVING) {
        // Update filter history
        x2 = x1;
        y2 = y1;
        x1 = x0;
        y1 = y0;
        
        // Get raw coordinates and apply calibration
        x0 = tracker.getXCoordinate() - VIVE_CALIBRATION_X;
        y0 = tracker.getYCoordinate() - VIVE_CALIBRATION_Y;
        
        // Apply median filter for noise reduction
        x = medianFilter(x0, x1, x2);
        y = medianFilter(y0, y1, y2);
        
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

