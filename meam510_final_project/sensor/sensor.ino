#include "vive510.h"
#include "vivelib.h"
#include "Wire.h"
#include "webpage.h"
#include "html510.h"
#include <vl53l4cx_class.h>

#define I2CFREQ 40000
#define I2C Wire

#define SDA 1
#define SCL 0

#define TOF1 4
#define TOF2 5
#define TOF3 6

#define TOF1ADDRESS 0x12
#define TOF2ADDRESS 0x14
#define TOF3ADDRESS 0x16

#define SLAVEADDRESS 0x50
#define TOPHATADDRESS 0x28

#define VIVEPIN1 18  // front
#define VIVEPIN2 19  // back

const char *ssid = "ben_is_madder";

HTML510Server htmlServer(80);

Vive510 vive1(VIVEPIN1);
Vive510 vive2(VIVEPIN2);
VL53L4CX tof1(&I2C, TOF1);
VL53L4CX tof2(&I2C, TOF2);
VL53L4CX tof3(&I2C, TOF3);

int distance1, distance2, distance3;
static uint16_t xFront, yFront, xBack, yBack;
float viveX, viveY;
double angle;

bool wallFollowing = 0;
bool autoNavigationRed = 0;
bool autoNavigationBlue = 0;

int previousButtonState;

int counter = 0;

void send_I2C_byte(int data) {
  // Send data to slave
  Wire.beginTransmission(SLAVEADDRESS);
  Wire.write(data);  // Send some test data
  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    // Serial.println("Data sent to motor mcu successfully");
    rgbLedWrite(2, 0, 20, 0);  // green
  } else {
    // Serial.printf("Error sending data to motor mcu: %d\n", error);
    rgbLedWrite(2, 20, 0, 0);  // red
  }
}

void send_I2C_byte_tophat(uint8_t data) {
  // Send data to slave
  Wire.beginTransmission(TOPHATADDRESS);
  Wire.write(data);  // Send some test data
  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    // Serial.println("Data sent successfully");
  } else {
    // Serial.printf("Error sending data: %d\n", error);
  }
}

uint8_t receive_I2C_byte_tophat() {  // data should have space declared from caller
  // Request data from slave
  uint8_t bytesReceived = Wire.requestFrom(TOPHATADDRESS, 1);
  uint8_t byteIn = 0;

  if (bytesReceived > 0) {
    Serial.print("Received from slave: ");
    while (Wire.available()) {
      byteIn = Wire.read();
      // Serial.printf("0x%02X ", byteIn);
    }
    // Serial.println();
  } else return 0;
  return byteIn;  // return number of bytes read
}

uint8_t topHatSubroutine() {
  // send_I2C_byte_tophat(counter);
  counter = 0;
  uint8_t receivedHealth = receive_I2C_byte_tophat();
  if (receivedHealth <= 0) {
    send_I2C_byte(0);
    send_I2C_byte(98);
  }
  return 0;
}

int buttonState = 0;

void handleRoot() {
  htmlServer.sendhtml(body);
}

void handleFrontLeftToF() {
  String s = String(distance1);
  htmlServer.sendplain(s);
}

void handleFrontRightToF() {
  String s = String(distance2);
  htmlServer.sendplain(s);
}

void handleBackLeftToF() {
  String s = String(distance3);
  htmlServer.sendplain(s);
}

void handleViveX() {
  String s = String(viveX);
  htmlServer.sendplain(s);
}

void handleViveY() {
  String s = String(viveY);
  htmlServer.sendplain(s);
}

void handleViveTheta() {
  String s = String(angle);
  htmlServer.sendplain(s);
}

int buttonStates[] = { 40, 50, 60, 70 };  // random numbers, disregard

void handleHitW() {
  if ((wallFollowing == 0) && (autoNavigationRed == 0) && (autoNavigationBlue == 0)) {
    // Serial.println("forward");
    send_I2C_byte(1);
    if (buttonState != 1) {
      buttonState = 1;
    }
  }
}

void handleHitA() {
  if ((wallFollowing == 0) && (autoNavigationRed == 0) && (autoNavigationBlue == 0)) {
    // Serial.println("left");
    send_I2C_byte(2);
    if (buttonState != 2) {
      buttonState = 2;
    }
  }
}

void handleHitS() {
  if ((wallFollowing == 0) && (autoNavigationRed == 0) && (autoNavigationBlue == 0)) {
    // Serial.println("back");
    send_I2C_byte(3);
    if (buttonState != 3) {
      buttonState = 3;
    }
  }
}

void handleHitD() {
  if ((wallFollowing == 0) && (autoNavigationRed == 0) && (autoNavigationBlue == 0)) {
    // Serial.println("right");
    send_I2C_byte(4);
    if (buttonState != 4) {
      buttonState = 4;
    }
  }
}

void handleHitSpace() {
  if ((wallFollowing == 0) && (autoNavigationRed == 0) && (autoNavigationBlue == 0)) {
    // Serial.println("stop");
    send_I2C_byte(0);
    if (buttonState != 5) {
      buttonState = 5;
    }
  }
}

void handleSetManualMode() {
  wallFollowing = 0;
  autoNavigationRed = 0;
  autoNavigationBlue = 0;
  if (buttonState != 6) {
    buttonState = 6;
  }
}

void handleSetWallFollowMode() {
  wallFollowing = 1;
  autoNavigationRed = 0;
  autoNavigationBlue = 0;
  if (buttonState != 7) {
    buttonState = 7;
  }
}

void handleSetAutoNavRedMode() {
  wallFollowing = 0;
  autoNavigationRed = 1;
  autoNavigationBlue = 0;
  if (buttonState != 8) {
    buttonState = 8;
  }
}

void handleSetAutoNavBlueMode() {
  wallFollowing = 0;
  autoNavigationRed = 0;
  autoNavigationBlue = 1;
  if (buttonState != 11) {
    buttonState = 11;
  }
}

void handleServoOn() {
  send_I2C_byte(99);
  if (buttonState != 9) {
    buttonState = 9;
  }
  Serial.println("SERVO ON");
}

void handleServoOff() {
  send_I2C_byte(98);
  if (buttonState != 10) {
    buttonState = 10;
  }
  Serial.println("SERVO OFF");
}

void wallFollow(int tofFL, int tofBL, int tofFR, bool hardcode = 0, int tolerance = 40.0) {
  if (abs(tofFL - tofBL - 40) < tolerance) {
    // Serial.println("wf:forward");
    // send_I2C_byte(3);
    // delay(1);
    send_I2C_byte(1);
    delay(5);
  } else if (tofFL > tofBL + 40) {
    // Serial.println("wf:turn left");
    // send_I2C_byte(3);
    // delay(1);
    send_I2C_byte(6);
    delay(0.1);
  } else if (tofBL + 40 > tofFL) {
    if (tofFL < 90) {
      // Serial.println("wf:turn right");
      // send_I2C_byte(3);
      // delay(1);
      send_I2C_byte(5);
      delay(0.1);
    } else {
      // Serial.println("wf:turn right");
      // send_I2C_byte(3);
      // delay(1);
      send_I2C_byte(7);
      delay(0.1);
    }
  }
}

bool gotoPoint(float xDesired, float yDesired) {
  float deltaY = yDesired - viveY;
  float deltaX = xDesired - viveX;
  float desiredAngle = (180.0 / PI) * atan2Fast(deltaY, deltaX) + 90.0;

  if (desiredAngle > 180.0) {
    desiredAngle -= 360.0;
  } else if (desiredAngle < -180.0) {
    desiredAngle += 360.0;
  }

  if (abs(angle - desiredAngle) > 20.0) {
    send_I2C_byte(4);
    return false;
  }

  if (sqrt(deltaX * deltaX + deltaY * deltaY) > 5.0) {
    send_I2C_byte(1);
    return false;
  }

  return true;
}


void setup() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, "", 4);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 103), IPAddress(192, 168, 1, 2), IPAddress(255, 255, 255, 0));

  htmlServer.begin();
  htmlServer.attachHandler("/", handleRoot);
  htmlServer.attachHandler("/setManualMode", handleSetManualMode);
  htmlServer.attachHandler("/setWallFollowMode", handleSetWallFollowMode);
  htmlServer.attachHandler("/setAutoNavRed", handleSetAutoNavRedMode);
  htmlServer.attachHandler("/setAutoNavBlue", handleSetAutoNavBlueMode);
  htmlServer.attachHandler("/hitW", handleHitW);
  htmlServer.attachHandler("/hitA", handleHitA);
  htmlServer.attachHandler("/hitS", handleHitS);
  htmlServer.attachHandler("/hitD", handleHitD);
  htmlServer.attachHandler("/hitSpace", handleHitSpace);
  htmlServer.attachHandler("/viveX", handleViveX);
  htmlServer.attachHandler("/viveY", handleViveY);
  htmlServer.attachHandler("/viveTheta", handleViveTheta);
  htmlServer.attachHandler("/frontLeftTof", handleFrontLeftToF);
  htmlServer.attachHandler("/frontRightTof", handleFrontRightToF);
  htmlServer.attachHandler("/backLeftTof", handleBackLeftToF);
  htmlServer.attachHandler("/setServoOn", handleServoOn);
  htmlServer.attachHandler("/setServoOff", handleServoOff);

  Serial.begin(112500);

  I2C.begin(SDA, SCL, I2CFREQ);
  vive1.begin();
  vive2.begin();
  Serial.println("Vive Tracking Started");
  Serial.println(" ");

  pinMode(TOF1, OUTPUT);
  pinMode(TOF2, OUTPUT);
  pinMode(TOF3, OUTPUT);

  VL53L4CX_Error initStatus;

  tof1.begin();
  tof1.InitSensor(TOF1ADDRESS);
  tof1.end();

  tof2.begin();
  tof2.InitSensor(TOF2ADDRESS);
  tof2.end();

  tof3.begin();
  tof3.InitSensor(TOF3ADDRESS);
  tof3.end();

  tof1.VL53L4CX_StartMeasurement();
  tof2.VL53L4CX_StartMeasurement();
  tof3.VL53L4CX_StartMeasurement();

  Serial.println("ToF Sensors Started");
  Serial.println(" ");
}


unsigned long wallFollowStartTime = 0;
unsigned long previousMillis = 0;
uint8_t health = 100;
unsigned long autoNavStartTimeRed = 0;
unsigned long autoNavStartTimeBlue = 0;

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= 500) {  // unless i'm stupid that's 2 hz
    previousMillis = currentMillis;             // Reset the timer
    uint8_t tophatData = topHatSubroutine();    // Call the subroutine and process input

    // Optional: Debug or process the received tophat data
    // Serial.print("Tophat Data: ");
    // Serial.println(tophatData);
  }

  //vive shit
  processVive(vive1, xFront, yFront);
  processVive(vive2, xBack, yBack);

  viveX = (float(xFront) + float(xBack)) / 2.0;
  viveY = (float(yFront) + float(yBack)) / 2.0;

  float deltaX = float(xBack) - float(xFront);
  float deltaY = float(yBack) - float(yFront);
  angle = (180.0 / PI) * atan2Fast(deltaY, deltaX) + 90.0;
  if (angle > 180.0) {
    angle -= 360.0;
  }

  VL53L4CX_MultiRangingData_t MultiRangingData1, MultiRangingData2, MultiRangingData3;
  VL53L4CX_MultiRangingData_t *pMultiRangingData1 = &MultiRangingData1;
  VL53L4CX_MultiRangingData_t *pMultiRangingData2 = &MultiRangingData2;
  VL53L4CX_MultiRangingData_t *pMultiRangingData3 = &MultiRangingData3;

  int status1, status2, status3;
  uint8_t NewDataReady1 = 0, NewDataReady2 = 0, NewDataReady3 = 0;
  int no_of_object_found1 = 0, no_of_object_found2 = 0, no_of_object_found3 = 0;
  int objStatus1, objStatus2, objStatus3;
  float noise1, noise2, noise3;

  status1 = tof1.VL53L4CX_GetMeasurementDataReady(&NewDataReady1);
  if ((!status1) && (NewDataReady1 != 0)) {
    status1 = tof1.VL53L4CX_GetMultiRangingData(pMultiRangingData1);
    no_of_object_found1 = pMultiRangingData1->NumberOfObjectsFound;

    for (int i = 0; i < no_of_object_found1; i++) {
      objStatus1 = pMultiRangingData1->RangeData[i].RangeStatus;
      if (objStatus1 == 0) {
        distance1 = pMultiRangingData1->RangeData[i].RangeMilliMeter;
        noise1 = (float)pMultiRangingData1->RangeData[i].AmbientRateRtnMegaCps / 65536.0;
      }
    }
  }
  if (status1 == 0) {
    status1 = tof1.VL53L4CX_ClearInterruptAndStartMeasurement();
  }

  // Read data from TOF2
  status2 = tof2.VL53L4CX_GetMeasurementDataReady(&NewDataReady2);
  if ((!status2) && (NewDataReady2 != 0)) {
    status2 = tof2.VL53L4CX_GetMultiRangingData(pMultiRangingData2);
    no_of_object_found2 = pMultiRangingData2->NumberOfObjectsFound;

    for (int i = 0; i < no_of_object_found2; i++) {
      objStatus2 = pMultiRangingData2->RangeData[i].RangeStatus;
      if (objStatus2 == 0) {
        distance2 = pMultiRangingData2->RangeData[i].RangeMilliMeter;
        noise2 = (float)pMultiRangingData2->RangeData[i].AmbientRateRtnMegaCps / 65536.0;
      }
    }
  }
  if (status2 == 0) {
    status2 = tof2.VL53L4CX_ClearInterruptAndStartMeasurement();
  }

  // Read data from TOF3
  status3 = tof3.VL53L4CX_GetMeasurementDataReady(&NewDataReady3);
  if ((!status3) && (NewDataReady3 != 0)) {
    status3 = tof3.VL53L4CX_GetMultiRangingData(pMultiRangingData3);
    no_of_object_found3 = pMultiRangingData3->NumberOfObjectsFound;

    for (int i = 0; i < no_of_object_found3; i++) {
      objStatus3 = pMultiRangingData3->RangeData[i].RangeStatus;
      if (objStatus3 == 0) {
        distance3 = pMultiRangingData3->RangeData[i].RangeMilliMeter;
        noise3 = (float)pMultiRangingData3->RangeData[i].AmbientRateRtnMegaCps / 65536.0;
      }
    }
  }
  if (status3 == 0) {
    status3 = tof3.VL53L4CX_ClearInterruptAndStartMeasurement();
  }

  if (autoNavigationRed) {
    Serial.println("autored");
    if (autoNavStartTimeRed == 0) {
      autoNavStartTimeRed = millis();  // Record the start time when auto-navigation begins
    }

    unsigned long elapsedTime = millis() - autoNavStartTimeRed;

    if (elapsedTime <= 7500) {  // First 20 seconds
      send_I2C_byte(1);
    } else {
      send_I2C_byte(8);            // Send byte 1 for auto-navigation
      if (elapsedTime >= 10500) {  // First 20 seconds
        wallFollowing = true;
        autoNavigationBlue = false;
        autoNavigationRed = false;
        autoNavStartTimeRed = 0;
      }
    }
  }

  if (autoNavigationBlue) {
    Serial.println("autoblue");
    if (autoNavStartTimeBlue == 0) {
      autoNavStartTimeBlue = millis();  // Record the start time when auto-navigation begins
    }

    unsigned long elapsedTime = millis() - autoNavStartTimeBlue;

    if (elapsedTime <= 7500) {  // First 20 seconds
      send_I2C_byte(1);
    } else {
      send_I2C_byte(9);            // Send byte 1 for auto-navigation
      if (elapsedTime >= 10500) {  // First 20 seconds
        wallFollowing = true;
        autoNavigationBlue = false;
        autoNavigationRed = false;
        autoNavStartTimeBlue = 0;
      }
    }
  }


  if (wallFollowing) {
    wallFollow(distance1, distance3, distance2);
  }

  for (int i = 0; i <= 3; i++) {
    buttonStates[i] = buttonStates[i + 1];
  }
  buttonStates[3] = buttonState;

  // for (int i = 0; i < 4; i++) {
  //   Serial.print(buttonStates[i]);
  //   Serial.print(",");
  // }
  // Serial.print(buttonState);
  // Serial.print(",");
  // Serial.print(previousButtonState);

  // Serial.println();
  if ((buttonStates[0] == buttonStates[1]) && (buttonStates[1] == buttonStates[2]) && (buttonStates[2] == buttonStates[3]) && (buttonStates[3] != previousButtonState)) {
    previousButtonState = buttonState;
    counter++;
    // Serial.println("SOMETHING");
  }

  htmlServer.serve();


  Serial.print(distance1);
  Serial.print(", ");
  Serial.print(distance2);
  Serial.print(", ");
  Serial.print(distance3);
  Serial.print(", ");
  Serial.print(viveX);
  Serial.print(", ");
  Serial.println(viveY);
}