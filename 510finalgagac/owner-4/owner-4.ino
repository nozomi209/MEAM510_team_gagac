// owner-4.ino
// UART between owner and servant 
// Right Wall Following with Auto Switch
// [扩展] 添加Vive导航功能，保持原有逻辑不变

#include <HardwareSerial.h>

// ToF function prototypes
void ToF_init();
bool ToF_read(uint16_t d[3]);

// Wall-following behavior
String decideWallFollowing(uint16_t F, uint16_t R1, uint16_t R2);

// Vive navigation behavior (behavior-vive.ino)
bool gotoPoint(float xDesired, float yDesired, float viveX, float viveY, float viveAngle, String& cmd);
String decideViveNavigation(float xDesired, float yDesired, 
                           float viveX, float viveY, float viveAngle,
                           uint16_t F, uint16_t R1, uint16_t R2);

HardwareSerial ServantSerial(1);
uint16_t tofDist[3];

// [原有逻辑保持不变] 自动模式开关，默认关闭
bool isAutoRunning = false;

// [新增] Vive tracking data
float viveX = 0.0;
float viveY = 0.0;
float viveAngle = 0.0;
bool viveDataValid = false;

// [新增] Operation modes
enum OperationMode {
    MODE_WALL_FOLLOWING = 0,
    MODE_AUTO_NAV_RED = 1,
    MODE_AUTO_NAV_BLUE = 2,
    MODE_MANUAL = 3
};

OperationMode currentMode = MODE_WALL_FOLLOWING;

// [新增] Target points (in mm, relative to Vive coordinate system)
const float TARGET_RED_X = 2000.0;   // Red target X coordinate
const float TARGET_RED_Y = 2000.0;   // Red target Y coordinate
const float TARGET_BLUE_X = 6000.0;  // Blue target X coordinate
const float TARGET_BLUE_Y = 2000.0;  // Blue target Y coordinate

// [新增] Auto navigation timing
unsigned long autoNavStartTime = 0;
const unsigned long AUTO_NAV_TIMEOUT = 30000;  // 30 seconds timeout

void sendToServant(const String &cmd) {
  ServantSerial.println(cmd);
  Serial.println(cmd);
}

// [新增] Parse Vive data from UART
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
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===== OWNER BOARD (Right Wall Logic + Vive Nav) =====");
  
  ToF_init();

  // Owner RX=18, TX=17
  ServantSerial.begin(115200, SERIAL_8N1, 18, 17);
  Serial.println("UART to servant ready. Waiting for Start...");
  
  Serial.println("\nMode Control Commands:");
  Serial.println("  'W' - Wall Following Mode");
  Serial.println("  'R' - Auto Navigate to RED target");
  Serial.println("  'B' - Auto Navigate to BLUE target");
  Serial.println("  'M' - Manual Mode");
  Serial.println("  Web: 'AUTO_ON' / 'AUTO_OFF' - Enable/Disable auto mode");
  Serial.println("\nCurrent mode: WALL_FOLLOWING");
}

void loop() {
  // [原有逻辑保持不变] 1. check web controller 发过来的指令
  if (ServantSerial.available()) {
    String webCmd = ServantSerial.readStringUntil('\n');
    webCmd.trim();

    if (webCmd == "AUTO_ON") {
      isAutoRunning = true;
      // [新增] 如果当前是手动模式，切换到壁障跟随
      if (currentMode == MODE_MANUAL) {
        currentMode = MODE_WALL_FOLLOWING;
      }
      Serial.println(">>> AUTO MODE STARTED <<<");
    } 
    else if (webCmd == "AUTO_OFF") {
      isAutoRunning = false;
      sendToServant("S"); // 立刻停车
      Serial.println(">>> AUTO MODE STOPPED <<<");
    }
    // [新增] 接收Vive数据
    else if (webCmd.startsWith("VIVE:")) {
      parseViveData(webCmd);
    }
  }
  
  // [新增] 2. Check for mode change commands from Serial
  if (Serial.available()) {
    char cmd = Serial.read();
    cmd = toupper(cmd);
    
    switch (cmd) {
      case 'W':
        currentMode = MODE_WALL_FOLLOWING;
        autoNavStartTime = 0;
        isAutoRunning = true;  // Enable auto when switching to wall following
        Serial.println("Mode: WALL_FOLLOWING");
        break;
      case 'R':
        currentMode = MODE_AUTO_NAV_RED;
        autoNavStartTime = 0;
        isAutoRunning = true;
        Serial.println("Mode: AUTO_NAV_RED");
        break;
      case 'B':
        currentMode = MODE_AUTO_NAV_BLUE;
        autoNavStartTime = 0;
        isAutoRunning = true;
        Serial.println("Mode: AUTO_NAV_BLUE");
        break;
      case 'M':
        currentMode = MODE_MANUAL;
        autoNavStartTime = 0;
        isAutoRunning = false;
        Serial.println("Mode: MANUAL");
        break;
    }
    // Clear remaining serial buffer
    while (Serial.available()) Serial.read();
  }
  
  // [原有逻辑保持不变，但扩展] 3. auto mode 开了才跑
  // [新增] 如果auto模式关闭且不是手动模式，则停车
  if (!isAutoRunning && currentMode != MODE_MANUAL) {
    sendToServant("S");
    delay(50);
    return;
  }
  
  // [原有逻辑保持不变] 4. Read ToF and execute behavior
  if (ToF_read(tofDist)) {
    uint16_t F  = tofDist[0];
    uint16_t R1 = tofDist[1];
    uint16_t R2 = tofDist[2];

    String cmd;
    bool targetReached = false;
    
    // [原有逻辑保持不变] 壁障跟随模式
    if (currentMode == MODE_WALL_FOLLOWING) {
      cmd = decideWallFollowing(F, R1, R2);
      autoNavStartTime = 0;  // Reset timer
    }
    // [新增] Vive导航到红色目标点
    else if (currentMode == MODE_AUTO_NAV_RED) {
      if (!viveDataValid) {
        // No Vive data, fall back to wall following
        cmd = decideWallFollowing(F, R1, R2);
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
          cmd = decideWallFollowing(F, R1, R2);
        } else {
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
                                      viveX, viveY, viveAngle, F, R1, R2);
          }
        }
      }
    }
    // [新增] Vive导航到蓝色目标点
    else if (currentMode == MODE_AUTO_NAV_BLUE) {
      if (!viveDataValid) {
        // No Vive data, fall back to wall following
        cmd = decideWallFollowing(F, R1, R2);
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
          cmd = decideWallFollowing(F, R1, R2);
        } else {
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
                                      viveX, viveY, viveAngle, F, R1, R2);
          }
        }
      }
    }
    // [新增] 手动模式：不发送命令（由web控制）
    else if (currentMode == MODE_MANUAL) {
      // Manual mode - do nothing, commands come from web
      delay(50);
      return;
    }

    // send commands to servant
    sendToServant(cmd);
    
    // [新增] Print status periodically
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime > 1000) {
      lastStatusTime = millis();
      Serial.printf("Mode: %d | Auto: %s | Vive: X=%.1f, Y=%.1f, A=%.1f° | ToF: F=%d, R1=%d, R2=%d\n",
                   currentMode, isAutoRunning ? "ON" : "OFF", viveX, viveY, viveAngle, F, R1, R2);
    }
  } else {
    //if sensor timeout，stop car
    sendToServant("S");
  }
  
  delay(50);
}
