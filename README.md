# MEAM510 自主导航机器人系统 - 使用文档

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)

基于Vive追踪器和ESP32双板架构的自主导航机器人系统，实现精确位置追踪、自动导航、壁障跟随等功能。

---

## 📋 目录

- [快速开始](#快速开始)
- [系统概述](#系统概述)
- [硬件连接](#硬件连接)
- [软件安装](#软件安装)
- [配置说明](#配置说明)
- [使用指南](#使用指南)
- [功能说明](#功能说明)
- [故障排除](#故障排除)
- [项目结构](#项目结构)

---

## 🚀 快速开始

### 5分钟快速上手

1. **硬件连接**
   - 按照[硬件连接](#硬件连接)章节连接所有硬件
   - 确保UART线交叉连接（重要！）

2. **上传代码**
   - 先上传 `gagac-2/gagac-2.ino` 到Servant板
   - 再上传 `owner-4/owner-4.ino` 到Owner板

3. **启动系统**
   - 上电两块ESP32板
   - 等待5秒让Vive系统同步
   - 连接到WiFi：`ESP32-MobileBase`（密码：`12345678`）
   - 访问Web界面：http://192.168.4.1

4. **测试功能**
   - 打开串口监视器（115200波特率）
   - 发送 `W` 测试壁障跟随
   - 发送 `V` 测试3位置序列导航（比赛任务）

---

## 🎯 系统概述

### 系统架构

本项目采用**双ESP32板架构**：

- **Servant板** (`gagac-2/`)
  - 电机PWM控制和编码器反馈
  - Vive追踪器数据采集和计算
  - Web服务器（WiFi控制界面）
  - 接收Owner板控制命令并执行

- **Owner板** (`owner-4/`)
  - ToF传感器数据采集
  - 导航决策和路径规划
  - 行为模式切换
  - 向Servant板发送控制命令

### 数据流

```
Vive基站 → Vive追踪器 → Servant板
                              ↓ (UART, 100ms周期)
                          Owner板 ← ToF传感器
                              ↓
                        控制命令 → Servant板 → 电机
```

### 核心功能

✅ **已实现功能**
- Vive位置追踪（毫米级精度）
- 自动导航到指定坐标点
- 3位置序列导航（比赛任务，9分）
- 壁障跟随（右墙跟随）
- Web控制界面
- 障碍物避让

---

## 🔌 硬件连接

### Servant板连接

#### 电机控制（6个引脚）
```
左电机：
  MOTOR_L_PWM  → GPIO 9
  MOTOR_L_IN1  → GPIO 10
  MOTOR_L_IN2  → GPIO 11

右电机：
  MOTOR_R_PWM  → GPIO 14
  MOTOR_R_IN1  → GPIO 12
  MOTOR_R_IN2  → GPIO 13
```

#### 编码器（4个引脚）
```
左编码器：
  ENCODER_L_A  → GPIO 4
  ENCODER_L_B  → GPIO 5

右编码器：
  ENCODER_R_A  → GPIO 2
  ENCODER_R_B  → GPIO 1
```

#### Vive追踪器（2个引脚）
```
前追踪器 → GPIO 15
后追踪器 → GPIO 16
```

#### UART通信（与Owner板，2个引脚）
```
Servant GPIO17 (TX) → Owner GPIO18 (RX)
Servant GPIO18 (RX) → Owner GPIO17 (TX)
```
⚠️ **重要**：必须交叉连接！

### Owner板连接

#### ToF传感器（I2C总线）
```
前传感器（粉色标记）  → I2C (SDA/SCL)
右前传感器（绿色标记） → I2C (SDA/SCL)
右后传感器（蓝色标记） → I2C (SDA/SCL)
```
⚠️ **安装建议**：前面的ToF传感器尽量装得靠右边一点，以获得更好的壁障跟随效果。

#### UART通信（与Servant板，2个引脚）
```
Owner GPIO17 (TX) → Servant GPIO18 (RX)
Owner GPIO18 (RX) → Servant GPIO17 (TX)
```
⚠️ **重要**：必须交叉连接！

### 电源要求

- **Servant板**：5V/3A（电机驱动需要较大电流）
- **Owner板**：5V/1A（传感器和逻辑控制）
- **Vive追踪器**：3.3V（通过ESP32供电）

### ⚠️ 硬件注意事项

1. **主板排针连接**
   - 主板两层排针之间可能比较松，使用时注意检查连接是否牢固
   - 建议定期检查排针连接，避免接触不良导致的问题

2. **ToF传感器安装位置**
   - 前面的ToF传感器尽量装得靠右边一点，以获得更好的壁障跟随效果

3. **代码上传端口**
   - 主板上传代码时使用**右边的USB口**

---

## 💻 软件安装

### 前置要求

- **Arduino IDE** 1.8+ 或 2.0+
- **ESP32开发板支持包**

### 安装步骤

1. **安装ESP32开发环境**
   - 打开Arduino IDE
   - 文件 → 首选项 → 附加开发板管理器网址：
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - 工具 → 开发板 → 开发板管理器 → 搜索"ESP32"并安装

2. **配置开发板**
   - Servant板：工具 → 开发板 → ESP32 Arduino → "ESP32 Dev Module"
   - Owner板：工具 → 开发板 → ESP32 Arduino → "ESP32 Dev Module"
   - 串口波特率：115200

3. **上传代码**
   - 先上传 `510finalgagac/gagac-2/gagac-2.ino` 到Servant板
   - 再上传 `510finalgagac/owner-4/owner-4.ino` 到Owner板
   - ⚠️ **重要**：主板上传代码时使用**右边的USB口**

### 所需库

所有库均为ESP32内置，无需额外安装：
- `WebServer`（ESP32内置）
- `WiFi`（ESP32内置）
- `HardwareSerial`（ESP32内置）

---

## ⚙️ 配置说明

### WiFi配置

在 `gagac-2/gagac-2.ino` 中修改：

```cpp
// AP模式（默认，机器人作为WiFi热点）
const char* AP_SSID = "ESP32-MobileBase";
const char* AP_PASSWORD = "12345678";
```

### 目标点坐标配置

在 `owner-4/owner-4.ino` 中修改目标点坐标（单位：毫米）：

```cpp
// 单点导航目标
const float TARGET_RED_X = 2000.0;   // 红色目标X坐标
const float TARGET_RED_Y = 2000.0;   // 红色目标Y坐标
const float TARGET_BLUE_X = 6000.0;  // 蓝色目标X坐标
const float TARGET_BLUE_Y = 2000.0;  // 蓝色目标Y坐标

// 3位置序列导航目标（V模式使用）
// 位置1：使用 TARGET_RED_X/Y
// 位置2：使用 TARGET_BLUE_X/Y
// 位置3：使用 TARGET_LOC3_X/Y
const float TARGET_LOC3_X = 4000.0;  // 位置3 X坐标
const float TARGET_LOC3_Y = 4000.0;  // 位置3 Y坐标
```

⚠️ **重要**：根据实际的Vive坐标系和目标位置设置这些值。

### 导航参数调整

在 `owner-4/behavior-vive.ino` 中调整：

```cpp
const float ANGLE_TOLERANCE = 20.0f;      // 角度容差（度）
                                         // 角度误差>此值时先转向
const float DISTANCE_TOLERANCE = 5.0f;    // 到达距离阈值（毫米）
                                         // 距离<此值时认为到达目标
```

### 超时设置

在 `owner-4/owner-4.ino` 中配置：

```cpp
const unsigned long AUTO_NAV_TIMEOUT = 30000;  // 30秒超时（毫秒）
                                               // 每个位置30秒限制
```

---

## 📖 使用指南

### 模式控制

通过串口（115200波特率）发送命令：

| 命令 | 功能 | 说明 |
|------|------|------|
| `W` | 壁障跟随模式 | 使用ToF传感器跟随右墙 |
| `R` | 导航到红色目标 | 自动导航到配置的红色目标点 |
| `B` | 导航到蓝色目标 | 自动导航到配置的蓝色目标点 |
| `V` | **3位置序列导航** | 自动依次访问3个位置（比赛任务） |
| `M` | 手动模式 | 通过Web界面手动控制 |

### Web界面控制

1. **连接WiFi**
   - 连接到机器人WiFi：`ESP32-MobileBase`
   - 密码：`12345678`

2. **访问Web界面**
   - 打开浏览器访问：http://192.168.4.1

3. **功能说明**
   - **实时数据显示**：查看Vive坐标（X, Y, Angle）
   - **手动控制**：方向键或按钮控制移动
   - **速度调整**：速度/转向滑块
   - **Vive开关**：启用/禁用Vive追踪
   - **自动模式**：启动/停止自动模式

### 3位置序列导航（比赛任务）

**任务要求**：自动访问3个Vive位置，每个位置30秒，共9分

**使用步骤**：
1. 配置3个位置的Vive坐标（在 `owner-4.ino` 中）
2. 确保Vive系统正常工作
3. 通过串口发送 `V` 命令
4. 系统自动依次访问3个位置
5. 每个位置独立30秒计时
6. 完成后自动切换回壁障跟随模式

**状态监控**：
```
Mode: 3LOCS | Loc: 1/3 | Time: 5000/30000 ms | Auto: ON | ...
Location 1 reached! (2000.0, 2000.0) - Time: 8500 ms
Starting navigation to Location 2 ...
```

### 壁障跟随模式

**功能**：自动跟随右墙移动

**特点**：
- 自动保持与右墙约9cm距离
- 前方障碍物检测和转向
- 适用于封闭环境探索

**使用方法**：
1. 发送 `W` 命令启动
2. 机器人开始跟随右墙移动
3. 可通过其他命令切换模式

### ⚠️ 重要使用注意事项

#### Auto Mode相关问题

1. **Auto Mode启动无响应**
   - **症状**：刚打开网页时，如果点击Auto Mode按钮没有反应
   - **解决方案**：断开电源线重新连接，然后重新连接WiFi和访问网页

2. **Auto Mode重启后异常转向**
   - **症状**：测完一次Auto Mode断开后，再次点击Auto Mode开始时，机器人一瞬间可能会转一下弯
   - **原因**：可能有些内存（memory）没清理干净
   - **解决方案**：
     1. 重新连接WiFi后，先**刷新网页**（F5或Ctrl+R）
     2. 在网页上点击几次**前进按钮（F）**，让系统清空过去的记忆
     3. 然后再启动Auto Mode
   - **注意**：虽然代码中已经写了clear memory，但可能不够彻底，建议使用上述方法

---

## 🎮 功能说明

### 1. Vive位置追踪

**功能**：实时获取机器人精确位置和朝向角度

**实现**：
- Servant板读取前后两个Vive追踪器数据
- 计算机器人中心位置和朝向角度
- 每100ms通过UART发送到Owner板

**数据格式**：
```
VIVE:1234.56,2345.67,45.0
       ↑       ↑      ↑
      X坐标   Y坐标  角度(度)
```

### 2. 自动导航系统

**功能**：基于Vive坐标的精确导航

**算法**：
1. 计算到目标点的距离和方向角
2. 检查是否到达目标（距离<5mm）
3. 如果角度误差>20°，先转向目标方向
4. 如果角度误差≤20°，向目标前进
5. 结合ToF传感器进行障碍物避让

**支持模式**：
- 单点导航（R/B模式）
- 3位置序列导航（V模式）

### 3. 壁障跟随

**功能**：基于ToF传感器的右墙跟随

**算法**：
- 保持与右墙约9cm距离
- 前方障碍物<22cm时左转避让
- 前方障碍物<3cm时紧急倒车
- 检测到墙丢失时强力右转找墙

### 4. 障碍物避让

**功能**：ToF传感器实时检测和避让

**阈值**：
- 紧急停车：前方<15cm 或 右前<8cm
- 避让转向：前方<30cm
- 正常前进：前方>30cm

---

## 🐛 故障排除

### 问题1：Vive数据无效

**症状**：
- 串口显示Vive坐标为0或不变
- 自动导航模式自动降级为壁障跟随

**解决方案**：
1. ✅ 检查UART连接（必须交叉连接）
   - Owner板 GPIO18 ← Servant板 GPIO17
   - Owner板 GPIO17 → Servant板 GPIO18
2. ✅ 检查Vive基站是否正常工作
3. ✅ 检查追踪器是否在基站覆盖范围内
4. ✅ 通过Web界面确认Vive系统已启用

### 问题2：无法到达目标点

**症状**：
- 机器人无法到达目标点
- 30秒超时后切换到下一个位置

**解决方案**：
1. **检查目标点坐标**
   - 确认坐标是否在Vive坐标系内
   - 确认坐标值是否正确（单位：mm）

2. **调整导航参数**
   ```cpp
   // 在 behavior-vive.ino 中
   const float ANGLE_TOLERANCE = 20.0f;      // 可以增大（更早转向）
   const float DISTANCE_TOLERANCE = 5.0f;    // 可以增大（更容易到达）
   ```

3. **检查障碍物**
   - 使用ToF传感器数据检查路径
   - 调整障碍物避让参数

### 问题3：Web界面无法访问

**症状**：
- 无法连接到Web界面

**解决方案**：
1. 确认连接到机器人WiFi：`ESP32-MobileBase`
2. 检查IP地址：http://192.168.4.1
3. 确认Servant板WiFi正常启动（查看串口输出）
4. 重启Servant板

### 问题4：导航不稳定

**症状**：
- 机器人来回摆动
- 导航路径不顺畅

**解决方案**：
1. **检查Vive数据更新频率**
   - 应为10Hz（每100ms更新一次）
   - 查看串口输出确认更新频率

2. **调整导航参数**
   - 增大 `ANGLE_TOLERANCE` 可以减少转向频率
   - 调整速度曲线使移动更平滑

3. **检查ToF传感器**
   - 确认传感器工作正常
   - 清洁传感器表面

---

## 📁 项目结构

```
510 final/
├── README.md                           # 本文档（使用说明）
├── 任务完成情况分析.md                  # 比赛任务分析
│
├── 510finalgagac/                      # 主项目代码
│   ├── gagac-2/                        # Servant板代码
│   │   ├── gagac-2.ino                # 主程序（电机控制、Vive、Web）
│   │   ├── gagac-web.h                # Web界面HTML代码
│   │   ├── vive_tracker.h/.cpp        # Vive追踪器驱动
│   │   └── vive_utils.h/.cpp          # Vive工具函数
│   │
│   └── owner-4/                        # Owner板代码
│       ├── owner-4.ino                # 主程序（导航决策、模式控制）
│       ├── behavior-vive.ino          # Vive导航行为逻辑
│       ├── behavior-wall.ino          # 壁障跟随行为逻辑
│       ├── tof-new-sensor.ino         # ToF传感器驱动
│       └── README_VIVE_NAV.md         # Vive导航详细文档
│
└── meam510_final_project/              # 参考代码/旧版本
    ├── motor/                          # 电机控制参考代码
    └── sensor/                         # 传感器参考代码
```

### 关键文件说明

#### Servant板 (`gagac-2/`)
- **gagac-2.ino**：主控制循环，集成电机控制、Vive追踪、Web服务器
- **gagac-web.h**：Web界面HTML/CSS/JavaScript代码
- **vive_tracker.***：Vive追踪器驱动库
- **vive_utils.***：坐标计算工具函数

#### Owner板 (`owner-4/`)
- **owner-4.ino**：主程序，模式切换和命令处理
- **behavior-vive.ino**：Vive导航算法实现
- **behavior-wall.ino**：壁障跟随算法实现
- **tof-new-sensor.ino**：ToF传感器驱动

---

## 📊 比赛任务对应

### ✅ 已完成任务

| 任务 | 分数 | 实现状态 | 使用方法 |
|------|------|---------|---------|
| **Autonomous cover 3 vive locations** | **9 pts** | ✅ **已完成** | 发送 `V` 命令 |

**要求**：
- 自动访问3个Vive位置
- 每个位置30秒内完成（3分/位置）
- 共9分

**实现**：
- 模式：`MODE_AUTO_NAV_3LOCS`
- 每个位置独立30秒超时
- 自动顺序访问：位置1 → 位置2 → 位置3
- 实时状态显示

### ⚠️ 部分支持任务

| 任务 | 分数 | 实现状态 | 说明 |
|------|------|---------|------|
| Manual attack to tower/nexus | 3 pts | ⚠️ 部分支持 | 有手动控制，需添加攻击命令 |
| Drive up the ramp | 5 pts | ⚠️ 可能支持 | 手动模式可完成 |
| Autonomous full circuit wall follow | 10 pts | ⚠️ 部分支持 | 壁障跟随功能完整，缺少一圈检测 |

**详细分析**：参见 [任务完成情况分析.md](任务完成情况分析.md)

---

## 🔧 调试技巧

### 串口输出

系统会输出详细的状态信息：

```
===== OWNER BOARD (Right Wall Logic + Vive Nav) =====
UART to servant ready. Waiting for Start...
Mode: 3LOCS | Loc: 1/3 | Time: 5000/30000 ms | Auto: ON | Vive: X=2500.0, Y=1800.0, A=45.0° | ToF: F=500, R1=200, R2=180
```

### 状态信息说明

- **Mode**: 
  - `0` = 壁障跟随
  - `1` = 红色目标
  - `2` = 蓝色目标
  - `3` = 手动模式
  - `4` = 3位置序列（显示为3LOCS）
- **Auto**: `ON`/`OFF` - 自动模式开关状态
- **Vive**: 当前Vive坐标和角度
- **ToF**: 三个ToF传感器的距离值（单位：mm）
- **Loc**: 当前访问的位置（仅3位置序列模式）
- **Time**: 当前位置已用时间/超时时间（仅3位置序列模式）

### 测试流程

1. **测试Vive数据**
   - 查看Servant板串口：显示原始Vive数据
   - 查看Web界面：实时坐标显示
   - 查看Owner板串口：接收到的Vive数据

2. **测试导航**
   - 先测试单点导航（`R` 或 `B`）
   - 再测试3位置序列（`V`）
   - 观察串口输出的状态信息

3. **参数调整**
   - 从保守值开始（慢速、大容差）
   - 逐步优化速度和精度
   - 记录不同参数的效果

---

## 📚 相关文档

### 主要文档

1. **[README.md](README.md)**（本文档）
   - 项目总览和快速开始
   - 系统架构和项目结构
   - 硬件连接和配置说明

2. **[owner-4/README_VIVE_NAV.md](510finalgagac/owner-4/README_VIVE_NAV.md)**
   - Vive导航功能详细说明
   - 所有模式的使用方法
   - 工作原理和算法说明
   - 故障排除和进阶配置

3. **[任务完成情况分析.md](任务完成情况分析.md)**
   - 比赛任务完成情况统计
   - 每个任务的实现状态
   - 待开发功能分析
   - 优先级建议

---

## 👥 开发信息

### 课程信息

- **课程名称**：Design of Mechatronic Systems
- **学期**：2025 Spring
- **课程编号**：MEAM510

### 项目特点

- 双板架构设计（Servant + Owner）
- 毫米级精度位置追踪
- 多模式自主导航
- 实时Web控制界面
- 完整的障碍物避让系统

---

## 📝 许可证

本项目为课程作业项目，仅供学习和参考使用。

---

## 🔗 相关链接

- [ESP32官方文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Arduino ESP32支持](https://github.com/espressif/arduino-esp32)
- [Vive追踪系统](https://www.vive.com/us/support/vive/category_howto/tracking-system.html)

---

## 📞 技术支持

如有问题，请：

1. 查看相关文档：
   - [README_VIVE_NAV.md](510finalgagac/owner-4/README_VIVE_NAV.md) - 详细功能说明
   - [任务完成情况分析.md](任务完成情况分析.md) - 任务分析

2. 检查串口输出和错误信息

3. 确认硬件连接和配置

---

**最后更新**：2025年  
**版本**：2.0  
**状态**：活跃开发中

---

**祝使用愉快！** 🚀
