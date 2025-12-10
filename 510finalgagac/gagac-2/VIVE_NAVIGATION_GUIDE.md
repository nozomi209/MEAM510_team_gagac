# VIVE定位导航完整方法指南

## 📋 概述

本文档详细说明从**获取Vive位置** → **计算目标路径** → **执行控制命令** → **移动到目标位置**的完整流程和方法。

---

## 🔄 完整工作流程

### 流程图

```
┌─────────────────────────────────────────────────────────────┐
│  1. Vive位置获取阶段                                        │
│     - 两个tracker接收基站信号                               │
│     - 计算中心位置 (X, Y)                                   │
│     - 计算朝向角度 (Angle)                                  │
└─────────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────┐
│  2. 数据传输阶段                                            │
│     Servant板 → Owner板 (UART)                              │
│     格式: "VIVE:x.xx,y.yy,a.aa\n"                          │
└─────────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────┐
│  3. 导航计算阶段 (Owner板)                                  │
│     - 解析Vive数据                                          │
│     - 计算到目标点的距离和角度                              │
│     - 判断是否需要转向或前进                                │
└─────────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────┐
│  4. 控制命令生成阶段                                        │
│     生成命令: "F<speed>", "L<rate>", "R<rate>", "S"        │
└─────────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────┐
│  5. 命令执行阶段 (Servant板)                                │
│     - 接收控制命令                                          │
│     - 控制电机运动                                          │
│     - 返回新的Vive位置                                      │
└─────────────────────────────────────────────────────────────┘
                        ↓
                    (循环直到到达目标)
```

---

## 📍 阶段1：Vive位置获取（Servant板）

### 1.1 硬件配置

**两个Tracker安装位置：**
- **Tracker 1** (GPIO 15)：车后左边
- **Tracker 2** (GPIO 16)：车后右边

### 1.2 位置计算代码

在 `gagac-2.ino` 的 `loop()` 函数中：

```cpp
if (isViveActive) {
    // 处理两个tracker的数据
    processViveData(viveFront, viveXFront, viveYFront);  // Tracker 1 (左边)
    processViveData(viveBack, viveXBack, viveYBack);      // Tracker 2 (右边)
    
    // 计算中心位置（两个tracker连线的中点）
    viveX = (float(viveXFront) + float(viveXBack)) / 2.0;
    viveY = (float(viveYFront) + float(viveYBack)) / 2.0;
    
    // 计算朝向角度
    float deltaX = float(viveXBack) - float(viveXFront);
    float deltaY = float(viveYBack) - float(viveYFront);
    viveAngle = (180.0 / PI) * fastAtan2(deltaY, deltaX) + VIVE_ANGLE_OFFSET;
    
    // 角度归一化到 -180° 到 180°
    if (viveAngle > 180.0) {
        viveAngle -= 360.0;
    } else if (viveAngle < -180.0) {
        viveAngle += 360.0;
    }
}
```

### 1.3 数据输出

**每100ms通过UART发送到Owner板：**

```cpp
// 在 gagac-2.ino 中
if (millis() - lastViveUartTime > 100 && isViveActive) {
    lastViveUartTime = millis();
    // 格式: "VIVE:x.xx,y.yy,a.aa\n"
    OwnerSerial.printf("VIVE:%.2f,%.2f,%.2f\n", viveX, viveY, viveAngle);
}
```

**数据格式说明：**
- `x.xx`：X坐标（单位：Vive坐标单位，通常1000-8000）
- `y.yy`：Y坐标（单位：Vive坐标单位，通常1000-8000）
- `a.aa`：朝向角度（单位：度，范围-180°到180°）

---

## 📡 阶段2：数据传输（UART通信）

### 2.1 通信配置

**硬件连接：**
- Servant板：TX=7, RX=6
- Owner板：RX=6, TX=7
- 波特率：115200

### 2.2 Owner板接收代码

在 `owner-4.ino` 中接收Vive数据：

```cpp
// 全局变量存储Vive数据
float viveX = 0.0, viveY = 0.0, viveAngle = 0.0;
bool hasViveData = false;

void loop() {
    // 从Servant板接收数据
    if (ServantSerial.available()) {
        String data = ServantSerial.readStringUntil('\n');
        data.trim();
        
        // 解析Vive数据
        if (data.startsWith("VIVE:")) {
            // 格式: "VIVE:x.xx,y.yy,a.aa"
            int comma1 = data.indexOf(',');
            int comma2 = data.indexOf(',', comma1 + 1);
            
            if (comma1 > 0 && comma2 > 0) {
                viveX = data.substring(5, comma1).toFloat();
                viveY = data.substring(comma1 + 1, comma2).toFloat();
                viveAngle = data.substring(comma2 + 1).toFloat();
                hasViveData = true;
            }
        }
    }
}
```

---

## 🧮 阶段3：导航计算（Owner板）

### 3.1 导航算法核心函数

在 `behavior-vive.ino` 中的 `gotoPoint()` 函数：

```cpp
bool gotoPoint(float xDesired, float yDesired, 
               float viveX, float viveY, float viveAngle, 
               String& cmd) {
    // 步骤1：计算到目标点的距离和方向
    float deltaY = yDesired - viveY;
    float deltaX = xDesired - viveX;
    float distance = sqrt(deltaX * deltaX + deltaY * deltaY);
    
    // 步骤2：计算目标方向角度
    float desiredAngle = (180.0 / PI) * fastAtan2(deltaY, deltaX) + 90.0;
    
    // 角度归一化
    if (desiredAngle > 180.0) desiredAngle -= 360.0;
    if (desiredAngle < -180.0) desiredAngle += 360.0;
    
    // 步骤3：计算当前朝向与目标方向的夹角误差
    float currentAngle = viveAngle;
    if (currentAngle > 180.0) currentAngle -= 360.0;
    if (currentAngle < -180.0) currentAngle += 360.0;
    
    float angleError = desiredAngle - currentAngle;
    if (angleError > 180.0) angleError -= 360.0;
    if (angleError < -180.0) angleError += 360.0;
    
    // 步骤4：判断是否到达目标
    if (distance <= DISTANCE_TOLERANCE) {  // 默认5mm
        cmd = "S";  // 停止
        return true;
    }
    
    // 步骤5：决策：先转向还是直接前进
    if (abs(angleError) > ANGLE_TOLERANCE) {  // 默认20°
        // 需要先转向
        if (angleError > 0) {
            cmd = "R" + String(60);  // 右转（顺时针）
        } else {
            cmd = "L" + String(60);  // 左转（逆时针）
        }
        return false;
    }
    
    // 步骤6：朝向正确，前进
    float speed = 50.0f;  // 基础速度
    if (distance < 50.0f) {
        speed = 30.0f;  // 接近时减速
    } else if (distance < 100.0f) {
        speed = 40.0f;
    }
    cmd = "F" + String((int)speed);
    return false;
}
```

### 3.2 导航参数

**可调整参数（在 `behavior-vive.ino` 中）：**

```cpp
const float ANGLE_TOLERANCE = 20.0f;      // 角度误差阈值（度）
const float DISTANCE_TOLERANCE = 5.0f;     // 到达距离阈值（mm）
```

**速度参数：**
- 基础速度：50%
- 距离 < 100mm：40%
- 距离 < 50mm：30%

---

## 🎮 阶段4：控制命令生成

### 4.1 命令格式

**控制命令格式（Owner板 → Servant板）：**

| 命令 | 格式 | 说明 | 示例 |
|------|------|------|------|
| 前进 | `F<speed>` | speed: 0-100（速度百分比） | `F50` = 50%速度前进 |
| 后退 | `B<speed>` | speed: 0-100 | `B30` = 30%速度后退 |
| 左转 | `L<rate>` | rate: 0-100（转向强度） | `L60` = 60%强度左转 |
| 右转 | `R<rate>` | rate: 0-100 | `R60` = 60%强度右转 |
| 停止 | `S` | 立即停止 | `S` |

### 4.2 命令发送

在 `owner-4.ino` 中：

```cpp
void sendToServant(const String &cmd) {
    ServantSerial.println(cmd);  // 通过UART发送
    Serial.println(cmd);         // 串口输出（调试用）
}

// 使用示例
String cmd;
bool reached = gotoPoint(targetX, targetY, viveX, viveY, viveAngle, cmd);
sendToServant(cmd);
```

---

## ⚙️ 阶段5：命令执行（Servant板）

### 5.1 命令接收

在 `gagac-2.ino` 的 `loop()` 中：

```cpp
// 从Owner板接收命令
if (OwnerSerial.available()) {
    String cmd = OwnerSerial.readStringUntil('\n');
    cmd.trim();
    handleCommand(cmd);
}
```

### 5.2 命令处理

在 `gagac-2.ino` 的 `handleCommand()` 函数中：

```cpp
void handleCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd.startsWith("F")) {
        // 前进：F<speed>
        float speed = cmd.substring(1).toFloat();
        setCarSpeed(speed);
    }
    else if (cmd.startsWith("B")) {
        // 后退：B<speed>
        float speed = cmd.substring(1).toFloat();
        setCarSpeed(-speed);
    }
    else if (cmd.startsWith("L")) {
        // 左转：L<rate>
        float turnRate = cmd.substring(1).toFloat();
        setCarTurn(50, -turnRate);  // 负号表示左转
    }
    else if (cmd.startsWith("R")) {
        // 右转：R<rate>
        float turnRate = cmd.substring(1).toFloat();
        setCarTurn(50, turnRate);   // 正号表示右转
    }
    else if (cmd == "S") {
        // 停止
        stopMotors();
    }
}
```

### 5.3 电机控制

```cpp
void setCarSpeed(float speedPercent) {
    float maxRPM = MOTOR_MAX_RPM_RATED * 0.9;
    float targetRPM = maxRPM * speedPercent / 100.0;
    
    targetSpeedL = 0.996 * targetRPM;  // 左轮稍微慢一点（校准）
    targetSpeedR = targetRPM;
}

void setCarTurn(float speedPercent, float turnRate) {
    float maxRPM = MOTOR_MAX_RPM_RATED * 0.9;
    float baseSpeed = maxRPM * speedPercent / 100.0;
    
    float turnFactor = turnRate / 100.0;
    targetSpeedL = baseSpeed * (1.0 + turnFactor);  // 左轮加速
    targetSpeedR = baseSpeed * (1.0 - turnFactor);  // 右轮减速
}
```

---

## 🔁 完整导航循环

### 导航循环示例（Owner板）

```cpp
void loop() {
    // 1. 接收Vive数据
    if (ServantSerial.available()) {
        String data = ServantSerial.readStringUntil('\n');
        if (data.startsWith("VIVE:")) {
            // 解析Vive数据
            parseViveData(data, viveX, viveY, viveAngle);
            hasViveData = true;
        }
    }
    
    // 2. 如果处于导航模式且有Vive数据
    if (isNavigating && hasViveData) {
        String cmd;
        
        // 3. 计算导航命令
        bool reached = gotoPoint(targetX, targetY, 
                                viveX, viveY, viveAngle, 
                                cmd);
        
        // 4. 发送控制命令
        sendToServant(cmd);
        
        // 5. 如果到达目标，停止导航
        if (reached) {
            isNavigating = false;
            Serial.println("Target reached!");
        }
    }
    
    delay(50);  // 控制循环频率（20Hz）
}
```

---

## 📊 导航状态机

### 状态转换图

```
┌─────────────┐
│   IDLE      │  (等待状态)
│  (停止)     │
└──────┬──────┘
       │ 收到导航命令
       ↓
┌─────────────┐
│  TURNING    │  (转向状态)
│  (调整角度) │  ──→ 角度误差 > 20°
└──────┬──────┘
       │ 角度误差 < 20°
       ↓
┌─────────────┐
│  MOVING     │  (前进状态)
│  (向目标)   │  ──→ 距离 > 5mm
└──────┬──────┘
       │ 距离 < 5mm
       ↓
┌─────────────┐
│  REACHED    │  (到达状态)
│  (停止)     │  ──→ 返回IDLE
└─────────────┘
```

### 状态判断逻辑

```cpp
// 伪代码
if (distance <= DISTANCE_TOLERANCE) {
    // 状态：REACHED
    cmd = "S";
    return true;
}
else if (abs(angleError) > ANGLE_TOLERANCE) {
    // 状态：TURNING
    if (angleError > 0) cmd = "R60";
    else cmd = "L60";
    return false;
}
else {
    // 状态：MOVING
    cmd = "F" + String(speed);
    return false;
}
```

---

## 🎯 实际使用示例

### 示例1：导航到单个目标点

```cpp
// 在 owner-4.ino 中

// 1. 定义目标点
const float TARGET_X = 3000.0;
const float TARGET_Y = 4000.0;

// 2. 启动导航
void startNavigation() {
    isNavigating = true;
    targetX = TARGET_X;
    targetY = TARGET_Y;
}

// 3. 在loop()中执行导航
void loop() {
    // 接收Vive数据...
    
    if (isNavigating && hasViveData) {
        String cmd;
        bool reached = gotoPoint(targetX, targetY, 
                                viveX, viveY, viveAngle, 
                                cmd);
        sendToServant(cmd);
        
        if (reached) {
            isNavigating = false;
            Serial.println("Target reached!");
        }
    }
}
```

### 示例2：序列导航（3个位置）

```cpp
// 定义3个目标点
const float TARGETS[3][2] = {
    {2000.0, 2000.0},  // 位置1
    {6000.0, 2000.0},  // 位置2
    {4000.0, 4000.0}   // 位置3
};

int currentTarget = 0;
bool isNavigating = false;

void loop() {
    // 接收Vive数据...
    
    if (isNavigating && hasViveData) {
        String cmd;
        bool reached = gotoPoint(
            TARGETS[currentTarget][0], 
            TARGETS[currentTarget][1],
            viveX, viveY, viveAngle, 
            cmd
        );
        sendToServant(cmd);
        
        if (reached) {
            currentTarget++;
            if (currentTarget >= 3) {
                // 完成所有目标
                isNavigating = false;
                Serial.println("All targets reached!");
            } else {
                Serial.printf("Target %d reached, moving to target %d\n", 
                             currentTarget, currentTarget + 1);
            }
        }
    }
}
```

---

## ⚙️ 参数调优指南

### 导航精度调整

**如果机器人无法精确到达目标：**

```cpp
// 减小距离阈值（更精确，但可能无法到达）
const float DISTANCE_TOLERANCE = 3.0f;  // 从5.0改为3.0

// 或增大距离阈值（更容易到达，但精度降低）
const float DISTANCE_TOLERANCE = 10.0f;  // 从5.0改为10.0
```

### 转向灵敏度调整

**如果转向不够灵敏或过于敏感：**

```cpp
// 减小角度阈值（更频繁转向，更精确）
const float ANGLE_TOLERANCE = 10.0f;  // 从20.0改为10.0

// 增大角度阈值（减少转向，更平滑）
const float ANGLE_TOLERANCE = 30.0f;  // 从20.0改为30.0
```

### 速度调整

**如果速度太快或太慢：**

```cpp
// 在 gotoPoint() 函数中
float speed = 50.0f;  // 基础速度（0-100）
if (distance < 50.0f) {
    speed = 30.0f;  // 接近时速度
} else if (distance < 100.0f) {
    speed = 40.0f;  // 中等距离速度
}
```

### 转向强度调整

**如果转向不够或过度：**

```cpp
// 在 gotoPoint() 函数中
if (angleError > 0) {
    cmd = "R" + String(60);  // 右转强度（0-100）
} else {
    cmd = "L" + String(60);  // 左转强度（0-100）
}
```

---

## 🐛 常见问题与解决

### 问题1：机器人无法到达目标

**可能原因：**
- 距离阈值太小
- 速度太慢
- 角度阈值太大

**解决方法：**
```cpp
// 增大距离阈值
const float DISTANCE_TOLERANCE = 10.0f;

// 提高速度
float speed = 60.0f;  // 从50.0提高

// 减小角度阈值
const float ANGLE_TOLERANCE = 15.0f;
```

### 问题2：机器人来回摆动

**可能原因：**
- 角度阈值太小
- 控制频率太高

**解决方法：**
```cpp
// 增大角度阈值
const float ANGLE_TOLERANCE = 25.0f;

// 降低控制频率
delay(100);  // 从50ms改为100ms
```

### 问题3：机器人转向过度

**可能原因：**
- 转向强度太大
- 角度误差计算有误

**解决方法：**
```cpp
// 减小转向强度
cmd = "R" + String(40);  // 从60改为40
cmd = "L" + String(40);
```

---

## 📝 总结

### 关键步骤回顾

1. **位置获取**：两个tracker → 中心位置 + 朝向角度
2. **数据传输**：Servant板 → Owner板（UART，100ms一次）
3. **导航计算**：计算距离和角度误差 → 生成控制命令
4. **命令执行**：Owner板 → Servant板 → 电机控制
5. **循环执行**：直到到达目标

### 关键参数

- **DISTANCE_TOLERANCE**：到达判断距离（默认5mm）
- **ANGLE_TOLERANCE**：转向判断角度（默认20°）
- **速度**：基础50%，接近时30-40%
- **转向强度**：60%

### 调试建议

1. 使用串口监视器查看Vive数据和导航状态
2. 逐步调整参数，观察效果
3. 记录到达时间和精度，优化参数
4. 测试不同距离和角度的目标点

---

**提示**：根据实际测试结果调整参数，每个机器人的特性可能不同。

