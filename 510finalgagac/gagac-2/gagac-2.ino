// gagac-2.ino 

#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include "gagac-web.h"
#include "vive_tracker.h"
#include "vive_utils.h"

//UART from owner board
HardwareSerial OwnerSerial(1);   // use UART1，RX/TX pin

//pin
#define MOTOR_L_PWM   9
#define MOTOR_L_IN1   10
#define MOTOR_L_IN2   11

#define MOTOR_R_PWM   14
#define MOTOR_R_IN1   12
#define MOTOR_R_IN2   13

#define ENCODER_L_A   4
#define ENCODER_L_B   5

#define ENCODER_R_A   1
#define ENCODER_R_B   2

// VIVE Tracker pins
#define VIVE_PIN_FRONT  18  // Front VIVE tracker
#define VIVE_PIN_BACK   19  // Back VIVE tracker

//PWM setup
#define PWM_FREQ      700
#define PWM_RESOLUTION 10
#define PWM_MAX       1023

//~~~~~~~~~~wifi config~~~~~~~~~~~~~~~~
//const char* SSID     = "MoXianBao";
//const char* PASSWORD = "olivedog";

/* ！！！use in lab！！！！！！！
const char* SSID     = "TP-Link_8A8C";
const char* PASSWORD = "12488674";
IPAddress localIP(192, 168, 1, 120);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
*/

//remember to un- comment another line at void setup() if using lab wifi
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
void IRAM_ATTR encoderL_ISR() {
    if (digitalRead(ENCODER_L_B)) {
        encoderCountL++;
    } else {
        encoderCountL--;
    }
}

void IRAM_ATTR encoderR_ISR() {
    if (digitalRead(ENCODER_R_B)) {
        encoderCountR++;
    } else {
        encoderCountR--;
    }
}

void IRAM_ATTR onControlTimer() {
    controlFlag = true;
}

//motor control
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
void setCarSpeed(float speedPercent) {
    float maxRPM = MOTOR_MAX_RPM_RATED * 0.9;
    float targetRPM = maxRPM * speedPercent / 100.0;
    
    targetSpeedL = targetRPM;
    targetSpeedR = targetRPM;
}

void setCarTurn(float speedPercent, float turnRate) {
    float maxRPM = MOTOR_MAX_RPM_RATED * 0.9;
    
    float baseSpeed = maxRPM * speedPercent / 100.0;
    
    float turnFactor = turnRate / 100.0;
    targetSpeedL = baseSpeed * (1.0 + turnFactor);
    targetSpeedR = baseSpeed * (1.0 - turnFactor);
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

// helper
void printHelp() {

    Serial.println("Command List ");
    Serial.println("[Movement Control]");
    Serial.println("F<speed>   - Forward (F50)");
    Serial.println("B<speed>   - Back (B30) ");
    Serial.println("L<rate>    - Left (L20)");
    Serial.println("R<rate>    - Right (R20)");
    Serial.println("S          - Stop");
    Serial.println("[PID Parameters]");
    Serial.println("KPB<val>   - Kp (KPB2.5)");
    Serial.println("KIB<val>   - Ki (KIB0.7)");
    Serial.println("KDB<val>   - Kd (KDB0.0)");
    Serial.println("PID        - PID parameter");
    Serial.println("[Feedforward]");
    Serial.println("FFA<val>   - feedforwardA (FFA8.0)");
    Serial.println("FFB<val>   - feedbackB (FFB100)");
    Serial.println("FF1        - start feedforward");
    Serial.println("FF0        - ban feedforward");
    Serial.println("Testing");
    Serial.println("PWML<val>  - PWML (PWML300)");
    Serial.println("WMR<val>   - PWMR(PWMR300)");
    Serial.println("DZ<val>    - Deadzone PWM (DZ300)");
    Serial.println("RESET      - reset encoder");
    Serial.println("TEST       - hardware testing");
    Serial.println("[System Info]");
    Serial.println("I          - Detailed status report");
    Serial.println("H          - show helper");
}


void handleCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();

    //Serial.print("[SERVANT CMD] ");
    //Serial.println(cmd);

    //keyboard control
    if (cmd.startsWith("F")) {
        float speed = cmd.substring(1).toFloat();
        setCarSpeed(speed);
        Serial.printf("▶ forward %.1f%% (target: L=%.1f, R=%.1f RPM)\n", 
                     speed, targetSpeedL, targetSpeedR);
    }
    else if (cmd.startsWith("B")) {
        float speed = cmd.substring(1).toFloat();
        setCarSpeed(-speed);
        Serial.printf("◀ back %.1f%% (target: L=%.1f, R=%.1f RPM)\n", 
                     speed, abs(targetSpeedL), abs(targetSpeedR));
    }
    else if (cmd.startsWith("L")) {
        float turnRate = cmd.substring(1).toFloat();
        setCarTurn(50, turnRate);
        Serial.printf("↰ left %.1f (L=%.1f, R=%.1f RPM)\n", 
                     turnRate, targetSpeedL, targetSpeedR);
    }
    else if (cmd.startsWith("R")) {
        float turnRate = cmd.substring(1).toFloat();
        setCarTurn(50, -turnRate);
        Serial.printf("↱ right %.1f (L=%.1f, R=%.1f RPM)\n", 
                     turnRate, targetSpeedL, targetSpeedR);
    }
    else if (cmd == "S") {
        stopMotors();
        Serial.println("Motor stopped");
    }
    
    // PID base
    else if (cmd.startsWith("KPB")) {
        Kp_base = cmd.substring(3).toFloat();
        Serial.printf("Kp base set to: %.3f\n", Kp_base);
        integralL = 0;
        integralR = 0;
    }
    else if (cmd.startsWith("KIB")) {
        Ki_base = cmd.substring(3).toFloat();
        Serial.printf("Ki base set to: %.3f\n", Ki_base);
        integralL = 0;
        integralR = 0;
    }
    else if (cmd.startsWith("KDB")) {
        Kd_base = cmd.substring(3).toFloat();
        Serial.printf("Kd base set to: %.3f\n", Kd_base);
    }
    else if (cmd == "PID") {
        Serial.println("\ncurrent PID parameter:");
        Serial.printf("   base: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", 
                     Kp_base, Ki_base, Kd_base);
        Serial.printf("   current increase: Kp=%.3f, Ki=%.3f \n", Kp, Ki);
        Serial.printf("   feedforward parameter: A=%.2f, B=%.2f (%s)\n", 
                     feedforwardA, feedforwardB,
                     useFeedforward ? "start" : "ban");
        Serial.printf("   deadzone PWM: %d\n\n", deadZonePWM);
    }
    
    // feedforward parameter
    else if (cmd.startsWith("FFA")) {
        feedforwardA = cmd.substring(3).toFloat();
        Serial.printf("FFA: %.3f\n", feedforwardA);
    }
    else if (cmd.startsWith("FFB")) {
        feedforwardB = cmd.substring(3).toFloat();
        Serial.printf("FFB: %.3f\n", feedforwardB);
    }
    else if (cmd == "FF1") {
        useFeedforward = true;
        Serial.println("feedforward starts");
    }
    else if (cmd == "FF0") {
        useFeedforward = false;
        Serial.println("feedforward bans（pure PID mode）");
    }
    else if (cmd.startsWith("PWML")) {
        int pwm = cmd.substring(4).toInt();
        setMotorL(pwm);
        targetSpeedL = 0;  // PID banned
        Serial.printf("left motor pure PWM: %d (PID banned)\n", pwm);
    }
    else if (cmd.startsWith("PWMR")) {
        int pwm = cmd.substring(4).toInt();
        setMotorR(pwm);
        targetSpeedR = 0;  // PID banned
        Serial.printf("right motor pure PWM: %d (PID banned)\n", pwm);
    }
    else if (cmd == "I") {
        Serial.println("Motor Status Report");
        Serial.printf("left Motor:\n");
        Serial.printf("target speed:  %6.2f RPM                        ║\n", targetSpeedL);
        Serial.printf("current speed:  %6.2f RPM                        ║\n", speedL);
        Serial.printf("error:      %+6.2f RPM                       ║\n", errorL);
        Serial.printf("integral:    %+6.2f                           ║\n", integralL);
        Serial.printf("PWM output:   %4d / 1023                       ║\n", pwmOutputL);
        Serial.printf("Right Motor:                                        ║\n");
        Serial.printf("target speed:  %6.2f RPM                        ║\n", targetSpeedR);
        Serial.printf("current speed:  %6.2f RPM                        ║\n", speedR);
        Serial.printf("error:      %+6.2f RPM                       ║\n", errorR);
        Serial.printf("integral:    %+6.2f                           ║\n", integralR);
        Serial.printf("PWM output:   %4d / 1023                       ║\n", pwmOutputR);
        Serial.println("────────────────────────────────────────────────");
        Serial.printf("encoder counts:\n");
        Serial.printf("left encoder:  %8ld pulses\n", encoderCountL);
        Serial.printf("right encoder:  %8ld pulses\n", encoderCountR);
        Serial.println("╟────────────────────────────────────────────────╢");
        Serial.printf("Control Parameter:\n");
        Serial.printf("current Kp: %.3f  Ki: %.3f  Kd: %.3f \n", Kp, Ki, Kd);
        Serial.printf("feedforward: %s (A=%.2f, B=%.2f)\n", 
                     useFeedforward ? "ON " : "OFF", feedforwardA, feedforwardB);
        Serial.printf("deadzone PWM: %d \n", deadZonePWM);
    }
    else if (cmd == "H" || cmd == "HELP") {
        printHelp();
    }
    else if (cmd == "RESET") {
        encoderCountL = 0;
        encoderCountR = 0;
        lastEncoderCountL = 0;
        lastEncoderCountR = 0;
        Serial.println("ENcoder counts reset");
    }
    else if (cmd.startsWith("DZ")) {
        deadZonePWM = cmd.substring(2).toInt();
        Serial.printf("deadzone PWM set to: %d\n", deadZonePWM);
    }
    else if (cmd == "TEST") {
        stopMotors();
        delay(500);
        testHardware();
    }
    else {
        Serial.println("unknow commands, see helper");
    }
}

WebServer server(80);

//Routes
void handleRoot() { server.send(200, "text/html", webpage); }

// Arduino main function 
void setup() {
    Serial.begin(115200);
    delay(1000);

    //来自 owner 的 UART
    OwnerSerial.begin(115200, SERIAL_8N1, 18, 17);
    Serial.println("UART from owner ready");
    
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
        // 返回JSON格式，包含所有VIVE数据
        String json = "{\"x\":" + String(viveX) + 
                     ",\"y\":" + String(viveY) + 
                     ",\"angle\":" + String(viveAngle) + "}";
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
        String data = server.arg("data");
        Serial.print("Web: ");
        Serial.println(data);

        // movement control
        if (data.startsWith("F")) { setCarSpeed(data.substring(1).toFloat()); }
        else if (data.startsWith("B")) { setCarSpeed(-data.substring(1).toFloat()); }
        else if (data.startsWith("L")) { setCarTurn(50, data.substring(1).toFloat()); }
        else if (data.startsWith("R")) { setCarTurn(50, -data.substring(1).toFloat()); }
        else if (data == "S") { stopMotors(); }

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
    viveFront.initialize();
    viveBack.initialize();
    Serial.println("VIVE Tracking initialized");
    Serial.printf("   Front tracker: GPIO%d\n", VIVE_PIN_FRONT);
    Serial.printf("   Back tracker: GPIO%d\n", VIVE_PIN_BACK);
    
    // Synchronize with base stations
    Serial.println("Synchronizing VIVE trackers...");
    viveFront.synchronize(5);
    viveBack.synchronize(5);
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
    
    Serial.printf("PID timer: %dms (%.1fHz)\n", CONTROL_PERIOD_MS, 1000.0/CONTROL_PERIOD_MS);
    Serial.printf("PID base: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", Kp_base, Ki_base, Kd_base);
    Serial.printf("feedforward: A=%.2f, B=%.2f (%s)\n", 
                  feedforwardA, feedforwardB, 
                  useFeedforward ? "start" : "ban");
    Serial.printf("deadzone: %d PWM\n", deadZonePWM);
    Serial.println();
    
    lastSpeedCalcTime = millis();
    

    Serial.println("System Ready");
    Serial.println("'H'- for helper");
    Serial.println("'TEST'- for hardware testing");
    Serial.println();
}

void loop() {
    server.handleClient(); 

    // Process VIVE tracking data
    processViveData(viveFront, viveXFront, viveYFront);
    processViveData(viveBack, viveXBack, viveYBack);
    
    // Calculate center position (average of front and back)
    viveX = (float(viveXFront) + float(viveXBack)) / 2.0;
    viveY = (float(viveYFront) + float(viveYBack)) / 2.0;
    
    // Calculate orientation angle from front and back positions
    float deltaX = float(viveXBack) - float(viveXFront);
    float deltaY = float(viveYBack) - float(viveYFront);
    const float PI = 3.14159265;
    viveAngle = (180.0 / PI) * fastAtan2(deltaY, deltaX) + 90.0;
    
    // Normalize angle to -180 to 180 range
    if (viveAngle > 180.0) {
        viveAngle -= 360.0;
    } else if (viveAngle < -180.0) {
        viveAngle += 360.0;
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
    
    // when usb directly power servent (unplug the 5v wire from owner!!!!)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        handleCommand(cmd);
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
        if (millis() - lastVivePrintTime > 1000) {
            lastVivePrintTime = millis();
            Serial.printf("📍 VIVE: X=%.1f, Y=%.1f, Angle=%.1f° | Status: F=%d, B=%d\n",
                         viveX, viveY, viveAngle,
                         viveFront.getStatus(), viveBack.getStatus());
        }
    }
    
    delay(5);


}
