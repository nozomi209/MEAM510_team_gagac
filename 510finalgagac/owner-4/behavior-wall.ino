// behavior-wall.ino
// wall following logic

#include <Arduino.h>

static bool isValid(uint16_t d) {
    return (d > 1 && d < 3000);
}

// thresholds
const uint16_t FRONT_TURN_TH   = 450;   // 45cm turn
const uint16_t FRONT_BACKUP_TH = 120;   // 12cm backup

const float RIGHT_TARGET        = 80.0f; // 8cm ideal
const float RIGHT_FAR_HARD      = 140.0f; // >14cm is too far
const float RIGHT_CORNER_HARD   = 60.0f;  //<6cm dangerous

// speeds
const uint8_t SPEED_FWD        = 30;
const uint8_t SPEED_BACK       = 35;

const uint8_t TURN_PIVOT       = 130;   //hard turn
const uint8_t TURN_CORRECT     = 40;
const uint8_t TURN_GENTLE      = 30;    //find wall speed
const uint8_t TURN_TINY        = 10;

const float ANGLE_ERR_TH       = 10.0f;

String decideWallFollowing(uint16_t F_raw, uint16_t R1_raw, uint16_t R2_raw) {

    bool hasF  = isValid(F_raw);
    bool hasR1 = isValid(R1_raw);
    bool hasR2 = isValid(R2_raw);

    // fix sensor overflow issues
    float F  = (hasF && F_raw < 5000) ? F_raw  : 5000.0f;
    float R1 = hasR1 ? R1_raw : RIGHT_TARGET;
    float R2 = hasR2 ? R2_raw : RIGHT_TARGET;
    
    // 0. backup if too close
    if (F < FRONT_BACKUP_TH || R1 < 50) {
        return "B" + String(SPEED_BACK);
    }

    // 1. front wall
    if (F <= FRONT_TURN_TH) {
        return "L" + String(TURN_PIVOT); 
    }

    // 2. right too close
    // head angling in
    if (R1 < R2 - 10.0f && R1 < RIGHT_TARGET) {
         return "L" + String(TURN_CORRECT); 
    }
    // absolute distance check
    if (R1 < RIGHT_CORNER_HARD || R2 < RIGHT_CORNER_HARD) {
 
        return "L" + String(TURN_CORRECT); 
    }

    // 3. right too far
    if (R2 > RIGHT_FAR_HARD && R1 >= R2) {
        return "R" + String(TURN_GENTLE); 
    }

    // 4. minor angle adjust
    float e_angle = R1 - R2; 

    if (e_angle < -ANGLE_ERR_TH) {
        return "L" + String(TURN_TINY); 
    }
    if (e_angle > ANGLE_ERR_TH) {
        return "R" + String(TURN_TINY); 
    }

    // 5. straight
    return "F" + String(SPEED_FWD);
}