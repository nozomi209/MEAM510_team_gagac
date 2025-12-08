// behavior-wall.ino
// wall following logic

#include <Arduino.h>

static bool isValid(uint16_t d) {
    return (d > 1 && d < 3000);
}


// 1. dis
const uint16_t FRONT_TURN_TH   = 220;   
const uint16_t FRONT_BACKUP_TH = 30;    

const float RIGHT_TARGET        = 80.0f;  
const float RIGHT_FAR_HARD      = 140.0f; 
const float RIGHT_CORNER_HARD   = 60.0f;  

const float RIGHT_LOST_WALL     = 300.0f; 

// 2. 速度与转向力度
const uint8_t SPEED_FWD        = 30;
const uint8_t SPEED_BACK       = 25; 

// spin left
const uint8_t TURN_SPIN        = 105;   

const uint8_t TURN_CORRECT     = 30;    
const uint8_t TURN_GENTLE      = 30;    
const uint8_t TURN_HARD_FIND   = 60;    

const uint8_t TURN_TINY        = 10;
const float ANGLE_ERR_TH       = 10.0f;

// time limit for back up
const unsigned long MAX_BACKUP_MS = 600; 

String decideWallFollowing(uint16_t F_raw, uint16_t R1_raw, uint16_t R2_raw) {
    static unsigned long backupStartTime = 0;
    static bool isBackingUp = false;

    bool hasF  = isValid(F_raw);
    bool hasR1 = isValid(R1_raw);
    bool hasR2 = isValid(R2_raw);

    if (!hasF && !hasR1 && !hasR2) {
        return "S"; 
    }

    // fix sensor overflow issues
    float F  = (hasF && F_raw < 5000) ? F_raw : 5000.0f;
    float R1 = hasR1 ? R1_raw : 5000.0f;
    float R2 = hasR2 ? R2_raw : 5000.0f; 

    // 出胡同?
    bool isExitingAlley = (R1 > 200.0f); 


    // 0. Panic Mode backup if <3cm
    if (F < FRONT_BACKUP_TH) {
        if (!isBackingUp) {
            isBackingUp = true;
            backupStartTime = millis();
        }
        if (millis() - backupStartTime > MAX_BACKUP_MS) {
            return "L" + String(TURN_SPIN);
        }
        return "B" + String(SPEED_BACK);
    } 
    else {
        isBackingUp = false;
    }

    // 1. front turn left if wall
    if (F <= FRONT_TURN_TH) {
        return "L" + String(TURN_SPIN);
    }

    // 2. right too close
    if (!isExitingAlley) {
        if (R1 < R2 - 10.0f && R1 < RIGHT_TARGET) {
             return "L" + String(TURN_CORRECT);
        }
        if (R1 < RIGHT_CORNER_HARD || R2 < RIGHT_CORNER_HARD) {
            return "L" + String(TURN_CORRECT);
        }
    }

    // 3. right find wall
    

    if (R1 > RIGHT_LOST_WALL) {
        if (R2 > RIGHT_LOST_WALL) {
            // 前后都看不到墙 -> 说明在空地上 -> 直行
            return "F" + String(SPEED_FWD);
        } else {
            // 头出来了，屁股还在说明正在出胡同 jiu猛转
            return "R" + String(TURN_HARD_FIND); 
        }
    }
    
    // B: 微调 
    if (R2 > RIGHT_FAR_HARD && R1 >= R2) {
        return "R" + String(TURN_GENTLE);
    }

    // 4. 直走微调
    float e_angle = R1 - R2;
    if (e_angle < -ANGLE_ERR_TH) {
        return "L" + String(TURN_TINY);
    }
    if (e_angle > ANGLE_ERR_TH) {
        return "R" + String(TURN_TINY);
    }

    // 5. 直行
    return "F" + String(SPEED_FWD);
}