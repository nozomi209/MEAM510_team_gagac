#include "vive510.h"
#include "vivelib.h"

float arctanStar(float x) {
  float xsq = x * x;
  return x * (0.995354 + xsq * (-0.288679 + xsq * 0.079331));
}

float atan2Fast(float y, float x) {
  float angle;
  if (abs(y) <= abs(x)) {
    angle = arctanStar(y / x);
  } else if ((abs(y) > abs(x)) && ((x / y) >= 0)) {
    angle = (PI / 2.0) - arctanStar(x / y);
  } else if ((abs(y) > abs(x)) && ((x / y) < 0)) {
    angle = (-PI / 2.0) - arctanStar(x / y);
  }

  if (((x >= 0.0) && (y >= 0.0)) || ((x >= 0.0) && (y < 0.0))) {
    return angle;
  } else if ((x < 0.0) && (y >= 0.0)) {
    return angle + PI;
  } else if ((x < 0.0) && (y < 0.0)) {
    return angle - PI;
  }
}

uint32_t med3filt(uint32_t a, uint32_t b, uint32_t c) {
  if ((a <= b) && (a <= c))
    return (b <= c) ? b : c;
  else if ((b <= a) && (b <= c))
    return (a <= c) ? a : c;
  else
    return (a <= b) ? a : b;
}

void processVive(Vive510 &tracker, uint16_t &x, uint16_t &y) {
  static uint16_t x0, y0, oldx1, oldx2, oldy1, oldy2;

  if (tracker.status() == VIVE_RECEIVING) {
    oldx2 = oldx1;
    oldy2 = oldy1;
    oldx1 = x0;
    oldy1 = y0;

    x0 = tracker.xCoord() - CALIBRATIONX;
    y0 = tracker.yCoord() - CALIBRATIONY;

    x = med3filt(x0, oldx1, oldx2);
    y = med3filt(y0, oldy1, oldy2);

    x = constrain(x, 1000, 8000);
    y = constrain(y, 1000, 8000);

  } else {
    x = 0;
    y = 0;
    tracker.sync(5);
  }
}
