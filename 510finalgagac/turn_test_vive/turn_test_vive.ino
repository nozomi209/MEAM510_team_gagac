/*
 * turn_test_vive.ino — 90°转向标定/测试（基于 VIVE 朝向角反馈）
 *
 * 目标：
 * - 用串口监视器发命令，让底盘“左/右转到指定角度（默认90°）”
 * - 在线调参（快/慢 PWM、减速阈值、提前量、允许误差、刹车脉冲等）
 * - 实时打印：viveAngle、累计转角、剩余角度、当前阶段，方便把 90° 调到稳定可复现
 *
 * 硬件假设：
 * - 与现有 gagac-2.ino 相同接线（电机/编码器/VIVE 两个 tracker）
 * - 两个 tracker 安装在车后左右（与现有角度算法一致）
 *
 * 串口命令（末尾回车）：
 * - HELP                     打印帮助
 * - P                        打印当前参数
 * - STOP                     立刻停车/取消转向
 * - R[deg]                   右转 deg 度（例如：R 或 R90 或 R45）
 * - L[deg]                   左转 deg 度
 * - A=<deg>                  设置默认目标角度（例如：A=90）
 * - F=<pwm>                  设置快转 PWM（0~1023）
 * - S=<pwm>                  设置慢转 PWM（0~1023）
 * - SW=<deg>                 剩余角度 <= SW 时切慢转（例如：SW=25）
 * - TOL=<deg>                允许误差（例如：TOL=2）
 * - LEAD=<deg>               提前量（剩余角度 <= LEAD 就准备停/刹车，减少过冲）
 * - BRAKE=<ms>,<pwm>         反向刹车脉冲（例如：BRAKE=80,180；ms=0 表示关闭）
 * - PR=<ms>                  打印周期（例如：PR=100）
 *
 * 备注：
 * - 本测试用 PWM 直接驱动电机（不走编码器 PID），尽量减少干扰变量，方便你快速标定“90°”。
 * - 真正导航时你现在的 L/R 已经是原地旋转语义；建议先用本测试把“角度—电机力度/刹车”调稳。
 */

#include <Arduino.h>
#include "vive_tracker.h"
#include "vive_utils.h"

// ========== 与 gagac-2.ino 一致的引脚 ==========
#define MOTOR_L_PWM   9
#define MOTOR_L_IN1   10
#define MOTOR_L_IN2   11

#define MOTOR_R_PWM   14
#define MOTOR_R_IN1   12
#define MOTOR_R_IN2   13

#define VIVE_PIN_FRONT  6   // 车后左
#define VIVE_PIN_BACK   7   // 车后右

// 角度偏移（与你现有工程一致；若方向不对可改 -90/90）
#define VIVE_ANGLE_OFFSET  90.0f

// PWM
#define PWM_FREQ       700
#define PWM_RESOLUTION 10
#define PWM_MAX        1023

// ========== VIVE ==========
ViveTracker viveFront(VIVE_PIN_FRONT);
ViveTracker viveBack(VIVE_PIN_BACK);
uint16_t viveXFront = 0, viveYFront = 0;
uint16_t viveXBack  = 0, viveYBack  = 0;
float viveAngle = 0.0f; // -180~180

static float normDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// 角度解包裹：把 [-180,180] 的角连续化为可累计角
static float unwrapDeg(float prevUnwrapped, float prevWrapped, float curWrapped) {
  float d = normDeg(curWrapped - prevWrapped);
  return prevUnwrapped + d;
}

static bool viveOk() {
  return (viveFront.getStatus() == VIVE_STATUS_RECEIVING &&
          viveBack.getStatus() == VIVE_STATUS_RECEIVING &&
          viveXFront > 0 && viveYFront > 0 &&
          viveXBack > 0 && viveYBack > 0);
}

// ========== 电机底层 ==========
static void setMotorL(int pwm) {
  pwm = constrain(pwm, -PWM_MAX, PWM_MAX);
  if (pwm >= 0) {
    digitalWrite(MOTOR_L_IN1, HIGH);
    digitalWrite(MOTOR_L_IN2, LOW);
    ledcWrite(MOTOR_L_PWM, pwm);
  } else {
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, HIGH);
    ledcWrite(MOTOR_L_PWM, -pwm);
  }
}

static void setMotorR(int pwm) {
  pwm = constrain(pwm, -PWM_MAX, PWM_MAX);
  if (pwm >= 0) {
    digitalWrite(MOTOR_R_IN1, HIGH);
    digitalWrite(MOTOR_R_IN2, LOW);
    ledcWrite(MOTOR_R_PWM, pwm);
  } else {
    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, HIGH);
    ledcWrite(MOTOR_R_PWM, -pwm);
  }
}

static void stopMotors() {
  setMotorL(0);
  setMotorR(0);
}

// dir = +1 右转（左轮前进，右轮后退）；dir = -1 左转
static void applyRotatePwm(int dir, int pwm) {
  pwm = constrain(pwm, 0, PWM_MAX);
  if (dir >= 0) {
    setMotorL(+pwm);
    setMotorR(-pwm);
  } else {
    setMotorL(-pwm);
    setMotorR(+pwm);
  }
}

// ========== 可调参数 ==========
float paramDefaultTargetDeg = 90.0f;
int   paramFastPwm          = 520;
int   paramSlowPwm          = 260;
float paramSlowStartDeg     = 25.0f;
float paramTolDeg           = 2.0f;
float paramLeadDeg          = 3.0f;
uint16_t paramBrakeMs       = 80;
int      paramBrakePwm      = 180;
uint16_t paramPrintMs       = 100;

// ========== 转向状态机 ==========
enum TurnState : uint8_t { TURN_IDLE = 0, TURN_ACTIVE = 1, TURN_BRAKE = 2 };
TurnState turnState = TURN_IDLE;
int turnDir = +1; // +1 右转，-1 左转
float targetDeg = 90.0f;

float aWrappedPrev = 0.0f;
float aUnwrapped = 0.0f;
float aStart = 0.0f;
float aTarget = 0.0f;

uint32_t brakeT0 = 0;
uint32_t lastPrintT = 0;

static void printParams() {
  Serial.println("=== TURN TEST PARAMS ===");
  Serial.printf("A(default)=%.2f deg\n", paramDefaultTargetDeg);
  Serial.printf("F=%d pwm, S=%d pwm\n", paramFastPwm, paramSlowPwm);
  Serial.printf("SW=%.2f deg, TOL=%.2f deg, LEAD=%.2f deg\n", paramSlowStartDeg, paramTolDeg, paramLeadDeg);
  Serial.printf("BRAKE=%u ms, %d pwm\n", (unsigned)paramBrakeMs, paramBrakePwm);
  Serial.printf("PR=%u ms\n", (unsigned)paramPrintMs);
}

static void printHelp() {
  Serial.println("\nCommands:");
  Serial.println("  HELP");
  Serial.println("  P");
  Serial.println("  STOP");
  Serial.println("  R[deg]   (e.g. R, R90, R45)");
  Serial.println("  L[deg]");
  Serial.println("  A=<deg>");
  Serial.println("  F=<pwm>  (0~1023)");
  Serial.println("  S=<pwm>  (0~1023)");
  Serial.println("  SW=<deg>");
  Serial.println("  TOL=<deg>");
  Serial.println("  LEAD=<deg>");
  Serial.println("  BRAKE=<ms>,<pwm>");
  Serial.println("  PR=<ms>");
}

static void startTurn(int dir, float deg) {
  if (!viveOk()) {
    Serial.println("[ERR] VIVE not ready (need both trackers RECEIVING + nonzero coords).");
    return;
  }
  turnDir = (dir >= 0) ? +1 : -1;
  targetDeg = fabsf(deg);
  if (targetDeg < 1.0f) targetDeg = 1.0f;

  // 以当前解包裹角为起点
  aStart = aUnwrapped;
  aTarget = aStart + (float)turnDir * targetDeg;

  turnState = TURN_ACTIVE;
  Serial.printf("[TURN] start %s %.2f deg | aStart=%.2f\n", (turnDir > 0) ? "RIGHT" : "LEFT", targetDeg, aStart);
}

static void cancelTurn() {
  turnState = TURN_IDLE;
  stopMotors();
  Serial.println("[TURN] stopped.");
}

static void stepTurn() {
  float delta = aUnwrapped - aStart;            // 已转角（带符号）
  float remaining = aTarget - aUnwrapped;       // 剩余角（带符号，目标-当前）
  float absRem = fabsf(remaining);

  if (!viveOk()) {
    cancelTurn();
    Serial.println("[ERR] VIVE lost during turn -> stop.");
    return;
  }

  if (turnState == TURN_ACTIVE) {
    // 达到目标（考虑提前量/容差）
    if (absRem <= max(paramTolDeg, paramLeadDeg)) {
      if (paramBrakeMs > 0 && paramBrakePwm > 0) {
        // 反向刹车：方向取 remaining 的反号（如果还没到目标，刹车方向与当前转向相反）
        int brakeDir = (remaining >= 0.0f) ? +1 : -1; // 需要往 remaining 的方向继续转；刹车则反向
        applyRotatePwm(-brakeDir, paramBrakePwm);
        brakeT0 = millis();
        turnState = TURN_BRAKE;
      } else {
        stopMotors();
        turnState = TURN_IDLE;
        Serial.printf("[TURN] done. delta=%.2f deg, err=%.2f deg\n", delta, absRem);
      }
      return;
    }

    // 选快/慢转
    int pwm = (absRem <= paramSlowStartDeg) ? paramSlowPwm : paramFastPwm;
    applyRotatePwm(turnDir, pwm);
    return;
  }

  if (turnState == TURN_BRAKE) {
    if ((millis() - brakeT0) >= paramBrakeMs) {
      stopMotors();
      turnState = TURN_IDLE;
      // 刹车后再读一次误差（下一次 loop 会更新 aUnwrapped）
      Serial.println("[TURN] brake done -> stop.");
    }
  }
}

// ========== 串口解析 ==========
static String readLine() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = buf;
      buf = "";
      line.trim();
      return line;
    }
    buf += c;
    if (buf.length() > 200) buf = ""; // 防爆
  }
  return "";
}

static void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  String u = line;
  u.toUpperCase();

  if (u == "HELP" || u == "H" || u == "?") { printHelp(); return; }
  if (u == "P") { printParams(); return; }
  if (u == "STOP") { cancelTurn(); return; }

  // Rxxx / Lxxx
  if (u[0] == 'R' || u[0] == 'L') {
    char d = u[0];
    float deg = paramDefaultTargetDeg;
    if (u.length() > 1) {
      deg = u.substring(1).toFloat();
      if (deg <= 0.0f) deg = paramDefaultTargetDeg;
    }
    startTurn((d == 'R') ? +1 : -1, deg);
    return;
  }

  // key=value
  int eq = u.indexOf('=');
  if (eq > 0) {
    String key = u.substring(0, eq);
    String val = u.substring(eq + 1);
    key.trim(); val.trim();

    if (key == "A") paramDefaultTargetDeg = val.toFloat();
    else if (key == "F") paramFastPwm = constrain(val.toInt(), 0, PWM_MAX);
    else if (key == "S") paramSlowPwm = constrain(val.toInt(), 0, PWM_MAX);
    else if (key == "SW") paramSlowStartDeg = max(0.0f, val.toFloat());
    else if (key == "TOL") paramTolDeg = max(0.0f, val.toFloat());
    else if (key == "LEAD") paramLeadDeg = max(0.0f, val.toFloat());
    else if (key == "PR") paramPrintMs = (uint16_t)max(20, val.toInt());
    else if (key == "BRAKE") {
      int comma = val.indexOf(',');
      if (comma > 0) {
        paramBrakeMs = (uint16_t)max(0, val.substring(0, comma).toInt());
        paramBrakePwm = constrain(val.substring(comma + 1).toInt(), 0, PWM_MAX);
      } else {
        Serial.println("[ERR] BRAKE format: BRAKE=<ms>,<pwm>");
      }
    } else {
      Serial.println("[ERR] Unknown key.");
    }
    printParams();
    return;
  }

  Serial.println("[ERR] Unknown command. Type HELP.");
}

// ========== Arduino ==========
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== TURN TEST (VIVE) ===");
  printHelp();
  printParams();

  // motor pins
  pinMode(MOTOR_L_IN1, OUTPUT);
  pinMode(MOTOR_L_IN2, OUTPUT);
  pinMode(MOTOR_R_IN1, OUTPUT);
  pinMode(MOTOR_R_IN2, OUTPUT);

  ledcAttach(MOTOR_L_PWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_R_PWM, PWM_FREQ, PWM_RESOLUTION);
  stopMotors();

  // vive
  viveFront.initialize();
  viveBack.initialize();

  // 初始化角度解包裹基准
  aWrappedPrev = 0.0f;
  aUnwrapped = 0.0f;
  aStart = 0.0f;
  aTarget = 0.0f;

  Serial.println("[INFO] Waiting for VIVE receiving...");
}

void loop() {
  // 1) VIVE 更新
  processViveData(viveFront, viveXFront, viveYFront);
  processViveData(viveBack,  viveXBack,  viveYBack);

  if (viveOk()) {
    float deltaX = float(viveXBack) - float(viveXFront);
    float deltaY = float(viveYBack) - float(viveYFront);
    float wrapped = (180.0f / PI) * atan2f(deltaY, deltaX) + VIVE_ANGLE_OFFSET;
    wrapped = normDeg(wrapped);

    // unwrap
    aUnwrapped = unwrapDeg(aUnwrapped, aWrappedPrev, wrapped);
    aWrappedPrev = wrapped;
    viveAngle = wrapped;
  }

  // 2) 串口命令
  String line = readLine();
  if (line.length() > 0) handleLine(line);

  // 3) 转向控制
  if (turnState != TURN_IDLE) stepTurn();

  // 4) 打印 telemetry
  uint32_t now = millis();
  if ((now - lastPrintT) >= paramPrintMs) {
    lastPrintT = now;
    if (viveOk()) {
      float delta = aUnwrapped - aStart;
      float remaining = aTarget - aUnwrapped;
      Serial.printf("[VIVE] a=%.2f (unwrap=%.2f) | delta=%.2f | rem=%.2f | state=%d | front=%d back=%d\n",
                    viveAngle, aUnwrapped, delta, remaining, (int)turnState,
                    viveFront.getStatus(), viveBack.getStatus());
    } else {
      Serial.printf("[VIVE] not ready | front=%d back=%d | x1=%u y1=%u x2=%u y2=%u\n",
                    viveFront.getStatus(), viveBack.getStatus(),
                    (unsigned)viveXFront, (unsigned)viveYFront,
                    (unsigned)viveXBack, (unsigned)viveYBack);
    }
  }
}


