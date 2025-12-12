// gagac-2.ino 

// 主控（Servant）程序：负责电机驱动、编码器测速、VIVE 追踪、Wi-Fi 网页控制，以及与 Owner 板的 UART 通信
#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include "gagac-web.h"
#include "vive_tracker.h"
#include "vive_utils.h"

//TOPHAT
#include <Wire.h>  // I2C

#define TOPHAT_ADDR 0x28
#define PIN_I2C_SDA 15  // GPIO 15
#define PIN_I2C_SCL 16  //GPIO 16

unsigned long lastTopHatTime = 0;
byte wifiPacketCount = 0; // 数WiFi packet 





//UART from owner board
HardwareSerial OwnerSerial(1);   // use UART1，RX/TX pin

// 车体左右电机驱动引脚（接双路驱动器）
#define MOTOR_L_PWM   9
#define MOTOR_L_IN1   10
#define MOTOR_L_IN2   11

#define MOTOR_R_PWM   14
#define MOTOR_R_IN1   12
#define MOTOR_R_IN2   13

#define ENCODER_L_A   4
#define ENCODER_L_B   5

#define ENCODER_R_A   2
#define ENCODER_R_B   1

// VIVE Tracker pins
// 注意：两个tracker都安装在车后部分的两边（左右排列）
#define VIVE_PIN_FRONT  6   // 跟踪器1：车后左边 VIVE tracker (GPIO6)
#define VIVE_PIN_BACK   7   // 跟踪器2：车后右边 VIVE tracker (GPIO7)

// 角度计算偏移量（根据实际测试调整）
// 如果计算出的角度方向不对，可以尝试改为 -90.0 或其他值
#define VIVE_ANGLE_OFFSET  90.0

//PWM setup
#define PWM_FREQ      700
#define PWM_RESOLUTION 10
#define PWM_MAX       1023

// ~~~~~~~~~~ Wi-Fi config (AP mode) ~~~~~~~~~~
const char* AP_SSID = "ESP32-MobileBase";
const char* AP_PASSWORD = "12345678"; 

// PID parameters base (set)
float Kp_base = 2.55;
float Ki_base = 0.7;
float Kd_base = 0.0;

// dynamic PID parameter
float Kp = 2.5;
float Ki = 0.7;
float Kd = 0.0;

//前馈控制参数
float feedforwardA = 11.0;   // 线性系数：PWM = A * 转速 + B
float feedforwardB = 150.0;  // 偏置项
bool useFeedforward = true;  // 是否启用前馈控制

//Controling cycle
#define CONTROL_PERIOD_MS  20
#define SPEED_CALC_PERIOD  100

//Encoder Parameter
#define ENCODER_PPR       11
#define GEAR_RATIO        46.8
#define PULSES_PER_REV    (ENCODER_PPR * GEAR_RATIO * 2)

//Moto parameter
#define MOTOR_MAX_RPM_NO_LOAD    130
#define MOTOR_MAX_RPM_RATED      100

// PWM deadzone
int deadZonePWM = 400;

//Globals
volatile long encoderCountL = 0;
volatile long encoderCountR = 0;

float speedL = 0.0;
float speedR = 0.0;

float targetSpeedL = 0.0;
float errorL = 0.0;
float lastErrorL = 0.0;
float integralL = 0.0;
int pwmOutputL = 0;

float targetSpeedR = 0.0;
float errorR = 0.0;
float lastErrorR = 0.0;
float integralR = 0.0;
int pwmOutputR = 0;

hw_timer_t *controlTimer = NULL;
volatile bool controlFlag = false;

// ======= 简单序列执行（直行/转向按时间顺序执行，纯网页控制用） =======
struct SeqStep {
    char mode;         // 'F','B','L','R'
    float value;       // speed or turn rate
    uint32_t duration; // ms
};
const uint8_t SEQ_MAX = 16;
SeqStep seqSteps[SEQ_MAX];
uint8_t seqCount = 0;
uint8_t seqIndex = 0;
bool seqActive = false;
uint32_t seqStartMs = 0;
bool seqPaused = false;

void seqStop() {
    seqActive = false;
    seqPaused = false;
    seqIndex = 0;
    seqCount = seqCount; // keep last loaded
    stopMotors();
}

void seqApplyStep(const SeqStep& s) {
    switch (s.mode) {
        case 'F': setCarSpeed(s.value); break;
        case 'B': setCarSpeed(-s.value); break;
        case 'L': setCarTurn(50, -s.value); break;
        case 'R': setCarTurn(50, s.value); break;
        case 'S': stopMotors(); break; // 停车保持一段时间
        default: stopMotors(); break;
    }
}

bool seqParse(const String& payload) {
    seqCount = 0;
    int start = 0;
    String s = payload;
    while (start < s.length() && seqCount < SEQ_MAX) {
        int sep = s.indexOf(';', start);
        String item = (sep == -1) ? s.substring(start) : s.substring(start, sep);
        item.trim();
        if (item.length() == 0) { if (sep == -1) break; start = sep + 1; continue; }
        int c1 = item.indexOf(',');
        int c2 = item.indexOf(',', c1 + 1);
        if (c1 < 0 || c2 < 0) { if (sep == -1) break; start = sep + 1; continue; }
        SeqStep st;
        st.mode = toupper(item.substring(0, c1)[0]);
        st.value = item.substring(c1 + 1, c2).toFloat();
        st.duration = (uint32_t)item.substring(c2 + 1).toInt();
        seqSteps[seqCount++] = st;
        if (sep == -1) break;
        start = sep + 1;
    }
    seqIndex = 0;
    return seqCount > 0;
}

void seqStart() {
    if (seqCount == 0) return;
    seqActive = true;
    seqPaused = false;
    seqIndex = 0;
    seqStartMs = millis();
    seqApplyStep(seqSteps[0]);
}

void seqProcess() {
    if (!seqActive || seqPaused || seqCount == 0) return;
    uint32_t now = millis();
    SeqStep& cur = seqSteps[seqIndex];
    if (now - seqStartMs >= cur.duration) {
        seqIndex++;
        if (seqIndex >= seqCount) {
            seqStop();
            return;
        }
        seqStartMs = now;
        seqApplyStep(seqSteps[seqIndex]);
    }
}

//VIVE 默认开启（便于直接读坐标）
bool isViveActive = true;
bool isViveTestMode = false;  // 测试模式：输出详细坐标数据

unsigned long lastSpeedCalcTime = 0;
long lastEncoderCountL = 0;
long lastEncoderCountR = 0;

// VIVE Tracking variables
ViveTracker viveFront(VIVE_PIN_FRONT);
ViveTracker viveBack(VIVE_PIN_BACK);
uint16_t viveXFront = 0, viveYFront = 0;
uint16_t viveXBack = 0, viveYBack = 0;
float viveX = 0.0, viveY = 0.0;
float viveAngle = 0.0;

//interrupts
// 左轮编码器 A 相上升沿中断：根据 B 相判断计数方向
void IRAM_ATTR encoderL_ISR() {
    if (digitalRead(ENCODER_L_B)) {
        encoderCountL++;
    } else {
        encoderCountL--;
    }
}

// 右轮编码器 A 相上升沿中断：根据 B 相判断计数方向
void IRAM_ATTR encoderR_ISR() {
    if (digitalRead(ENCODER_R_B)) {
        encoderCountR++;
    } else {
        encoderCountR--;
    }
}

// 控制周期定时器回调：只置位标志位，避免耗时操作
void IRAM_ATTR onControlTimer() {
    controlFlag = true;
}

//motor control
// 依据正/负号设置电机方向与 PWM，占空比受限于 PWM_MAX
void setMotorL(int speed) {
    speed = constrain(speed, -PWM_MAX, PWM_MAX);
    
    if (speed >= 0) {
        digitalWrite(MOTOR_L_IN1, HIGH);
        digitalWrite(MOTOR_L_IN2, LOW);
        ledcWrite(MOTOR_L_PWM, speed);
    } else {
        digitalWrite(MOTOR_L_IN1, LOW);
        digitalWrite(MOTOR_L_IN2, HIGH);
        ledcWrite(MOTOR_L_PWM, -speed);
    }
}

void setMotorR(int speed) {
    speed = constrain(speed, -PWM_MAX, PWM_MAX);
    
    if (speed >= 0) {
        digitalWrite(MOTOR_R_IN1, HIGH);
        digitalWrite(MOTOR_R_IN2, LOW);
        ledcWrite(MOTOR_R_PWM, speed);
    } else {
        digitalWrite(MOTOR_R_IN1, LOW);
        digitalWrite(MOTOR_R_IN2, HIGH);
        ledcWrite(MOTOR_R_PWM, -speed);
    }
}

void stopMotors() {
    setMotorL(0);
    setMotorR(0);
    targetSpeedL = 0;
    targetSpeedR = 0;
    integralL = 0;
    integralR = 0;
}

// speed calculate
// 基于编码器计数差分与时间间隔计算当前速度 (RPM)
void calculateSpeed() {
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastSpeedCalcTime;
    
    if (deltaTime >= SPEED_CALC_PERIOD) {
        long deltaCountL = encoderCountL - lastEncoderCountL;
        speedL = (float)deltaCountL / PULSES_PER_REV * 60000.0 / (float)deltaTime;
        
        long deltaCountR = encoderCountR - lastEncoderCountR;
        speedR = (float)deltaCountR / PULSES_PER_REV * 60000.0 / (float)deltaTime;
        
        lastEncoderCountL = encoderCountL;
        lastEncoderCountR = encoderCountR;
        lastSpeedCalcTime = currentTime;
    }
}

//updated PID function
// 左轮 PID + 前馈控制，动态调整 Kp/Ki
int pidControlL() {
    float speedRatio = abs(targetSpeedL) / 45.0;  
    
    if (speedRatio > 1.5) {
        Kp = Kp_base * 1.3;
        Ki = Ki_base * 1.2;
    } else if (speedRatio < 0.5) {
        Kp = Kp_base * 0.8;
        Ki = Ki_base * 0.9;
    } else {
        Kp = Kp_base;
        Ki = Ki_base;
    }
    
    errorL = targetSpeedL - speedL;
    
    integralL += errorL * (CONTROL_PERIOD_MS / 1000.0);
    float integralLimit = PWM_MAX / (Ki + 0.001);
    integralL = constrain(integralL, -integralLimit, integralLimit);
    
    float derivative = (errorL - lastErrorL) / (CONTROL_PERIOD_MS / 1000.0);
    lastErrorL = errorL;
    
    float output;
    
    if (useFeedforward && targetSpeedL != 0) {
        float feedforward = feedforwardA * abs(targetSpeedL) + feedforwardB;
        if (targetSpeedL < 0) feedforward = -feedforward;
        
        float feedback = Kp * errorL + Ki * integralL + Kd * derivative;
        output = feedforward + feedback;
    } else {
        output = Kp * errorL + Ki * integralL + Kd * derivative;
    }
    
    output = constrain(output, -PWM_MAX, PWM_MAX);
    
    if (targetSpeedL == 0) {
        output = 0;
        integralL = 0;
    } else {
        if (output > 0 && output < deadZonePWM) {
            output = deadZonePWM;
        } else if (output < 0 && output > -deadZonePWM) {
            output = -deadZonePWM;
        }
    }
    
    return (int)output;
}

int pidControlR() {
    float speedRatio = abs(targetSpeedR) / 45.0;
    
    if (speedRatio > 1.5) {
        Kp = Kp_base * 1.3;
        Ki = Ki_base * 1.2;
    } else if (speedRatio < 0.5) {
        Kp = Kp_base * 0.8;
        Ki = Ki_base * 0.9;
    } else {
        Kp = Kp_base;
        Ki = Ki_base;
    }
    
    errorR = targetSpeedR - speedR;
    
    integralR += errorR * (CONTROL_PERIOD_MS / 1000.0);
    float integralLimit = PWM_MAX / (Ki + 0.001);
    integralR = constrain(integralR, -integralLimit, integralLimit);
    
    float derivative = (errorR - lastErrorR) / (CONTROL_PERIOD_MS / 1000.0);
    lastErrorR = errorR;
    
    float output;
    
    if (useFeedforward && targetSpeedR != 0) {
        float feedforward = feedforwardA * abs(targetSpeedR) + feedforwardB;
        if (targetSpeedR < 0) feedforward = -feedforward;
        
        float feedback = Kp * errorR + Ki * integralR + Kd * derivative;
        output = feedforward + feedback;
    } else {
        output = Kp * errorR + Ki * integralR + Kd * derivative;
    }
    
    output = constrain(output, -PWM_MAX, PWM_MAX);
    
    if (targetSpeedR == 0) {
        output = 0;
        integralR = 0;
    } else {
        if (output > 0 && output < deadZonePWM) {
            output = deadZonePWM;
        } else if (output < 0 && output > -deadZonePWM) {
            output = -deadZonePWM;
        }
    }
    
    return (int)output;
}

//set car speed &turn
// 仅设定目标转速，实际输出由定时中断内的 PID 完成
void setCarSpeed(float speedPercent) {
    float maxRPM = MOTOR_MAX_RPM_RATED * 0.9;
    float targetRPM = maxRPM * speedPercent / 100.0;
    
    targetSpeedL = 0.999* targetRPM; //给左轮 - 一点
    targetSpeedR = targetRPM;
}

void setCarTurn(float speedPercent, float turnRate) {
    float maxRPM = MOTOR_MAX_RPM_RATED * 0.9;
    
    float baseSpeed = maxRPM * speedPercent / 100.0;
    
    float turnFactor = turnRate / 100.0;
    targetSpeedL = baseSpeed * (1.0 + turnFactor);  // 
    targetSpeedR = baseSpeed * (1.0 - turnFactor); ///
}

//test hardware
void testHardware() {
    Serial.println("Hardware Test");
    
    Serial.println("\n[test1] PWM output test");
    Serial.println("  left PWM=300, 3s...");
    setMotorL(300);
    delay(3000);
    setMotorL(0);
    Serial.println(" left motor testing done");
    delay(1000);
    
    Serial.println("\n[test1] PWM output test");
    Serial.println("  right PWM=300, 3s...");
    setMotorL(300);
    delay(3000);
    setMotorL(0);
    Serial.println(" right motor testing done");
    delay(1000);
    
    Serial.println("\n[test2] encoder test");
    Serial.println("manually rotate left wheel...");
    encoderCountL = 0;
    for(int i=0; i<20; i++) {
        delay(500);
        if(encoderCountL != 0) {
            Serial.printf(" left encoder works, counts=%ld\n", encoderCountL);
            break;
        }
        if(i == 19) Serial.println("left encoder not responding");
    }
    
    Serial.println("\n manually rotate right wheel...");
    encoderCountR = 0;
    for(int i=0; i<20; i++) {
        delay(500);
        if(encoderCountR != 0) {
            Serial.printf(" right encoder works, counts =%ld\n", encoderCountR);
            break;
        }
        if(i == 19) Serial.println(" right encoder not responding!");
    }
    
    Serial.println("\nself-testing is done");

    
    while(!Serial.available()) delay(100);
    while(Serial.available()) Serial.read();
}



void handleCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();

    // 只有在调试时才解开下面这一行，平时comment掉
    // Serial.println(cmd); 

    // keyboard control
    if (cmd.startsWith("F")) {
        float speed = cmd.substring(1).toFloat();
        setCarSpeed(speed);
    }
    else if (cmd.startsWith("B")) {
        float speed = cmd.substring(1).toFloat();
        setCarSpeed(-speed);
    }
    else if (cmd.startsWith("L")) {
        float turnRate = cmd.substring(1).toFloat();
        setCarTurn(50, -turnRate);  
    }
    else if (cmd.startsWith("R")) {
        float turnRate = cmd.substring(1).toFloat();
        setCarTurn(50, turnRate); 
    }
    else if (cmd == "S") {
        stopMotors();
    }
    // PID / Feedforward 参数
    else if (cmd.startsWith("KPB")) Kp_base = cmd.substring(3).toFloat();
    else if (cmd.startsWith("KIB")) Ki_base = cmd.substring(3).toFloat();
    else if (cmd.startsWith("KDB")) Kd_base = cmd.substring(3).toFloat();
    else if (cmd.startsWith("FFA")) feedforwardA = cmd.substring(3).toFloat();
    else if (cmd.startsWith("FFB")) feedforwardB = cmd.substring(3).toFloat();
    else if (cmd == "FF1") useFeedforward = true;
    else if (cmd == "FF0") useFeedforward = false;
    else if (cmd == "RESET") {
        encoderCountL = 0;
        encoderCountR = 0;
    }

}

WebServer server(80);

//Routes
void handleRoot() { server.send(200, "text/html", webpage); }

// Arduino main function 
// 初始化串口、Wi-Fi AP、Web API、引脚模式、PWM、编码器中断、VIVE、控制定时器
void setup() {
    Serial.begin(115200);
    delay(1000);

    //来自 owner 的 UART（实际接线：Servant TX=GPIO17 -> Owner RX，Servant RX=GPIO18 <- Owner TX）
    OwnerSerial.begin(115200, SERIAL_8N1, 18, 17);
    Serial.println("UART from owner ready");
    
    // TopHat I2C init
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.println("TopHat I2C Initialized on pins 15/16");



    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    Serial.print("Access Point started! Connect to: ");
    Serial.println(AP_SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("\n\n");

    //web
    server.on("/", handleRoot);

    // VIVE data endpoint - 合并为一个API以减少网络包
    server.on("/viveData", [](){
        // 返回中心坐标/角度，以及两只tracker的原始/滤波数据与状态
        uint16_t rawXFront = viveFront.getXCoordinate();
        uint16_t rawYFront = viveFront.getYCoordinate();
        uint16_t rawXBack  = viveBack.getXCoordinate();
        uint16_t rawYBack  = viveBack.getYCoordinate();

        String json = "{";
        json += "\"x\":" + String(viveX);
        json += ",\"y\":" + String(viveY);
        json += ",\"angle\":" + String(viveAngle);
        json += ",\"frontRaw\":{\"x\":" + String(rawXFront) + ",\"y\":" + String(rawYFront) + "}";
        json += ",\"backRaw\":{\"x\":" + String(rawXBack) + ",\"y\":" + String(rawYBack) + "}";
        json += ",\"frontFiltered\":{\"x\":" + String(viveXFront) + ",\"y\":" + String(viveYFront) + "}";
        json += ",\"backFiltered\":{\"x\":" + String(viveXBack) + ",\"y\":" + String(viveYBack) + "}";
        json += ",\"status\":{\"front\":" + String(viveFront.getStatus()) + ",\"back\":" + String(viveBack.getStatus()) + "}";
        json += "}";
        server.send(200, "application/json", json);
    });
    
    // 保留单独端点以兼容（可选）
    server.on("/viveX", [](){
        server.send(200, "text/plain", String(viveX));
    });
    
    server.on("/viveY", [](){
        server.send(200, "text/plain", String(viveY));
    });
    
    server.on("/viveAngle", [](){
        server.send(200, "text/plain", String(viveAngle));
    });

    server.on("/cmd", [](){
        wifiPacketCount++; //wifi包
        String data = server.arg("data");
        Serial.print("Web: ");
        Serial.println(data);

        // movement control
        if (data.startsWith("F")) { setCarSpeed(data.substring(1).toFloat()); }
        else if (data.startsWith("B")) { setCarSpeed(-data.substring(1).toFloat()); }
        // [修改] 网页按L -> 传负数
        else if (data.startsWith("L")) { setCarTurn(50, -data.substring(1).toFloat()); } 
        // [修改] 网页按R -> 传正数
        else if (data.startsWith("R")) { setCarTurn(50, data.substring(1).toFloat()); }
        else if (data == "S") { stopMotors(); }


        // [新增] 转发 Auto 开关给 Owner
        else if (data == "AUTO_ON") {
            OwnerSerial.println("AUTO_ON"); 
            Serial.println("Sent AUTO_ON to Owner");
        }
        else if (data == "AUTO_OFF") {
            OwnerSerial.println("AUTO_OFF");
            Serial.println("Sent AUTO_OFF to Owner");
            stopMotors(); // 顺便让车停下
        }
        // 手动规划开关/路线下发
        else if (data == "MP_ON" || data == "MP_OFF" || data.startsWith("MP_ROUTE:")) {
            OwnerSerial.println(data);
            Serial.printf("Sent %s to Owner (manual planner)\n", data.c_str());
        }
        // 手动规划参数下发
        else if (data.startsWith("MP_PARAM:")) {
            OwnerSerial.println(data);
            Serial.printf("Sent %s to Owner (manual param)\n", data.c_str());
        }
        // 本地序列控制（网页直接让小车按时间执行直行/转向）
        else if (data.startsWith("SEQ:")) {
            String payload = data.substring(4);
            if (seqParse(payload)) {
                Serial.printf("Loaded SEQ with %d steps\n", seqCount);
            } else {
                Serial.println("SEQ parse failed");
            }
        }
        else if (data == "SEQ_START") {
            seqStart();
            Serial.println("SEQ start");
        }
        else if (data == "SEQ_STOP") {
            seqStop();
            Serial.println("SEQ stop");
        }
        
        // [新增] 转发参数调整命令给 Owner
        else if (data.startsWith("PARAM:")) {
            OwnerSerial.println(data); // 直接转发 "PARAM:参数名=值"
            Serial.printf("Sent parameter update: %s\n", data.c_str());
        }
        
        // [新增] 本地处理 VIVE 开关
        else if (data == "VIVE_ON") { 
            isViveActive = true; 
            Serial.println("VIVE System ACTIVATED");
        }
        else if (data == "VIVE_OFF") { 
            isViveActive = false; 
            Serial.println("VIVE System DISABLED");
        }
        else if (data == "VIVE_TEST_ON") {
            isViveTestMode = true;
            isViveActive = true;
            Serial.println("VIVE Test Mode ACTIVATED - 详细数据输出");
        }
        else if (data == "VIVE_TEST_OFF") {
            isViveTestMode = false;
            Serial.println("VIVE Test Mode DISABLED");
        }

        // slider
        else if (data.startsWith("SPEED=")) {
            float val = data.substring(6).toFloat();
            setCarSpeed(val);
            Serial.printf("⚙ slider speed %.1f%%\n", val);
        }
        else if (data.startsWith("TURN=")) {
            float val = data.substring(5).toFloat();
            setCarTurn(50, val);
            Serial.printf("↔ slider turn %.1f\n", val);
        }

        server.send(200, "text/plain", "OK");
    });

    server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.println("Web server started at http://192.168.4.1");

    Serial.println("ESP32-S3 PID Control System");
    Serial.println();
    Serial.println("System Info:");
    Serial.println("Motor: JGA25-370-46.8K (12V, 130RPM)");
    Serial.printf("Encoder: %.0f pulses/rev\n", PULSES_PER_REV);
    Serial.printf("   Arduino ESP32: v%d.%d.%d\n", 
                  ESP_ARDUINO_VERSION_MAJOR, 
                  ESP_ARDUINO_VERSION_MINOR, 
                  ESP_ARDUINO_VERSION_PATCH);
    Serial.println();
    
    //pinmode
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    
    //PWM
    ledcAttach(MOTOR_L_PWM, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(MOTOR_R_PWM, PWM_FREQ, PWM_RESOLUTION);
    
    Serial.println(" PWM set:");
    Serial.printf("   left motor:  GPIO%d @ %dHz\n", MOTOR_L_PWM, PWM_FREQ);
    Serial.printf("   right motor: GPIO%d @ %dHz\n", MOTOR_R_PWM, PWM_FREQ);
    Serial.println();
    
    //encoder pin
    pinMode(ENCODER_L_A, INPUT_PULLUP);
    pinMode(ENCODER_L_B, INPUT_PULLUP);
    pinMode(ENCODER_R_A, INPUT_PULLUP);
    pinMode(ENCODER_R_B, INPUT_PULLUP);
    
    //attachinterrupt
    attachInterrupt(digitalPinToInterrupt(ENCODER_L_A), encoderL_ISR, RISING);
    attachInterrupt(digitalPinToInterrupt(ENCODER_R_A), encoderR_ISR, RISING);
    
    Serial.println("encoder set:");
    Serial.printf("   left encoder: A=GPIO%d, B=GPIO%d\n", ENCODER_L_A, ENCODER_L_B);
    Serial.printf("   right encoder: A=GPIO%d, B=GPIO%d\n", ENCODER_R_A, ENCODER_R_B);
    Serial.println();
    
    // Initialize VIVE trackers
    // 两个tracker安装在车后部分的两边（左右排列）
    viveFront.initialize();
    viveBack.initialize();
    Serial.println("VIVE Tracking initialized");
    Serial.printf("   跟踪器1 (车后左边): GPIO%d\n", VIVE_PIN_FRONT);
    Serial.printf("   跟踪器2 (车后右边): GPIO%d\n", VIVE_PIN_BACK);
    
    // Synchronize with base stations
    Serial.println("Synchronizing VIVE trackers...");
    viveFront.synchronize(5);
    viveBack.synchronize(5);
    Serial.printf("同步结果: tracker1=%d, tracker2=%d (0=无信号,1=仅同步,2=接收中)\n",
                  viveFront.getStatus(), viveBack.getStatus());
    Serial.println("VIVE synchronization complete");
    Serial.println();
    
    //timer
    uint32_t timerFrequency = 1000000 / CONTROL_PERIOD_MS;
    controlTimer = timerBegin(timerFrequency);
    
    if (controlTimer == NULL) {
        Serial.println("timer init faield!");
        while(1) delay(1000);
    }
    
    timerAttachInterrupt(controlTimer, &onControlTimer);
    timerAlarm(controlTimer, CONTROL_PERIOD_MS * 1000, true, 0);
    
    lastSpeedCalcTime = millis();
    
    Serial.println("System Ready");
}

void loop() {
    // 轮询处理 Web 请求
    server.handleClient(); 
    if (isViveActive) {
        // Process VIVE tracking data
        processViveData(viveFront, viveXFront, viveYFront);
        processViveData(viveBack, viveXBack, viveYBack);
        
        // Calculate center position (average of two trackers at back of vehicle)
        // 两个tracker在车后两边，计算它们连线的中点作为中心位置
        viveX = (float(viveXFront) + float(viveXBack)) / 2.0;
        viveY = (float(viveYFront) + float(viveYBack)) / 2.0;
        
        // Calculate orientation angle from two tracker positions
        // 两个tracker在车后左右排列：
        // - tracker1（viveFront/GPIO15）在车后左边
        // - tracker2（viveBack/GPIO16）在车后右边
        // 连线方向：从左边tracker指向右边tracker（从左到右）
        // 车辆前进方向：垂直于连线方向（向前或向后，取决于定义）
        float deltaX = float(viveXBack) - float(viveXFront);  // 从左边到右边的X方向
        float deltaY = float(viveYBack) - float(viveYFront);  // 从左边到右边的Y方向
        // 使用标准 atan2f 计算角度，减少近零时的近似误差
        // VIVE_ANGLE_OFFSET 表示车辆前进方向相对于连线方向的偏移
        // 如果角度方向不对，可以调整 VIVE_ANGLE_OFFSET 的值（如改为 -90.0）
        viveAngle = (180.0 / PI) * atan2f(deltaY, deltaX) + VIVE_ANGLE_OFFSET;
        
        // Normalize angle to -180 to 180 range
        if (viveAngle > 180.0) {
            viveAngle -= 360.0;
        } else if (viveAngle < -180.0) {
            viveAngle += 360.0;
        }
    }
    calculateSpeed();
    
    //PID control
    if (controlFlag) {
        controlFlag = false;
        
        pwmOutputL = pidControlL();
        pwmOutputR = pidControlR();
        
        setMotorL(pwmOutputL);
        setMotorR(pwmOutputR);
    }
    
    // 串口命令（用于测试，USB直接供电时启用）
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();
        
        // VIVE测试命令
        if (cmd == "VIVE_TEST_ON" || cmd == "TEST_ON") {
            isViveTestMode = true;
            isViveActive = true;
            Serial.println("✅ VIVE 测试模式已启用 - 每200ms输出详细数据");
            Serial.println("   发送 'VIVE_TEST_OFF' 或 'TEST_OFF' 关闭测试模式");
        }
        else if (cmd == "VIVE_TEST_OFF" || cmd == "TEST_OFF") {
            isViveTestMode = false;
            Serial.println("❌ VIVE 测试模式已关闭");
        }
        else if (cmd == "VIVE_ON") {
            isViveActive = true;
            Serial.println("✅ VIVE 系统已激活");
        }
        else if (cmd == "VIVE_OFF") {
            isViveActive = false;
            isViveTestMode = false;
            Serial.println("❌ VIVE 系统已关闭");
        }
        else if (cmd == "VIVE_STATUS" || cmd == "STATUS") {
            Serial.println("═══════════════════════════════════════");
            Serial.println("📍 VIVE 系统状态");
            Serial.println("───────────────────────────────────────");
            Serial.printf("系统激活: %s\n", isViveActive ? "是" : "否");
            Serial.printf("测试模式: %s\n", isViveTestMode ? "是" : "否");
            Serial.printf("跟踪器1状态 (车后左边): %d (0=无信号, 1=仅同步, 2=接收中)\n", viveFront.getStatus());
            Serial.printf("跟踪器2状态 (车后右边): %d (0=无信号, 1=仅同步, 2=接收中)\n", viveBack.getStatus());
            Serial.printf("当前坐标: X=%.2f, Y=%.2f\n", viveX, viveY);
            Serial.printf("当前角度: %.2f°\n", viveAngle);
            Serial.println("═══════════════════════════════════════");
        }
        else {
            // 其他命令交给handleCommand处理
            handleCommand(cmd);
        }
    }

    //commands from owner (UART)
        // ===== commands from owner (UART) =====
    if (OwnerSerial.available()) {
    String cmd = OwnerSerial.readStringUntil('\n');
    Serial.print("[OWNER CMD] ");
    Serial.println(cmd);

    //OwnerSerial.print("[SERVANT CMD] ");
    //OwnerSerial.println(cmd);

    handleCommand(cmd);
}

    
    // update status 
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 200) {
        lastPrintTime = millis();
        
        if (targetSpeedL != 0 || targetSpeedR != 0) {
            Serial.printf("⚙ L: target=%5.1f current=%5.1f error=%+5.1f PWM=%4d | ", 
                         targetSpeedL, speedL, errorL, pwmOutputL);
            Serial.printf("R: target=%5.1f current=%5.1f error=%+5.1f PWM=%4d | ", 
                         targetSpeedR, speedR, errorR, pwmOutputR);
            Serial.printf("Kp=%.2f\n", Kp);  // show current kp
        }
        
        // Print VIVE data periodically
        static unsigned long lastVivePrintTime = 0;
        // 测试模式：更频繁、更详细的输出
        // 测试模式下提高输出频率（原 200ms -> 100ms）
        if (isViveTestMode && millis() - lastVivePrintTime > 100) {
            lastVivePrintTime = millis();
            // 获取原始坐标（未滤波）
            uint16_t rawXFront = viveFront.getXCoordinate();
            uint16_t rawYFront = viveFront.getYCoordinate();
            uint16_t rawXBack = viveBack.getXCoordinate();
            uint16_t rawYBack = viveBack.getYCoordinate();
            
            Serial.println("═══════════════════════════════════════");
            Serial.printf("📍 VIVE 测试数据 [%lu ms]\n", millis());
            Serial.println("───────────────────────────────────────");
            Serial.printf("跟踪器1 (车后左边, GPIO15):\n");
            Serial.printf("  原始坐标: X=%d, Y=%d\n", rawXFront, rawYFront);
            Serial.printf("  滤波后:   X=%d, Y=%d\n", viveXFront, viveYFront);
            Serial.printf("  状态:     %d (0=无信号, 1=仅同步, 2=接收中)\n", viveFront.getStatus());
            Serial.printf("跟踪器2 (车后右边, GPIO16):\n");
            Serial.printf("  原始坐标: X=%d, Y=%d\n", rawXBack, rawYBack);
            Serial.printf("  滤波后:   X=%d, Y=%d\n", viveXBack, viveYBack);
            Serial.printf("  状态:     %d (0=无信号, 1=仅同步, 2=接收中)\n", viveBack.getStatus());
            Serial.println("───────────────────────────────────────");
            float deltaX = float(viveXBack) - float(viveXFront);
            float deltaY = float(viveYBack) - float(viveYFront);
            Serial.printf("ΔX=%.1f, ΔY=%.1f, angle=%.1f° (offset=%.1f°)\n",
                          deltaX, deltaY, viveAngle, VIVE_ANGLE_OFFSET);
            Serial.printf("中心位置: X=%.2f, Y=%.2f\n", viveX, viveY);
            Serial.printf("朝向角度: %.2f°\n", viveAngle);
            Serial.printf("左右距离: %.2f (用于验证，应接近车后部宽度)\n", 
                         sqrt(pow(deltaX, 2) + pow(deltaY, 2)));
            Serial.println("═══════════════════════════════════════\n");
        }
        // 正常模式：1秒输出一次
        else if (!isViveTestMode && millis() - lastVivePrintTime > 1000 && isViveActive) {
            lastVivePrintTime = millis();
            Serial.printf("📍 VIVE: X=%.1f, Y=%.1f, Angle=%.1f° | Status: F=%d, B=%d\n",
                         viveX, viveY, viveAngle,
                         viveFront.getStatus(), viveBack.getStatus());
        }
        
        // Send VIVE data to owner board via UART (every 100ms for navigation)
        static unsigned long lastViveUartTime = 0;
        if (millis() - lastViveUartTime > 100 && isViveActive) {
            lastViveUartTime = millis();
            // Format: "VIVE:x.xx,y.yy,a.aa\n"
            OwnerSerial.printf("VIVE:%.2f,%.2f,%.2f\n", viveX, viveY, viveAngle);
        }
    }
    
    // 本地序列执行（直行/转向按时间）
    seqProcess();
    
    //TopHat update
    if (millis() - lastTopHatTime > 500) {
        lastTopHatTime = millis();
        
        //向TopHat 发送计数
        Wire.beginTransmission(TOPHAT_ADDR);
        Wire.write(wifiPacketCount); 
        byte error = Wire.endTransmission();
        
        if (error != 0) {
           // Serial.print("TopHat I2C Error: "); Serial.println(error);
        }

        wifiPacketCount = 0; // 重计
    }

    delay(5);


}
