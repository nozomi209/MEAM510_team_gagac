// Minimal ToF stub to satisfy linker; replace with real sensor driver if available.
#include <Arduino.h>

void ToF_init() {
    // TODO: initialize ToF sensors here
}

// Read three ToF distances; return true if data valid.
bool ToF_read(uint16_t d[3]) {
    d[0] = d[1] = d[2] = 0;
    return false; // no data in stub
}

// Compatibility shim for legacy symbol
bool ToF_readPt(uint16_t d[3]) {
    return ToF_read(d);
}

