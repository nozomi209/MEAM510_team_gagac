// behavior-wall.ino
// wall following logic
// motor wires are reversed, so L=Right, R=Left

#include <Arduino.h>

static bool isValid(uint16_t d) {
    return (d > 1 && d < 3000);
}

// thresholds
const uint16_t FRONT_TURN_TH   = 450;   // 45cm turn
const uint16_t FRONT_BACKUP_TH = 120;   // 12cm backup

const float LEFT_TARGET        = 100.0f; // 10cm ideal
const float LEFT_FAR_HARD      = 140.0f; // >14cm is too far
const float LEFT_CORNER_HARD   = 60.0f;  //<6cm dangerous

// speeds
const uint8_t SPEED_FWD        = 30;
const uint8_t SPEED_BACK       = 35;

const uint8_t TURN_PIVOT       = 160;   //hard turn
const uint8_t TURN_CORRECT     = 60;
const uint8_t TURN_GENTLE      = 30;    //find wall speed
const uint8_t TURN_TINY        = 10;

const float ANGLE_ERR_TH       = 10.0f;

String decideWallFollowing(uint16_t F_raw, uint16_t L1_raw, uint16_t L2_raw) {

    bool hasF  = isValid(F_raw);
    bool hasL1 = isValid(L1_raw);
    bool hasL2 = isValid(L2_raw);

    // fix sensor overflow issues
    float F  = (hasF && F_raw < 5000) ? F_raw  : 5000.0f;
    float L1 = hasL1 ? L1_raw : LEFT_TARGET;
    float L2 = hasL2 ? L2_raw : LEFT_TARGET;
    
    // 0. backup if too close
    if (F < FRONT_BACKUP_TH || L1 < 50) {
        return "B" + String(SPEED_BACK);
    }

    // 1. front wall
    if (F <= FRONT_TURN_TH) {
        // send L -> physical Right (pivot)
        return "L" + String(TURN_PIVOT); 
    }

    // 2. left too close
    // head angling in
    if (L1 < L2 - 10.0f && L1 < LEFT_TARGET) {
         return "L" + String(TURN_CORRECT); 
    }
    // absolute distance check
    if (L1 < LEFT_CORNER_HARD || L2 < LEFT_CORNER_HARD) {
        return "L" + String(TURN_CORRECT); 
    }

    // 3. left too far
    if (L2 > LEFT_FAR_HARD && L1 >= L2) {
        return "R" + String(TURN_GENTLE); // send R -> physical Left
    }

    // 4. minor angle adjust
    float e_angle = L1 - L2; 

    if (e_angle < -ANGLE_ERR_TH) {
        return "L" + String(TURN_TINY); 
    }
    if (e_angle > ANGLE_ERR_TH) {
        return "R" + String(TURN_TINY); 
    }

    // 5. straight
    return "F" + String(SPEED_FWD);
}