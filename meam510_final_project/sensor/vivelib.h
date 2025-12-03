#ifndef VIVELIB
#define VIVELIB

#include <arduino.h>
#include <vl53l4cx_class.h>

#define PI 3.14159265
#define CALIBRATIONX 70
#define CALIBRATIONY 500

float arctanStar(float x);
float atan2Fast(float y, float x);
uint32_t med3filt(uint32_t a, uint32_t b, uint32_t c);
void processVive(Vive510& tracker, uint16_t& x, uint16_t& y);

#endif
