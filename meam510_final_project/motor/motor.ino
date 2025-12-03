#include "webpage.h"
#include "html510.h"
#include "Wire.h"
#include <ESP32Servo.h>

#define LEDC_RESOLUTION_BITS 12
#define LEDC_RESOLUTION ((1 << LEDC_RESOLUTION_BITS) - 1)

#define LEFTENCODERPINA 4
#define LEFTENCODERPINB 5
#define LEFTMOTORPINA 6
#define LEFTMOTORPINB 7

#define RIGHTENCODERPINA 10
#define RIGHTENCODERPINB 1
#define RIGHTMOTORPINA 19
#define RIGHTMOTORPINB 18

#define SERVOPIN 8

#define I2CFREQ 100000
#define I2C Wire
#define SLAVEADDRESS 0x50

#define SDA 0
#define SCL 9

const char* ssid = "ben_is_mad";

HTML510Server htmlServer(80);
Servo servo1;

float Kp = 0.5, Ki = 0, Kd = 0;  // 0.7 //0.033
float previousError = 0;

int lastPositionL = 0;
int lastRpmL = 0;
int lastPositionR = 0;
int lastRpmR = 0;

volatile uint8_t receivedData[32];
volatile uint8_t dataLength = 0;
volatile bool dataReceived = false;

void receiveEvent(int bytesin) {
  uint8_t len = 0;

  while (Wire.available()) {
    if (len < sizeof(receivedData)) {
      receivedData[len++] = Wire.read();
    } else {
      Wire.read();  // Discard excess data
    }
  }
  dataLength = len;
  dataReceived = true;
}

// controller
int lastOutput = 0;
float pidControl(int setpoint, int measuredValue) {
  static float integral;
  int error = setpoint - measuredValue;
  integral = 0;// += error;
  float derivative = 0; //error - previousError;
  float output = Kp * error + Ki * integral + Kd * derivative;

  if (integral > LEDC_RESOLUTION) integral = LEDC_RESOLUTION;

  if (output > LEDC_RESOLUTION || output < -LEDC_RESOLUTION) {
    output = lastOutput;
  }

  previousError = error;
  lastOutput = output;
  return output;
}


// encoder reading
int encoderL;
volatile int positionL = 0;
volatile float changeInPositionPerSecondL = 0;
volatile unsigned long lastTimeL = 0;
unsigned long timeDifferenceL;
volatile float speedL = 0;
void updateEncoderL() {
  encoderL = digitalRead(LEFTENCODERPINB);
  positionL = (encoderL > 0) ? -1 : 1;

  unsigned long currentTimeL = micros();
  if (lastTimeL > 0) {
    timeDifferenceL = currentTimeL - lastTimeL;
    speedL = 1000000.0 / timeDifferenceL;
  }
  changeInPositionPerSecondL = positionL * speedL;
  lastTimeL = currentTimeL;
}

int encoderR;
volatile int positionR = 0;
volatile float changeInPositionPerSecondR = 0;
volatile unsigned long lastTimeR = 0;
unsigned long timeDifferenceR;
volatile float speedR = 0;
void updateEncoderR() {
  encoderR = digitalRead(RIGHTENCODERPINB);
  positionR = (encoderR > 0) ? 1 : -1;

  unsigned long currentTimeR = micros();
  if (lastTimeR > 0) {
    timeDifferenceR = currentTimeR - lastTimeR;
    speedR = 1000000.0 / timeDifferenceR;
  }
  changeInPositionPerSecondR = positionR * speedR;
  lastTimeR = currentTimeR;
}

// html getting
void handleRoot() {
  htmlServer.sendhtml(body);
}

// sending the keystroke to motor
enum Direction { NONE,
                 W,
                 A,
                 S,
                 D,
                 SPACE };
Direction currentDirection = NONE;

void handleHitW() {
  currentDirection = W;
  Serial.println("FORWARD");
}

void handleHitA() {
  currentDirection = A;
  Serial.println("LEFT");
}

void handleHitS() {
  currentDirection = S;
  Serial.println("BACKWARD");
}

void handleHitD() {
  currentDirection = D;
  Serial.println("RIGHT");
}

void handleHitSpace() {
  currentDirection = NONE;
  Serial.println("STOP");
}

void handleLeftEncoder() {
  String s = String(lastRpmL);
  htmlServer.sendplain(s);
}


void handleRightEncoder() {
  String s = String(lastRpmR);
  htmlServer.sendplain(s);
}

int left = 1, right = 1;
bool servoOn = 0;
void keyTranslator(int speed = 90) {
  if (currentDirection == 0) {
    left = 1;
    right = 1;
  } else if (currentDirection == 1) {
    left = speed;
    right = speed;
  } else if (currentDirection == 2) {
    left = -speed / 2.0;
    right = speed / 2.0;
    left = (int)left;
    right = (int)right;
  } else if (currentDirection == 3) {
    left = -speed;
    right = -speed;
  } else if (currentDirection == 4) {
    left = speed / 2.0;
    right = -speed / 2.0;
    left = (int)left;
    right = (int)right;
  } else if (currentDirection == 5) {
    left = -speed/4;
    right = -speed;
  }

  else if (currentDirection == 6) {
    right = speed / 4.0;
    left = 0.0;
    left = (int)left;
    right = (int)right;
  }

  else if (currentDirection == 7) {
    left = speed / 4.0;
    right = 0.0;
    left = (int)left;
    right = (int)right;
  }
  else if (currentDirection == 98) {
    servoOn = 0;
  }
  else if (currentDirection == 99){
    servoOn = 1;
  }
}

// converting to duty cycle
int duty_left_save;
int duty_left = 0;
float rpmL = 0;
void leftyFunction() {
  int targetRPM = map(left, -100, 100, -99, 99);
  int measuredRPM = rpmL;

  int pidOutputL = (int)pidControl(targetRPM, measuredRPM);
  pidOutputL += duty_left_save;
  duty_left = min(abs(pidOutputL), LEDC_RESOLUTION-1);

  if (pidOutputL >= 0) {
    ledcWrite(LEFTMOTORPINA, 0);
    ledcWrite(LEFTMOTORPINB, duty_left);
    duty_left_save = duty_left;
  } else if (pidOutputL < 0) {
    ledcWrite(LEFTMOTORPINA, duty_left);
    ledcWrite(LEFTMOTORPINB, 0);
    duty_left_save = -duty_left;
  }
  // Serial.print("Left PID: ");
  // Serial.print(pidOutputL);
}

int duty_right_save;
int duty_right = 0;
float rpmR = 0;
void rightyFunction() {
  int targetRPM = map(right, -100, 100, -99, 99);
  int measuredRPM = rpmR;

  int pidOutputR = (int)pidControl(targetRPM, measuredRPM);
  pidOutputR += duty_right_save;
  duty_right = min(abs(pidOutputR), LEDC_RESOLUTION-1);

  if (pidOutputR >= 0) {
    ledcWrite(RIGHTMOTORPINA, duty_right);
    ledcWrite(RIGHTMOTORPINB, 0);
    duty_right_save = duty_right;
  } else if (pidOutputR < 0) {
    ledcWrite(RIGHTMOTORPINA, 0);
    ledcWrite(RIGHTMOTORPINB, duty_right);
    duty_right_save = -duty_right;
  }
  // Serial.print(" | Right PID: ");
  // Serial.println(pidOutputR);
}
// setup/loop

unsigned long previousMillis = 0;  // Store the last time the servo moved
unsigned long interval = 750;  // The interval for each servo movement (1200 ms)
int servoPosition = 0;  // The current servo position (0, 90, or 180)

void updateServo() {
  if (servoOn) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      if (servoPosition == 0) {
        servo1.write(0);
        servoPosition = 90;
      } else if (servoPosition == 90) {
        servo1.write(90);
        servoPosition = 180;
      }
      else if (servoPosition == 180) {
        servo1.write(180);
        servoPosition = 0;
      }
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(112500);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 103), IPAddress(192, 168, 1, 2), IPAddress(255, 255, 255, 0));

  htmlServer.begin();
  htmlServer.attachHandler("/", handleRoot);
  htmlServer.attachHandler("/hitW", handleHitW);
  htmlServer.attachHandler("/hitA", handleHitA);
  htmlServer.attachHandler("/hitS", handleHitS);
  htmlServer.attachHandler("/hitD", handleHitD);
  htmlServer.attachHandler("/hitSpace", handleHitSpace);
  htmlServer.attachHandler("/leftEncoder", handleLeftEncoder);
  htmlServer.attachHandler("/rightEncoder", handleRightEncoder);

  ledcAttach(LEFTMOTORPINA, 100, LEDC_RESOLUTION_BITS);
  ledcAttach(LEFTMOTORPINB, 100, LEDC_RESOLUTION_BITS);

  pinMode(LEFTENCODERPINA, INPUT);
  pinMode(LEFTENCODERPINB, INPUT);

  ledcAttach(RIGHTMOTORPINA, 100, LEDC_RESOLUTION_BITS);
  ledcAttach(RIGHTMOTORPINB, 100, LEDC_RESOLUTION_BITS);

  pinMode(RIGHTENCODERPINA, INPUT);
  pinMode(RIGHTENCODERPINB, INPUT);

  // pinMode(SDA, INPUT_PULLUP);
  // pinMode(SCL, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(LEFTENCODERPINA), updateEncoderL, RISING);
  attachInterrupt(digitalPinToInterrupt(RIGHTENCODERPINA), updateEncoderR, RISING);

  Wire.begin((uint8_t)SLAVEADDRESS, SDA, SCL, I2CFREQ);
  Wire.onReceive(receiveEvent);

  servo1.setPeriodHertz(50);
  servo1.attach(SERVOPIN, 1000, 2000);
}

void loop() {
  // put your main code here, to run repeatedly:
  htmlServer.serve();

  if (dataReceived) {
    currentDirection = static_cast<Direction>(receivedData[0]);
    dataReceived = false;  // Clear flag
  }
  keyTranslator(60);
  leftyFunction();
  rightyFunction();

  rpmL = changeInPositionPerSecondL / 12 * 60 / 60;  // thx cole
  if (rpmL > 110 || rpmL < -110) {
    positionL = lastPositionL;
    rpmL = lastRpmL;
  }

  lastPositionL = positionL;
  lastRpmL = rpmL;

  rpmR = changeInPositionPerSecondR / 12 * 60 / 60;  // thx cole
  if (rpmR > 110 || rpmR < -110) {
    positionR = lastPositionR;
    rpmR = lastRpmR;
  }

  lastPositionR = positionR;
  lastRpmR = rpmR;
  
  updateServo();

  // DO NOT DELETE THESE PRINT STATEMENTS OR EVERYTHING BREAKS
  Serial.print(left);
  Serial.print(", ");
  Serial.print(right);
  Serial.print(", ");
  Serial.print(lastRpmL);
  Serial.print(", ");
  Serial.println(lastRpmR);
}