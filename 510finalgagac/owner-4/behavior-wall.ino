// behavior-wall.ino
// wall following logic (右侧巡墙逻辑)

#include <Arduino.h>

// 检查传感器读数是否有效 (过滤掉 0 或 异常大的噪音)
static bool isValid(uint16_t d) {
    return (d > 1 && d < 3000);
}



// 1. Distance Thresholds (距离阈值设置)
const uint16_t FRONT_TURN_TH   = 220;   // 前方遇到障碍的阈值 (22cm)，小于这个距离就开始左转
const uint16_t FRONT_BACKUP_TH = 30;    // [紧急] 贴脸阈值 (3cm)，小于这个距离说明快撞上了，必须倒车

// Simplified Wall Following Thresholds (巡墙距离参数)
const float WALL_TOO_CLOSE      = 50.0f;  // < 5cm: 危险距离！必须立刻左转远离
const float WALL_IDEAL          = 90.0f;  // 理想距离 (9cm)
const float WALL_TOO_FAR        = 130.0f; // > 13cm: 太远了，需要往右靠

const float RIGHT_LOST_WALL     = 200.0f; // 超过 20cm 认为墙没了 (或者出胡同了)

// 2. Speed and Turn Strength (速度与转向力度)
const uint8_t SPEED_FWD        = 30;    // 前进基础速度
const uint8_t SPEED_BACK       = 25;    // 倒车速度

const uint8_t TURN_SPIN        = 102;   // 原地左转力度 (用于前方避障)
const uint8_t TURN_CORRECT     = 30;    // 左转修正力度 
const uint8_t TURN_GENTLE      = 27;    // 右转找墙力度 (离远了慢慢靠过去)
const uint8_t TURN_HARD_FIND   = 110;   // [猛烈] 出胡同时的强力右转力度

const uint8_t TURN_TINY        = 10;    // 微调力度
const float ANGLE_ERR_TH       = 8.0f;  // 角度误差容忍度 (降低敏感度，防止画龙)

// Time Limits (时间限制)
const unsigned long MAX_BACKUP_MS = 600; // 倒车最长时间 (0.6秒)，防止一直倒撞到后面

// Sequences (固定动作序列的时间设置 - 毫秒)
// 出胡同序列:
const unsigned long SEQ_EXIT_STRAIGHT_MS = 300; // 第一步: 强制直行 300ms (把车身带出来，防刮蹭)
const unsigned long SEQ_EXIT_TURN_MS     = 400; // 第二步: 强力右转 400ms (盲转找墙)
const unsigned long SEQ_EXIT_STOP_MS     = 600; // 第三步: 停车观察 400ms (消除惯性)
// 前方避障序列:
const unsigned long SEQ_FRONT_TURN_MS    = 600; // 遇到前墙: 闭眼左转 600ms
const unsigned long SEQ_FRONT_STOP_MS    = 450; // 转完后: 停车冷静 300ms

String decideWallFollowing(uint16_t F_raw, uint16_t R1_raw, uint16_t R2_raw) {
    // 静态变量 (Static Variables) - 用于记住上一轮循环的状态
    static unsigned long backupStartTime = 0;
    static bool isBackingUp = false;           // 是否正在倒车
    static unsigned long exitStartTime = 0;
    static bool isExitingSequenceActive = false; // 是否正在执行"出胡同"序列
    static unsigned long frontTurnStartTime = 0;
    static bool isFrontTurnSequenceActive = false; // 是否正在执行"前方避障"序列

    // 检查传感器状态
    bool hasF  = isValid(F_raw);
    bool hasR1 = isValid(R1_raw);
    bool hasR2 = isValid(R2_raw);

    // Startup protection (启动保护)
    // 如果三个传感器都读不到数 (比如刚开机)，原地不动，防止瞎跑
    if (!hasF && !hasR1 && !hasR2) { return "S"; }

    // 数据清洗: 如果读数无效，设为 5000 (无穷远)
    float F  = (hasF && F_raw < 5000) ? F_raw : 5000.0f;
    float R1 = hasR1 ? R1_raw : 5000.0f;
    float R2 = hasR2 ? R2_raw : 5000.0f; 

    // 判断车头是否已经探出墙外 (R1 读数很大)
    bool isHeadOut = (R1 > RIGHT_LOST_WALL); 

    // ================= LOGIC CORE (逻辑核心) =================

    // 0. PANIC BACKUP (紧急倒车)
    // 只有在真的快撞上 (>3cm) 时才触发
    if (F < FRONT_BACKUP_TH) {
        if (!isBackingUp) { isBackingUp = true; backupStartTime = millis(); }
        // 倒车超时保护: 如果倒了 600ms 还没退出来，强制左转 (赌一把)
        if (millis() - backupStartTime > MAX_BACKUP_MS) { 
            isFrontTurnSequenceActive = true; 
            frontTurnStartTime = millis();
            return "L" + String(TURN_SPIN); 
        }
        return "B" + String(SPEED_BACK);
    } else { isBackingUp = false; }

    // 1. FRONT TURN SEQUENCE (前方避障序列)
    // 触发条件: 前方 < 22cm 或者 序列已经被激活
    if (F <= FRONT_TURN_TH || isFrontTurnSequenceActive) {
        if (!isFrontTurnSequenceActive) { isFrontTurnSequenceActive = true; frontTurnStartTime = millis(); }
        unsigned long dt = millis() - frontTurnStartTime;
        
        // 阶段A: 闭眼左转 (避免传感器盲区导致转过头)
        if (dt < SEQ_FRONT_TURN_MS) { return "L" + String(TURN_SPIN); }
        // 阶段B: 停车冷静 (让传感器数据稳定)
        else if (dt < (SEQ_FRONT_TURN_MS + SEQ_FRONT_STOP_MS)) { return "S"; }
        // 阶段C: 序列结束
        else { isFrontTurnSequenceActive = false; }
        
        if (isFrontTurnSequenceActive) return "L" + String(TURN_SPIN);
    }

    // 2. EXIT ALLEY SEQUENCE (出胡同序列)
    // 触发条件: 车头出去了(R1空) 并且 屁股还在(R2有) -> 说明正在出弯
    if ((isHeadOut && R2 <= RIGHT_LOST_WALL) || isExitingSequenceActive) {
        if (!isHeadOut && !isExitingSequenceActive) { /* 误判过滤，忽略 */ }
        else {
            if (!isExitingSequenceActive) { isExitingSequenceActive = true; exitStartTime = millis(); }
            unsigned long dt = millis() - exitStartTime;
            
            // 阶段A: 强制直行 300ms (防止车身侧面刮到墙角)
            if (dt < SEQ_EXIT_STRAIGHT_MS) { return "F" + String(SPEED_FWD); }
            // 阶段B: 猛烈右转 400ms (因为已经直行了一段，现在要猛拐去抓新墙)
            else if (dt < (SEQ_EXIT_STRAIGHT_MS + SEQ_EXIT_TURN_MS)) { return "R" + String(TURN_HARD_FIND); }
            // 阶段C: 停车 400ms (消除惯性)
            else if (dt < (SEQ_EXIT_STRAIGHT_MS + SEQ_EXIT_TURN_MS + SEQ_EXIT_STOP_MS)) { return "S"; }
            // 阶段D: 结束
            else { isExitingSequenceActive = false; }
            
            if (isExitingSequenceActive) return "F" + String(SPEED_FWD);
        }
    }

    // ========================================================
    // 3. SIMPLIFIED WALL FOLLOWING (简化版巡墙逻辑 - 安全优先)
    // ========================================================
    // 只有在车头还在墙里时 (没出胡同)，才执行此逻辑
    if (!isHeadOut) {
        // PRIORITY 1: SAFETY (安全第一)
        // 如果距离小于 5cm，不管角度如何，必须立刻左转远离！
        if (R1 < WALL_TOO_CLOSE) {
            return "L" + String(TURN_CORRECT);
        }

        // PRIORITY 2: ALIGNMENT (车头扎进去了)
        // 如果 车头距离(R1) < 车尾距离(R2)，说明车正在往墙上撞
        // 必须立刻左转回正
        if (R1 < R2 - 5.0f) { // 5mm 缓冲
            return "L" + String(TURN_CORRECT);
        }

        // PRIORITY 3: TOO FAR (离太远了)
        // 只有在安全的情况下 (>5cm 且 车头没扎墙)，才考虑拉近距离
        if (R1 > WALL_TOO_FAR) {
            return "R" + String(TURN_GENTLE);
        }

        // PRIORITY 4: NOSE POINTING OUT (车头朝外)
        // 车头距离 > 车尾距离，说明车正在远离墙，微调右转保持平行
        if (R1 > R2 + 5.0f) {
            return "R" + String(TURN_TINY);
        }

        // PRIORITY 5: TINY ADJUSTMENTS (微调)
        // 保持绝对直线行驶
        float angle = R1 - R2;
        if (angle < -3.0f) return "L" + String(TURN_TINY); // 稍微向里偏了，修一点
        // 移除了向外偏的微调，防止震荡撞墙
    }
    else {
        // Head is out (车头在空地)
        // 上面的序列没触发，说明前后都没墙(开阔地)，画个温柔的大圈找墙
        return "R" + String(20);
    }

    // 完美状态，直行
    return "F" + String(SPEED_FWD);
}