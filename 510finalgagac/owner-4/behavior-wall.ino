// behavior-wall.ino
// 右侧巡墙逻辑：基于 3 路 ToF（前/右前/右后）实现 P 控制巡墙
// 版本：胡同倒车改变姿态 + 小半径转向 + P控制

#include <Arduino.h>

// 检查传感器读数是否有效
static bool isValid(uint16_t d) { return (d > 1 && d < 3000); }

// ============ 巡墙参数 ============
volatile bool WALL_FOLLOW_ENABLED = true;

float TOF_SPACING_MM = 160.0f;      // 两个右侧 ToF 在车身纵向的间距（mm）
float WALL_TARGET_DIST = 125.0f;    // 目标离墙距离（mm）
float WALL_DIST_KP     = 0.18f;     // 距离环 Kp
float WALL_ANGLE_KP    = 1.30f;     // 角度环 Kp
float FRONT_OBS_DIST   = 330.0f;    // 前方障碍阈值（mm）

int   WF_SPEED_FWD        = 60;     // 直行速度（0~100）
int   WF_MAX_TURN_RIGHT   = 100;     // 最大右转力度（0~100）
int   WF_MAX_TURN_LEFT    = 100;     // 最大左转力度（0~100）

float FRONT_PANIC_DIST = 65.0f;     // 前方紧急倒车距离（mm）
float WALL_LOST_DIST   = 340.0f;    // 丢墙判定距离（mm）

// ============ 胡同逃脱参数（倒车改变姿态 + 小半径转向）============
float ALLEY_FRONT_CLOSE   = 250.0f;   // 前方距离 < 这个值触发胡同检测
float ALLEY_RIGHT_CLOSE   = 190.0f;   // 右边距离 < 这个值触发胡同检测
int   ALLEY_BACKUP_SPEED  = 4;       // 倒车速度 (0~100)
int   ALLEY_BACKUP_TURN   = 20;       // 倒车时转向力度（向左打方向，让车尾向右靠墙）
unsigned long ALLEY_BACKUP_MS = 400;  // 倒车时间 (ms)
unsigned long ALLEY_CHECK_MS  = 25;  // 倒车后检测时间 (ms)
float ALLEY_FRONT_CLEAR   = 100.0f;   // 前方距离 > 这个值认为"可以走了"

// 出胡同后小半径转向参数
int   ALLEY_EXIT_TURN_STRENGTH = 0;  // 小半径右转力度 (0~100)
int   ALLEY_EXIT_FWD_SPEED     = 3;  // 边走边转的前进速度
unsigned long ALLEY_EXIT_TURN_MS = 400; // 小半径转向时间 (ms)
unsigned long ALLEY_EXIT_FWD_MS  = 200; // 转完后直行稳定时间 (ms)

// 出拐角序列时间（正常拐角，不是胡同）
unsigned long WF_SEQ_EXIT_STRAIGHT_MS = 350;
unsigned long WF_SEQ_EXIT_TURN_MS     = 450;
unsigned long WF_SEQ_EXIT_PAUSE_MS    = 200;

// 直行死区
int WF_TURN_DEADBAND = 6;

// 找墙参数
int   SEEK_TURN_STRENGTH    = 60;   // 找墙时原地右转力度 (0~100)
int   SEEK_FWD_SPEED        = 50;   // 找到墙后前进速度 (0~100)
float SEEK_WALL_FOUND_DIST  = 800.0f; // 前方 ToF < 这个值认为"看到墙了"
float SEEK_WALL_CLOSE_DIST  = 400.0f; // 前方 ToF < 这个值认为"靠近墙了"

// 调试变量
float debugWallAngleDeg = 0.0f;
float debugDistError    = 0.0f;
int   debugTurn         = 0;

// 状态枚举
enum WallFollowState {
    WF_NORMAL,
    WF_OBSTACLE_FRONT,
    WF_PANIC_BACKUP,
    WF_LOST_WALL,
    WF_CORNER,
    WF_EXITING,
    // 胡同逃脱（倒车改变姿态 + 小半径转向）
    WF_ALLEY_BACKUP,      // 胡同：倒车（带转向）
    WF_ALLEY_CHECK,       // 胡同：检测是否出来了
    WF_ALLEY_EXIT_TURN,   // 胡同：出来后小半径右转
    WF_ALLEY_EXIT_FWD,    // 胡同：转完后直行稳定
    // 找墙状态
    WF_SEEK_TURN,         // 找墙：原地右转（Pivot）对准墙
    WF_SEEK_FWD           // 找墙：向前走向墙
};

static WallFollowState wfState = WF_NORMAL;
static unsigned long wfSequenceStartTime = 0;
static bool wfExitSequenceActive = false;
static unsigned long alleyPhaseStartTime = 0;

// ===== debug/telemetry getters =====
uint8_t wf_getState() { return (uint8_t)wfState; }
float wf_getAngleDeg() { return debugWallAngleDeg; }
float wf_getDistError() { return debugDistError; }
int wf_getTurn() { return debugTurn; }

// 参数更新函数
void updateWallParam(const String& paramName, float value) {
    if (paramName == "WALL_FOLLOW_ENABLED") WALL_FOLLOW_ENABLED = (value > 0.5f);
    else if (paramName == "TOF_SPACING_MM") TOF_SPACING_MM = value;
    else if (paramName == "WALL_TARGET_DIST") WALL_TARGET_DIST = value;
    else if (paramName == "WALL_DIST_KP") WALL_DIST_KP = value;
    else if (paramName == "WALL_ANGLE_KP") WALL_ANGLE_KP = value;
    else if (paramName == "FRONT_OBS_DIST") FRONT_OBS_DIST = value;
    else if (paramName == "WF_SPEED_FWD") WF_SPEED_FWD = (int)value;
    else if (paramName == "WF_MAX_TURN_RIGHT") WF_MAX_TURN_RIGHT = (int)value;
    else if (paramName == "WF_MAX_TURN_LEFT") WF_MAX_TURN_LEFT = (int)value;
    else if (paramName == "FRONT_PANIC_DIST") FRONT_PANIC_DIST = value;
    else if (paramName == "WALL_LOST_DIST") WALL_LOST_DIST = value;
    // 胡同倒车参数
    else if (paramName == "ALLEY_FRONT_CLOSE") ALLEY_FRONT_CLOSE = value;
    else if (paramName == "ALLEY_RIGHT_CLOSE") ALLEY_RIGHT_CLOSE = value;
    else if (paramName == "ALLEY_BACKUP_SPEED") ALLEY_BACKUP_SPEED = (int)value;
    else if (paramName == "ALLEY_BACKUP_TURN") ALLEY_BACKUP_TURN = (int)value;
    else if (paramName == "ALLEY_BACKUP_MS") ALLEY_BACKUP_MS = (unsigned long)value;
    else if (paramName == "ALLEY_CHECK_MS") ALLEY_CHECK_MS = (unsigned long)value;
    else if (paramName == "ALLEY_FRONT_CLEAR") ALLEY_FRONT_CLEAR = value;
    // 出胡同后小半径转向参数
    else if (paramName == "ALLEY_EXIT_TURN_STRENGTH") ALLEY_EXIT_TURN_STRENGTH = (int)value;
    else if (paramName == "ALLEY_EXIT_FWD_SPEED") ALLEY_EXIT_FWD_SPEED = (int)value;
    else if (paramName == "ALLEY_EXIT_TURN_MS") ALLEY_EXIT_TURN_MS = (unsigned long)value;
    else if (paramName == "ALLEY_EXIT_FWD_MS") ALLEY_EXIT_FWD_MS = (unsigned long)value;
    // 出拐角序列
    else if (paramName == "WF_SEQ_EXIT_STRAIGHT_MS") WF_SEQ_EXIT_STRAIGHT_MS = (unsigned long)value;
    else if (paramName == "WF_SEQ_EXIT_TURN_MS") WF_SEQ_EXIT_TURN_MS = (unsigned long)value;
    else if (paramName == "WF_SEQ_EXIT_PAUSE_MS") WF_SEQ_EXIT_PAUSE_MS = (unsigned long)value;
    else if (paramName == "WF_TURN_DEADBAND") WF_TURN_DEADBAND = (int)value;
    // 找墙参数
    else if (paramName == "SEEK_TURN_STRENGTH") SEEK_TURN_STRENGTH = (int)value;
    else if (paramName == "SEEK_FWD_SPEED") SEEK_FWD_SPEED = (int)value;
    else if (paramName == "SEEK_WALL_FOUND_DIST") SEEK_WALL_FOUND_DIST = value;
    else if (paramName == "SEEK_WALL_CLOSE_DIST") SEEK_WALL_CLOSE_DIST = value;
}

static int clampInt(int v, int lo, int hi) { return constrain(v, lo, hi); }

// ============ 核心计算：P控制 + 状态机 ============
static int calculateWallFollowTurn(float F, float R1, float R2) {
    // 处理传感器数据
    bool d1Valid = (R1 > 30.0f && R1 < WALL_LOST_DIST);
    bool d2Valid = (R2 > 30.0f && R2 < WALL_LOST_DIST);
    float d1 = d1Valid ? R1 : 999.0f;  // 右前
    float d2 = d2Valid ? R2 : 999.0f;  // 右后

    bool frontValid   = (F > 0.0f && F < 2000.0f);
    bool frontPanic   = (frontValid && F < FRONT_PANIC_DIST);
    bool frontBlocked = (frontValid && F < FRONT_OBS_DIST);

    // 胡同检测：前方近 + 两个右边ToF都近（更严格的条件）
    // 只在正常状态下才检测胡同，避免误触发
    bool inAlley = false;
    if (wfState == WF_NORMAL) {
        bool frontNear = (frontValid && F < ALLEY_FRONT_CLOSE);
        bool rightBothNear = (R1 > 30.0f && R1 < ALLEY_RIGHT_CLOSE) && 
                             (R2 > 30.0f && R2 < ALLEY_RIGHT_CLOSE);
        inAlley = frontNear && rightBothNear;
    }

    // 出拐角检测（正常拐角）
    bool headOut = (d1 >= 999.0f);
    bool tailIn  = (d2 < WALL_LOST_DIST);
    bool isExitingCorner = (headOut && tailIn);

    // ========== 胡同逃脱状态机（倒车改变姿态 + 小半径转向）==========
    
    // 阶段1：倒车（带左转向，让车尾向右甩）
    if (wfState == WF_ALLEY_BACKUP) {
        unsigned long elapsed = millis() - alleyPhaseStartTime;
        if (elapsed < ALLEY_BACKUP_MS) {
            debugWallAngleDeg = 0; debugDistError = 0;
            debugTurn = -ALLEY_BACKUP_TURN;
            return -ALLEY_BACKUP_TURN;
        } else {
            // 倒车完成 → 进入检测阶段
            wfState = WF_ALLEY_CHECK;
            alleyPhaseStartTime = millis();
            debugTurn = 0;
            return 0;
        }
    }

    // 阶段2：检测前方是否清了
    if (wfState == WF_ALLEY_CHECK) {
        unsigned long elapsed = millis() - alleyPhaseStartTime;
        if (elapsed < ALLEY_CHECK_MS) {
            debugTurn = 0;
            return 0; // 停车检测
        } else {
            // 检测完毕：判断前方是否可以走了
            if (F >= ALLEY_FRONT_CLEAR) {
                // 前方空了 → 进入小半径转向阶段
                wfState = WF_ALLEY_EXIT_TURN;
                alleyPhaseStartTime = millis();
                debugTurn = ALLEY_EXIT_TURN_STRENGTH;
                return ALLEY_EXIT_TURN_STRENGTH;
            } else {
                // 前方还是堵 → 继续倒车
                wfState = WF_ALLEY_BACKUP;
                alleyPhaseStartTime = millis();
                return -ALLEY_BACKUP_TURN;
            }
        }
    }

    // 阶段3：出胡同后小半径右转（边走边转）
    if (wfState == WF_ALLEY_EXIT_TURN) {
        unsigned long elapsed = millis() - alleyPhaseStartTime;
        if (elapsed < ALLEY_EXIT_TURN_MS) {
            debugWallAngleDeg = 0; debugDistError = 0;
            debugTurn = ALLEY_EXIT_TURN_STRENGTH;
            return ALLEY_EXIT_TURN_STRENGTH;
        } else {
            // 转向完成 → 进入直行稳定阶段
            wfState = WF_ALLEY_EXIT_FWD;
            alleyPhaseStartTime = millis();
            debugTurn = 0;
            return 0;
        }
    }

    // 阶段4：转完后直行稳定
    if (wfState == WF_ALLEY_EXIT_FWD) {
        unsigned long elapsed = millis() - alleyPhaseStartTime;
        if (elapsed < ALLEY_EXIT_FWD_MS) {
            debugWallAngleDeg = 0; debugDistError = 0;
            debugTurn = 0;
            return 0; // 直行
        } else {
            // 稳定完成 → 恢复正常巡墙
            wfState = WF_NORMAL;
        }
    }

    // ========== 出拐角序列（正常拐角，不是胡同）==========
    if (wfExitSequenceActive) {
        wfState = WF_EXITING;
        unsigned long elapsed = millis() - wfSequenceStartTime;
        debugWallAngleDeg = 0; debugDistError = 0;

        if (elapsed < WF_SEQ_EXIT_STRAIGHT_MS) {
            debugTurn = 0;
            return 0; // 直行
        } else if (elapsed < WF_SEQ_EXIT_STRAIGHT_MS + WF_SEQ_EXIT_TURN_MS) {
            debugTurn = (int)(WF_MAX_TURN_RIGHT * 0.8f);
            return (int)(WF_MAX_TURN_RIGHT * 0.8f); // 右转
        } else if (elapsed < WF_SEQ_EXIT_STRAIGHT_MS + WF_SEQ_EXIT_TURN_MS + WF_SEQ_EXIT_PAUSE_MS) {
            debugTurn = 0;
            return 0; // 暂停
        } else {
            wfExitSequenceActive = false;
        }
    }

    if (isExitingCorner && !wfExitSequenceActive) {
        wfExitSequenceActive = true;
        wfSequenceStartTime = millis();
        wfState = WF_EXITING;
        debugTurn = 0;
        return 0;
    }

    // ========== 安全状态 ==========
    
    // 紧急倒车（贴脸了）
    if (frontPanic) {
        wfState = WF_PANIC_BACKUP;
        debugWallAngleDeg = 0; debugDistError = 0;
        debugTurn = 0;
        return 0;
    }

    // 检测到胡同（前方近 + 右边近）→ 触发倒车改变姿态
    if (inAlley) {
        wfState = WF_ALLEY_BACKUP;
        alleyPhaseStartTime = millis();
        debugWallAngleDeg = 0; debugDistError = 0;
        debugTurn = -ALLEY_BACKUP_TURN;
        return -ALLEY_BACKUP_TURN;
    }

    // 前方被堵但不是胡同（只有前方近，右边没墙）→ 简单左转避障
    if (frontBlocked) {
        wfState = WF_OBSTACLE_FRONT;
        debugWallAngleDeg = 0; debugDistError = 0;
        debugTurn = -WF_MAX_TURN_LEFT;
        return -WF_MAX_TURN_LEFT;
    }

    // ========== 找墙状态机 ==========
    
    // 原地右转找墙
    if (wfState == WF_SEEK_TURN) {
        debugWallAngleDeg = 0; debugDistError = 0;
        if (F < SEEK_WALL_FOUND_DIST) {
            wfState = WF_SEEK_FWD;
            debugTurn = 0;
            return 0;
        }
        debugTurn = SEEK_TURN_STRENGTH;
        return SEEK_TURN_STRENGTH;
    }

    // 向前走向墙
    if (wfState == WF_SEEK_FWD) {
        debugWallAngleDeg = 0; debugDistError = 0;
        if (F < SEEK_WALL_CLOSE_DIST) {
            wfState = WF_NORMAL;
        }
        if (d1 < WALL_LOST_DIST || d2 < WALL_LOST_DIST) {
            wfState = WF_NORMAL;
        }
        debugTurn = 0;
        return 0;
    }

    // 两边都丢墙 → 进入找墙模式
    if (d1 >= 999.0f && d2 >= 999.0f) {
        wfState = WF_SEEK_TURN;
        debugWallAngleDeg = 0; debugDistError = 0;
        debugTurn = SEEK_TURN_STRENGTH;
        return SEEK_TURN_STRENGTH;
    }

    // ========== 正常 P 控制巡墙 ==========
    wfState = WF_NORMAL;

    // 估算墙角度
    float wallAngleDeg = 0.0f;
    if (d1Valid && d2Valid) {
        float diff = d1 - d2;
        float wallAngleRad = atan2f(diff, max(1.0f, TOF_SPACING_MM));
        wallAngleDeg = wallAngleRad * 180.0f / PI;
    }

    // 距离误差
    float distError = WALL_TARGET_DIST - d1;

    debugWallAngleDeg = wallAngleDeg;
    debugDistError = distError;

    // P 控制公式
    float turn = (WALL_DIST_KP * (d1 - WALL_TARGET_DIST)) + (WALL_ANGLE_KP * wallAngleDeg);
    int ti = (int)lroundf(turn);
    debugTurn = clampInt(ti, -WF_MAX_TURN_LEFT, WF_MAX_TURN_RIGHT);
    return debugTurn;
}

// ============ 主决策函数 ============
String decideWallFollowing(uint16_t F_raw, uint16_t R1_raw, uint16_t R2_raw) {
    bool hasF  = isValid(F_raw);
    bool hasR1 = isValid(R1_raw);
    bool hasR2 = isValid(R2_raw);
    if (!hasF && !hasR1 && !hasR2) return "S";

    float F  = hasF  ? (float)F_raw  : 5000.0f;
    float R1 = hasR1 ? (float)R1_raw : 5000.0f;
    float R2 = hasR2 ? (float)R2_raw : 5000.0f;

    if (!WALL_FOLLOW_ENABLED) {
        wfState = WF_NORMAL;
        return "S";
    }

    int turn = calculateWallFollowTurn(F, R1, R2);

    // ========== 根据状态输出命令 ==========

    // 胡同逃脱：倒车（BL = 倒车左转，车尾向右甩）
    if (wfState == WF_ALLEY_BACKUP) {
        return "BL" + String(ALLEY_BACKUP_SPEED);
    }

    // 胡同逃脱：检测阶段（停车）
    if (wfState == WF_ALLEY_CHECK) {
        return "S";
    }

    // 胡同逃脱：出胡同后小半径右转（R = 边走边右转）
    if (wfState == WF_ALLEY_EXIT_TURN) {
        return "R" + String(ALLEY_EXIT_TURN_STRENGTH);
    }

    // 胡同逃脱：转完后直行稳定
    if (wfState == WF_ALLEY_EXIT_FWD) {
        return "F" + String(ALLEY_EXIT_FWD_SPEED);
    }

    // 前方障碍（简单左转）
    if (wfState == WF_OBSTACLE_FRONT) {
        return "PL" + String(WF_MAX_TURN_LEFT);
    }

    // 找墙
    if (wfState == WF_SEEK_TURN) {
        return "PR" + String(SEEK_TURN_STRENGTH);
    }
    if (wfState == WF_SEEK_FWD) {
        return "F" + String(SEEK_FWD_SPEED);
    }

    // 紧急倒车
    if (wfState == WF_PANIC_BACKUP) {
        return "B30";
    }

    // 出拐角序列
    if (wfState == WF_EXITING) {
        unsigned long elapsed = millis() - wfSequenceStartTime;
        if (elapsed < WF_SEQ_EXIT_STRAIGHT_MS) return "F" + String(WF_SPEED_FWD);
        if (elapsed < WF_SEQ_EXIT_STRAIGHT_MS + WF_SEQ_EXIT_TURN_MS) return "R" + String(abs(turn));
        return "S";
    }

    // 正常巡墙：根据 turn 值决定（边走边转）
    int dead = WF_TURN_DEADBAND;
    if (dead < 0) dead = 0;
    if (abs(turn) <= dead) {
        return "F" + String(WF_SPEED_FWD);
    }

    if (turn > 0) return "R" + String(turn);
    return "L" + String(-turn);
}
