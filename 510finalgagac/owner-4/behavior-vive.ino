// behavior-vive.ino
// Vive-based autonomous navigation logic
// Uses Vive tracking data for position and orientation

#include <Arduino.h>

// Navigation parameters
const float ANGLE_TOLERANCE = 20.0f;      // degrees - angle error threshold for turning
const float DISTANCE_TOLERANCE = 5.0f;    // mm - distance threshold to consider reached

// Fast atan2 implementation (from vive_utils)
float fastAtan2(float y, float x) {
    float angle;
    
    if (abs(y) <= abs(x)) {
        float ratio = y / x;
        float xSquared = ratio * ratio;
        angle = ratio * (0.995354 + xSquared * (-0.288679 + xSquared * 0.079331));
    } else if ((abs(y) > abs(x)) && ((x / y) >= 0)) {
        float ratio = x / y;
        float xSquared = ratio * ratio;
        angle = (PI / 2.0) - ratio * (0.995354 + xSquared * (-0.288679 + xSquared * 0.079331));
    } else if ((abs(y) > abs(x)) && ((x / y) < 0)) {
        float ratio = x / y;
        float xSquared = ratio * ratio;
        angle = (-PI / 2.0) - ratio * (0.995354 + xSquared * (-0.288679 + xSquared * 0.079331));
    }
    
    // Adjust for correct quadrant
    if (((x >= 0.0) && (y >= 0.0)) || ((x >= 0.0) && (y < 0.0))) {
        // angle stays as is
    } else if ((x < 0.0) && (y >= 0.0)) {
        angle = angle + PI;
    } else if ((x < 0.0) && (y < 0.0)) {
        angle = angle - PI;
    }
    
    return angle;
}

// Navigate to target point using Vive coordinates
// Returns: true if target reached, false if still navigating
// Command format: "F<speed>", "L<rate>", "R<rate>", "S"
bool gotoPoint(float xDesired, float yDesired, float viveX, float viveY, float viveAngle, String& cmd) {
    // Calculate distance and angle to target
    float deltaY = yDesired - viveY;
    float deltaX = xDesired - viveX;
    float distance = sqrt(deltaX * deltaX + deltaY * deltaY);
    
    // Calculate desired angle (in degrees, 0° = +Y axis, clockwise positive)
    float desiredAngle = (180.0 / PI) * fastAtan2(deltaY, deltaX) + 90.0;
    
    // Normalize angle to -180 to 180 range
    if (desiredAngle > 180.0) {
        desiredAngle -= 360.0;
    } else if (desiredAngle < -180.0) {
        desiredAngle += 360.0;
    }
    
    // Normalize current angle
    float currentAngle = viveAngle;
    if (currentAngle > 180.0) {
        currentAngle -= 360.0;
    } else if (currentAngle < -180.0) {
        currentAngle += 360.0;
    }
    
    // Calculate angle error (shortest path)
    float angleError = desiredAngle - currentAngle;
    if (angleError > 180.0) {
        angleError -= 360.0;
    } else if (angleError < -180.0) {
        angleError += 360.0;
    }
    
    // Check if target reached
    if (distance <= DISTANCE_TOLERANCE) {
        cmd = "S";  // Stop
        return true;
    }
    
    // First, turn to face target direction
    if (abs(angleError) > ANGLE_TOLERANCE) {
        // Need to turn first
        if (angleError > 0) {
            cmd = "R" + String(60);  // Turn right (clockwise)
        } else {
            cmd = "L" + String(60);  // Turn left (counter-clockwise)
        }
        return false;
    }
    
    // Facing correct direction, move forward
    if (distance > DISTANCE_TOLERANCE) {
        // Calculate speed based on distance (slower when close)
        float speed = 50.0f;  // Base speed
        if (distance < 50.0f) {
            speed = 30.0f;  // Slow down when close
        } else if (distance < 100.0f) {
            speed = 40.0f;
        }
        cmd = "F" + String((int)speed);
        return false;
    }
    
    // Should not reach here, but stop just in case
    cmd = "S";
    return true;
}

// Navigate to target with obstacle avoidance (combines Vive nav with ToF sensors)
// Returns command string
String decideViveNavigation(float xDesired, float yDesired, 
                           float viveX, float viveY, float viveAngle,
                           uint16_t F, uint16_t R1, uint16_t R2) {
    String cmd;
    
    // Check for obstacles first (safety)
    const uint16_t OBSTACLE_THRESHOLD = 150;  // 15cm
    
    // Emergency stop if too close to obstacle
    if (F < OBSTACLE_THRESHOLD || R1 < 80) {
        return "S";
    }
    
    // Try to navigate to target
    bool reached = gotoPoint(xDesired, yDesired, viveX, viveY, viveAngle, cmd);
    
    // If moving forward and obstacle detected, stop and turn
    if (cmd.startsWith("F") && F < 300) {
        // Obstacle ahead, turn away
        if (R1 < R2 - 20) {
            cmd = "L" + String(40);  // Turn left if more space on left (away from right wall)
        } else {
            cmd = "R" + String(40);  // Turn right
        }
    }
    
    return cmd;
}

