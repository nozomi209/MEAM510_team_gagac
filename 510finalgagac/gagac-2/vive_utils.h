/*
 * VIVE Utilities Library
 * Helper functions for VIVE coordinate processing and calculations
 */

#ifndef VIVE_UTILS_H
#define VIVE_UTILS_H

#include <arduino.h>
#include "vive_tracker.h"

// Mathematical constants
#define VIVE_PI 3.14159265

// Calibration offsets (adjust based on your setup)
#define VIVE_CALIBRATION_X 70
#define VIVE_CALIBRATION_Y 500

// Coordinate constraints
#define VIVE_X_MIN 1000
#define VIVE_X_MAX 8000
#define VIVE_Y_MIN 1000
#define VIVE_Y_MAX 8000

// Fast arctangent approximation
float fastArctan(float x);

// Fast atan2 implementation
float fastAtan2(float y, float x);

// Median filter for coordinate smoothing
uint32_t medianFilter(uint32_t a, uint32_t b, uint32_t c);

// Process VIVE tracker data with filtering and calibration
void processViveData(ViveTracker& tracker, uint16_t& x, uint16_t& y);

#endif // VIVE_UTILS_H

