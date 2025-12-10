//昨晚最新版
// behavior-wall.ino
// 右侧巡墙逻辑 (Right Wall Following)

#include <Arduino.h>

// 检查传感器读数是否有效
static bool isValid(uint16_t d) { return (d > 1 && d < 3000); }

// ============ 参数设置 ============
const uint16_t FRONT_TURN_TH   = 250;   // 前方避障距离 (17cm)
const uint16_t FRONT_BACKUP_TH = 50;    // 紧急倒车距离 (5cm)

// 巡墙距离参数
const float WALL_TOO_CLOSE      = 50.0f;  // <6cm: 太近 (危险)
const float WALL_IDEAL          = 80.0f;  // 9cm: 理想距离
const float WALL_TOO_FAR        = 120.0f; // >13cm: 太远
const float RIGHT_LOST_WALL     = 200.0f; // >20cm: 认为出胡同或丢墙

// 速度参数
const uint8_t SPEED_FWD        = 50;    // 前进速度
const uint8_t SPEED_BACK       = 25;    // 倒车速度

// 转向力度参数
const uint8_t TURN_SPIN        = 105;   // 原地旋转力度 (前方避障)
const uint8_t TURN_CORRECT     = 12;    // 左转修正 (离墙太近)
const uint8_t TURN_GENTLE      = 12;    // 右转找墙 (离墙太远)
const uint8_t TURN_HARD_FIND   = 120;   // 强力找墙 (出胡同)
const uint8_t TURN_TINY        = 10;    // 微调 (保持平行)

// 卡死检测参数
const unsigned long STALL_CHECK_TIME = 2000; // 每2秒检查一次是否卡死
const int STALL_MOVE_TH = 20; // 2秒内移动小于20mm视为卡死

// 动作序列时间 (毫秒)
const unsigned long MAX_BACKUP_MS = 600; 
const unsigned long SEQ_EXIT_STRAIGHT_MS =6; // 出胡同: 先直行
const unsigned long SEQ_EXIT_TURN_MS     = 300; // 出胡同: 再强转
const unsigned long SEQ_EXIT_STOP_MS     = 100; // 出胡同: 后停车

// 前方避障序列 (停 -> 转 -> 停)
const unsigned long SEQ_FRONT_PRE_STOP_MS  = 100; 
const unsigned long SEQ_FRONT_TURN_MS      = 300; 
const unsigned long SEQ_FRONT_POST_STOP_MS = 100; 

// 脱困序列 (倒车 -> 旋转)
const unsigned long SEQ_STUCK_BACK_MS = 800;  
const unsigned long SEQ_STUCK_TURN_MS = 500;

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

    // 1. 数据预处理
    bool hasF  = isValid(F_raw);
    bool hasR1 = isValid(R1_raw);
    bool hasR2 = isValid(R2_raw);

    if (!hasF && !hasR1 && !hasR2) { return "S"; } // 启动保护: 全无数据则停车

    float F  = (hasF && F_raw < 5000) ? F_raw : 5000.0f;
    float R1 = hasR1 ? R1_raw : 5000.0f;
    float R2 = hasR2 ? R2_raw : 5000.0f; 
    bool isHeadOut = (R1 > RIGHT_LOST_WALL); 


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
    if (F < FRONT_BACKUP_TH) {
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
    if (F <= FRONT_TURN_TH && R1<= 130.0F || isFrontTurnSequenceActive) { ////要改这里！！！！
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
