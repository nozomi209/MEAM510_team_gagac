// owner-4.ino
// UART between owner and servant 
// Integrated with Vive navigation

//ToF from [tof-3.ino] 
//  void ToF_init();
//  bool ToF_read(uint16_t d[3]);

//servant recieves commands as strings like: F70, L50, R50

//接线：
// tof 3个：
//        F: 前（粉色）；
//        L1: 左前（绿色）
//        L2: 左后（蓝色）
//        左右不要装反掉

//owner的18接servant的17；owner的17接servant的18
//默认两个大轮子朝前


#include <HardwareSerial.h>

// ToF function prototypes (tof-3.ino)
void ToF_init();
bool ToF_read(uint16_t d[3]);

// Wall-following behavior (behavior-wall.ino)
String decideWallFollowing(uint16_t F, uint16_t L1, uint16_t L2);

// Vive navigation behavior (behavior-vive.ino)
bool gotoPoint(float xDesired, float yDesired, float viveX, float viveY, float viveAngle, String& cmd);
String decideViveNavigation(float xDesired, float yDesired, 
                           float viveX, float viveY, float viveAngle,
                           uint16_t F, uint16_t L1, uint16_t L2);


//Owner: init UART1 on GPIO18 (RX) and GPIO17 (TX)
// 接的时候千万要owner的18接servant的17；owner的17接servant的18！！！！
HardwareSerial ServantSerial(1);

//TOF reading: d[0]=Fromt, d[1]=L1, d[2]=L2
uint16_t tofDist[3];

// Vive tracking data
float viveX = 0.0;
float viveY = 0.0;
float viveAngle = 0.0;
bool viveDataValid = false;

// Operation modes
enum OperationMode {
    MODE_WALL_FOLLOWING = 0,
    MODE_AUTO_NAV_RED = 1,
    MODE_AUTO_NAV_BLUE = 2,
    MODE_MANUAL = 3
};

OperationMode currentMode = MODE_WALL_FOLLOWING;

// Target points (in mm, relative to Vive coordinate system)
// Adjust these based on your actual target positions
const float TARGET_RED_X = 2000.0;   // Red target X coordinate
const float TARGET_RED_Y = 2000.0;   // Red target Y coordinate
const float TARGET_BLUE_X = 6000.0;  // Blue target X coordinate
const float TARGET_BLUE_Y = 2000.0;  // Blue target Y coordinate

// Auto navigation timing
unsigned long autoNavStartTime = 0;
const unsigned long AUTO_NAV_TIMEOUT = 30000;  // 30 seconds timeout

//send command to servant
void sendToServant(const String &cmd) {
  ServantSerial.println(cmd);

  //Serial.print("[TX to servant] ");
  Serial.println(cmd);
}

// Parse Vive data from UART
// Format: "VIVE:x.xx,y.yy,a.aa\n"
void parseViveData(String data) {
  if (data.startsWith("VIVE:")) {
    int comma1 = data.indexOf(',', 5);
    int comma2 = data.indexOf(',', comma1 + 1);
    
    if (comma1 > 0 && comma2 > 0) {
      viveX = data.substring(5, comma1).toFloat();
      viveY = data.substring(comma1 + 1, comma2).toFloat();
      viveAngle = data.substring(comma2 + 1).toFloat();
      viveDataValid = true;
    }
  }
}



void setup() {
  // USB serial
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("===== OWNER BOARD (ToF + UART to servant + Vive Nav) =====");

  // init TOF (tof-3.ino)
  ToF_init();

  // Servant: init UART1 on GPIO18 (RX) and GPIO17 (TX)
  // 接的时候千万要owner的18接servant的17；owner的17接servant的18！
  ServantSerial.begin(115200, SERIAL_8N1, 18, 17);
  Serial.println("UART to servant ready (using RX/TX pins)");
  
  Serial.println("\nMode Control Commands:");
  Serial.println("  'W' - Wall Following Mode");
  Serial.println("  'R' - Auto Navigate to RED target");
  Serial.println("  'B' - Auto Navigate to BLUE target");
  Serial.println("  'M' - Manual Mode");
  Serial.println("\nCurrent mode: WALL_FOLLOWING");
}

void loop() {
  // Check for mode change commands from Serial
  if (Serial.available()) {
    char cmd = Serial.read();
    cmd = toupper(cmd);
    
    switch (cmd) {
      case 'W':
        currentMode = MODE_WALL_FOLLOWING;
        autoNavStartTime = 0;
        Serial.println("Mode: WALL_FOLLOWING");
        break;
      case 'R':
        currentMode = MODE_AUTO_NAV_RED;
        autoNavStartTime = 0;
        Serial.println("Mode: AUTO_NAV_RED");
        break;
      case 'B':
        currentMode = MODE_AUTO_NAV_BLUE;
        autoNavStartTime = 0;
        Serial.println("Mode: AUTO_NAV_BLUE");
        break;
      case 'M':
        currentMode = MODE_MANUAL;
        autoNavStartTime = 0;
        Serial.println("Mode: MANUAL");
        break;
    }
    // Clear remaining serial buffer
    while (Serial.available()) Serial.read();
  }
  
  // Receive Vive data from servant board
  if (ServantSerial.available()) {
    String data = ServantSerial.readStringUntil('\n');
    data.trim();
    parseViveData(data);
  }
  
  //read tof distance
  if (ToF_read(tofDist)) {
    //tofDist[0] = 前
    //tofDist[1] = 左前
    //tofDist[2] = 左后
    uint16_t F  = tofDist[0]; //粉色
    uint16_t L1 = tofDist[1]; //绿色
    uint16_t L2 = tofDist[2]; //蓝色

    String cmd;
    bool targetReached = false;
    
    // Mode-based behavior decision
    switch (currentMode) {
      case MODE_AUTO_NAV_RED:
        if (!viveDataValid) {
          // No Vive data, fall back to wall following
          cmd = decideWallFollowing(F, L1, L2);
        } else {
          // Initialize timer if just started
          if (autoNavStartTime == 0) {
            autoNavStartTime = millis();
            Serial.println("Starting auto navigation to RED target");
          }
          
          // Check timeout
          if (millis() - autoNavStartTime > AUTO_NAV_TIMEOUT) {
            Serial.println("Auto nav timeout, switching to wall following");
            currentMode = MODE_WALL_FOLLOWING;
            autoNavStartTime = 0;
            cmd = decideWallFollowing(F, L1, L2);
            break;
          }
          
          // Navigate to red target
          targetReached = gotoPoint(TARGET_RED_X, TARGET_RED_Y, 
                                    viveX, viveY, viveAngle, cmd);
          
          if (targetReached) {
            Serial.println("RED target reached!");
            currentMode = MODE_WALL_FOLLOWING;
            autoNavStartTime = 0;
            cmd = "S";  // Stop
          } else {
            // Use obstacle-aware navigation
            cmd = decideViveNavigation(TARGET_RED_X, TARGET_RED_Y,
                                      viveX, viveY, viveAngle, F, L1, L2);
          }
        }
        break;
        
      case MODE_AUTO_NAV_BLUE:
        if (!viveDataValid) {
          // No Vive data, fall back to wall following
          cmd = decideWallFollowing(F, L1, L2);
        } else {
          // Initialize timer if just started
          if (autoNavStartTime == 0) {
            autoNavStartTime = millis();
            Serial.println("Starting auto navigation to BLUE target");
          }
          
          // Check timeout
          if (millis() - autoNavStartTime > AUTO_NAV_TIMEOUT) {
            Serial.println("Auto nav timeout, switching to wall following");
            currentMode = MODE_WALL_FOLLOWING;
            autoNavStartTime = 0;
            cmd = decideWallFollowing(F, L1, L2);
            break;
          }
          
          // Navigate to blue target
          targetReached = gotoPoint(TARGET_BLUE_X, TARGET_BLUE_Y, 
                                    viveX, viveY, viveAngle, cmd);
          
          if (targetReached) {
            Serial.println("BLUE target reached!");
            currentMode = MODE_WALL_FOLLOWING;
            autoNavStartTime = 0;
            cmd = "S";  // Stop
          } else {
            // Use obstacle-aware navigation
            cmd = decideViveNavigation(TARGET_BLUE_X, TARGET_BLUE_Y,
                                      viveX, viveY, viveAngle, F, L1, L2);
          }
        }
        break;
        
      case MODE_WALL_FOLLOWING:
      default:
        // decide behavior, returns command string
        cmd = decideWallFollowing(F, L1, L2);
        autoNavStartTime = 0;  // Reset timer
        break;
    }

    // send commands to servant
    sendToServant(cmd);
    
    // Print status
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime > 1000) {
      lastStatusTime = millis();
      Serial.printf("Mode: %d | Vive: X=%.1f, Y=%.1f, A=%.1f° | ToF: F=%d, L1=%d, L2=%d\n",
                   currentMode, viveX, viveY, viveAngle, F, L1, L2);
    }

  } else {
    //if sensor timeout，stop car
    sendToServant("S");
  }

  delay(50);
}

