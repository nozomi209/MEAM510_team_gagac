/*
 * behavior-wall.ino — 右侧巡墙/避障行为（基于 3 路 ToF：F/R1/R2）
 *
 * 本文件主要包含：
 * - `decideWallFollowing(F_raw, R1_raw, R2_raw)`：
 *    输入三路 ToF 距离（单位 mm），输出一条底盘控制命令字符串：
 *    "Fxx" 前进、"Lxx"/"Rxx" 转向、"Bxx" 后退、"S" 停车。
 *
 * 算法大体结构：
 * 1) 读数有效性/滤波与保护
 *    - 对异常读数做 isValid 判断；全无数据时直接停车
 *    - 前向突降/坡度导致的“假障碍”做抑制（避免误触发前向避障）
 *
 * 2) 前向避障优先（保命逻辑）
 *    - 前方太近：停车/倒车/原地转向（包含多阶段的转角通过序列）
 *
 * 3) 常规巡墙（右侧贴墙）
 *    - R1 负责距离控制（太近左转、太远右转）
 *    - R2 辅助平行控制（R1 vs R2 的差值用于微调车身姿态）
 *
 * 4) 卡死检测/脱困序列
 *    - 通过“2 秒内位移过小”判定卡死
 *    - 触发倒车+转向等脱困动作序列
 *
 * 参数说明：
 * - 文件顶部定义了大量阈值与力度参数（距离阈值、速度、转向强度、确认帧数等），
 *   可在现场按 ToF 噪声与墙面反射情况调参。
 *
 * 调用关系：
 * - `owner-4.ino` 负责读取 ToF 并在 AUTO 模式下调用本函数，然后通过 UART 下发给 Servant 执行。
 */

// behavior-wall.ino
// 右侧巡墙逻辑：基于 3 路 ToF（前/右前/右后）实现避障与贴墙，参数可实时调整



// if delta F from F now and F previous, is > 500
//goind down slope -> b


#include <Arduino.h>

// 检查传感器读数是否有效
static bool isValid(uint16_t d) { return (d > 1 && d < 3000); }

// ============ 参数设置 (可实时调整) ============
// 前方避障参数
uint16_t FRONT_TURN_TH   = 250;   // 前方避障距离 (17cm)
uint16_t FRONT_BACKUP_TH = 50;    // 紧急倒车距离 (5cm)
uint8_t  FRONT_CONFIRM_FRAMES = 3; // 连续 N 帧才认定前方有障碍
uint16_t FRONT_SLOPE_SUPPRESS_MAX = 400; // 下坡假障碍抑制上限（只靠前向突降判据）
uint16_t FRONT_DROP_GUARD_MM = 120;      // 单帧突降超过该值先忽略一帧

// 巡墙距离参数
float WALL_TOO_CLOSE      = 50.0f;  // <6cm: 太近 (危险)
float WALL_IDEAL          = 80.0f;  // 9cm: 理想距离
float WALL_TOO_FAR        = 120.0f; // >13cm: 太远
float RIGHT_LOST_WALL     = 200.0f; // >20cm: 认为出胡同或丢墙

// 速度参数
uint8_t SPEED_FWD        = 50;    // 前进速度
uint8_t SPEED_BACK       = 25;    // 倒车速度

// 转向力度参数
uint8_t TURN_SPIN        = 120;   // 原地旋转力度 (前方避障)
uint8_t TURN_CORRECT     = 12;    // 左转修正 (离墙太近)
uint8_t TURN_GENTLE      = 12;    // 右转找墙 (离墙太远)
uint8_t TURN_HARD_FIND   = 120;   // 强力找墙 (出胡同)
uint8_t TURN_TINY        = 10;    // 微调 (保持平行)

// 卡死检测参数
unsigned long STALL_CHECK_TIME = 2000; // 每2秒检查一次是否卡死
int STALL_MOVE_TH = 20; // 2秒内移动小于20mm视为卡死

// 动作序列时间 (毫秒)
unsigned long MAX_BACKUP_MS = 600; 
unsigned long SEQ_EXIT_STRAIGHT_MS = 6; // 出胡同: 先直行
unsigned long SEQ_EXIT_TURN_MS     = 100; // 出胡同: 再强转
unsigned long SEQ_EXIT_STOP_MS     = 100; // 出胡同: 后停车

// 前方避障序列 (停 -> 转 -> 停)
unsigned long SEQ_FRONT_PRE_STOP_MS  = 100; 
unsigned long SEQ_FRONT_TURN_MS      = 200; 
unsigned long SEQ_FRONT_POST_STOP_MS = 100; 

// 脱困序列 (倒车 -> 旋转)
unsigned long SEQ_STUCK_BACK_MS = 800;  
unsigned long SEQ_STUCK_TURN_MS = 100;

// 参数更新函数（从UART接收参数）
void updateWallParam(const String& paramName, float value) {
    if (paramName == "FRONT_TURN_TH") FRONT_TURN_TH = (uint16_t)value;
    else if (paramName == "FRONT_BACKUP_TH") FRONT_BACKUP_TH = (uint16_t)value;
    else if (paramName == "FRONT_CONFIRM_FRAMES") FRONT_CONFIRM_FRAMES = (uint8_t)value;
    else if (paramName == "FRONT_SLOPE_SUPPRESS_MAX") FRONT_SLOPE_SUPPRESS_MAX = (uint16_t)value;
    else if (paramName == "FRONT_DROP_GUARD_MM") FRONT_DROP_GUARD_MM = (uint16_t)value;
    else if (paramName == "WALL_TOO_CLOSE") WALL_TOO_CLOSE = value;
    else if (paramName == "WALL_IDEAL") WALL_IDEAL = value;
    else if (paramName == "WALL_TOO_FAR") WALL_TOO_FAR = value;
    else if (paramName == "RIGHT_LOST_WALL") RIGHT_LOST_WALL = value;
    else if (paramName == "SPEED_FWD") SPEED_FWD = (uint8_t)value;
    else if (paramName == "SPEED_BACK") SPEED_BACK = (uint8_t)value;
    else if (paramName == "TURN_SPIN") TURN_SPIN = (uint8_t)value;
    else if (paramName == "TURN_CORRECT") TURN_CORRECT = (uint8_t)value;
    else if (paramName == "TURN_GENTLE") TURN_GENTLE = (uint8_t)value;
    else if (paramName == "TURN_HARD_FIND") TURN_HARD_FIND = (uint8_t)value;
    else if (paramName == "TURN_TINY") TURN_TINY = (uint8_t)value;
    else if (paramName == "STALL_CHECK_TIME") STALL_CHECK_TIME = (unsigned long)value;
    else if (paramName == "STALL_MOVE_TH") STALL_MOVE_TH = (int)value;
    else if (paramName == "MAX_BACKUP_MS") MAX_BACKUP_MS = (unsigned long)value;
    else if (paramName == "SEQ_EXIT_STRAIGHT_MS") SEQ_EXIT_STRAIGHT_MS = (unsigned long)value;
    else if (paramName == "SEQ_EXIT_TURN_MS") SEQ_EXIT_TURN_MS = (unsigned long)value;
    else if (paramName == "SEQ_EXIT_STOP_MS") SEQ_EXIT_STOP_MS = (unsigned long)value;
    else if (paramName == "SEQ_FRONT_PRE_STOP_MS") SEQ_FRONT_PRE_STOP_MS = (unsigned long)value;
    else if (paramName == "SEQ_FRONT_TURN_MS") SEQ_FRONT_TURN_MS = (unsigned long)value;
    else if (paramName == "SEQ_FRONT_POST_STOP_MS") SEQ_FRONT_POST_STOP_MS = (unsigned long)value;
    else if (paramName == "SEQ_STUCK_BACK_MS") SEQ_STUCK_BACK_MS = (unsigned long)value;
    else if (paramName == "SEQ_STUCK_TURN_MS") SEQ_STUCK_TURN_MS = (unsigned long)value;
}

String decideWallFollowing(uint16_t F_raw, uint16_t R1_raw, uint16_t R2_raw) {
    // 状态记录变量
    static unsigned long backupStartTime = 0;
    static bool isBackingUp = false;           
    static unsigned long exitStartTime = 0;
    static bool isExitingSequenceActive = false; 

    // 前方避障变量
    static bool isFrontTurnSequenceActive = false; 
    static uint8_t frontSeqStage = 0; 
    static unsigned long stageStartTime = 0;

    // 卡死检测变量
    static unsigned long lastStallCheckTime = 0;
    static uint16_t lastF = 0, lastR1 = 0;
    static bool isStuckSequenceActive = false;
    static unsigned long stuckStartTime = 0;

    // 下坡/抖动抑制用的前方滤波与计数
    static float F_filt = 5000.0f;
    static float F_prev = 5000.0f;
    static uint8_t frontLowCount = 0;

    // 1. 数据预处理
    bool hasF  = isValid(F_raw);
    bool hasR1 = isValid(R1_raw);
    bool hasR2 = isValid(R2_raw);

    if (!hasF && !hasR1 && !hasR2) { return "S"; } // 启动保护: 全无数据则停车

    float F  = (hasF && F_raw < 5000) ? F_raw : 5000.0f;
    float R1 = hasR1 ? R1_raw : 5000.0f;
    float R2 = hasR2 ? R2_raw : 5000.0f; 
    bool isHeadOut = (R1 > RIGHT_LOST_WALL); 

    // 前向距离低通 + 单帧突降守门：只有前向可用时，以时间连续性抑制下坡假障碍
    F_filt = 0.6f * F_filt + 0.4f * F;
    if ((F_prev - F_filt) > FRONT_DROP_GUARD_MM && F_filt < FRONT_SLOPE_SUPPRESS_MAX) {
        // 认为是姿态/抖动造成的瞬时突降，先不触发，拉回到上次值
        F_filt = F_prev;
    }
    float F_use = (F < F_filt) ? F : F_filt; // 贴脸保护仍看原始更小值
    frontLowCount = (F_filt < FRONT_TURN_TH) ? (uint8_t)(frontLowCount + 1) : 0;
    bool frontConfirmed = frontLowCount >= FRONT_CONFIRM_FRAMES;
    F_prev = F_filt;


    // ================= [优先级 1] 卡死脱困序列 (倒车 -> 旋转) =================
    // 触发后强制执行，用于从障碍物中拔出来
    if (isStuckSequenceActive) {
        unsigned long dt = millis() - stuckStartTime;
        if (dt < SEQ_STUCK_BACK_MS) { return "B" + String(SPEED_BACK + 10); } // 强力倒车
        else if (dt < (SEQ_STUCK_BACK_MS + SEQ_STUCK_TURN_MS)) { return "L" + String(TURN_SPIN); } // 换个方向
        else { 
            isStuckSequenceActive = false; 
            lastStallCheckTime = millis(); 
            lastF = F; lastR1 = R1;
        }
        return "B" + String(SPEED_BACK);
    }

    // ================= [逻辑核心] 卡死检测 =================
    if (millis() - lastStallCheckTime > STALL_CHECK_TIME) {
        int deltaF = abs((int)F - (int)lastF);
        bool isBusy = (isFrontTurnSequenceActive || isExitingSequenceActive || isBackingUp);
        
        // 如果2秒内没怎么动 (delta < 2cm) 且没在执行其他任务
        if (!isBusy && deltaF < STALL_MOVE_TH) {
            // 判定为卡死，触发脱困序列 (倒车)
            isStuckSequenceActive = true;
            stuckStartTime = millis();
            return "B" + String(SPEED_BACK); 
        }
        // 更新历史位置
        lastF = F; lastR1 = R1;
        lastStallCheckTime = millis();
    }

    // ================= [优先级 2] 常规动作 =================

    // 0. 贴脸保护 (距离 < 5cm)
    if (F_use < FRONT_BACKUP_TH) {
        if (!isBackingUp) { isBackingUp = true; backupStartTime = millis(); }
        if (millis() - backupStartTime > MAX_BACKUP_MS) { 
            // 倒车超时 -> 强制转弯序列
            isFrontTurnSequenceActive = true; 
            frontSeqStage = 1; 
            stageStartTime = millis();
            return "S"; 
        }
        return "B" + String(SPEED_BACK);
    } else { isBackingUp = false; }

    // 1. 前方避障序列 (停 -> 盲转 -> 停)
    if ((frontConfirmed && F_filt <= FRONT_TURN_TH && R1 <= 120.0F) || isFrontTurnSequenceActive) {
        if (!isFrontTurnSequenceActive) { isFrontTurnSequenceActive = true; frontSeqStage = 1; stageStartTime = millis(); }
        unsigned long dt = millis() - stageStartTime;

        // 阶段1: 停车稳住
        if (frontSeqStage == 1) {
            if (dt < SEQ_FRONT_PRE_STOP_MS) return "S"; 
            else { frontSeqStage = 2; stageStartTime = millis(); return "L" + String(TURN_SPIN); }
        }
        // 阶段2: 闭眼转弯
        if (frontSeqStage == 2) {
            if (dt < SEQ_FRONT_TURN_MS) return "L" + String(TURN_SPIN);
            else { frontSeqStage = 3; stageStartTime = millis(); return "S"; }
        }
        // 阶段3: 转后观察
        if (frontSeqStage == 3) {
            if (dt < SEQ_FRONT_POST_STOP_MS) return "S"; 
            else { isFrontTurnSequenceActive = false; frontSeqStage = 0; }
        }
        return "S";
    }


    if (frontConfirmed && F_filt <= FRONT_TURN_TH && R1 > 120.0F) {
        return "R" + String(TURN_HARD_FIND); // 前方近、右侧空：大幅右找出口
    }

    // 2. 出胡同序列 (防撞角: 直行 -> 猛拐 -> 停)
    if ((isHeadOut && R2 <= RIGHT_LOST_WALL) || isExitingSequenceActive) {
        if (!isHeadOut && !isExitingSequenceActive) { }
        else {
            if (!isExitingSequenceActive) { isExitingSequenceActive = true; exitStartTime = millis(); }
            unsigned long dt = millis() - exitStartTime;
            if (dt < SEQ_EXIT_STRAIGHT_MS) { return "F" + String(SPEED_FWD); }
            else if (dt < (SEQ_EXIT_STRAIGHT_MS + SEQ_EXIT_TURN_MS)) { return "R" + String(TURN_HARD_FIND); }
            else if (dt < (SEQ_EXIT_STRAIGHT_MS + SEQ_EXIT_TURN_MS + SEQ_EXIT_STOP_MS)) { return "S"; }
            else { isExitingSequenceActive = false; }
            if (isExitingSequenceActive) return "F" + String(SPEED_FWD);
        }
    }

    // 3. 常规巡墙 (简化版: R1主导，R2辅助)
    if (!isHeadOut) {
        float diff = R1 - R2;
        
        // [特殊保护] 刚转过直角弯，车尾(R2)还在后面很远，diff 是负的大数
        // 此时强制直行，防止误判为"车头扎墙"
        if (diff < -100.0f) { return "F" + String(SPEED_FWD); }

        // ============ 第一层：只看 R1 (距离控制) ============
        
        // 1. 太近了 (R1 < 6cm) -> 必须左转远离 (保命)
        if (R1 < WALL_TOO_CLOSE) { 
            return "L" + String(TURN_CORRECT); 
        }

        // 2. 太远了 (R1 > 13cm) -> 必须右转去找墙 (防丢)
        if (R1 > WALL_TOO_FAR) { 
            return "R" + String(TURN_GENTLE); 
        }

        // ============ 第二层：只看 R1 vs R2 (平行控制) ============
        // 能走到这里，说明 R1 距离适中 (在 6cm ~ 13cm 之间)，很安全
        // 这时候我们才用 R2 来微调车身姿态
        
        // 3. 车头扎向墙 (R1 比 R2 小，说明车头歪进去了)
        // 稍微左转一点点回正
        if (diff < -5.0f) { 
            return "L" + String(TURN_TINY); 
        }

        // 4. 车头撇向外 (R1 比 R2 大，说明车头歪出去了)
        // 稍微右转一点点回正
        if (diff > 5.0f) { 
            return "R" + String(TURN_TINY); 
        }
        
        // 5. 既不近也不远，而且是平行的 -> 完美，直行
        return "F" + String(SPEED_FWD);
    }
    else { 
        // 6. 找不到墙 (空地模式) -> 画大圈找墙
        return "R" + String(30); 
    }

    return "F" + String(SPEED_FWD);
}