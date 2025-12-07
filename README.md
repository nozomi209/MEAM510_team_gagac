# MEAM510 最终项目 - 自主导航机器人系统

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Status](https://img.shields.io/badge/status-active-success.svg)]()

基于Vive追踪器和ESP32的双板架构自主导航机器人系统，实现精确位置追踪、自动导航、壁障跟随等功能。

---

## 📋 目录

- [项目概述](#项目概述)
- [主要功能](#主要功能)
- [系统架构](#系统架构)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
- [硬件连接](#硬件连接)
- [软件配置](#软件配置)
- [使用指南](#使用指南)
- [比赛任务对应](#比赛任务对应)
- [文档说明](#文档说明)
- [开发团队](#开发团队)

---

## 🎯 项目概述

本项目是一个完整的自主导航机器人系统，采用双ESP32板架构：
- **Servant板**：负责电机控制、Vive追踪数据采集、Web界面控制
- **Owner板**：负责传感器数据采集、导航决策、行为控制

系统通过Vive追踪器实现毫米级精度的位置定位，结合ToF传感器进行障碍物避让，支持多种自主导航模式。

### 核心技术栈

- **硬件平台**：ESP32微控制器
- **位置追踪**：Vive Lighthouse追踪系统
- **传感器**：ToF距离传感器（VL53L0X）
- **通信协议**：UART串口通信、WiFi Web控制
- **开发环境**：Arduino IDE

---

## ✨ 主要功能

### ✅ 已实现功能

1. **Vive位置追踪** (9分)
   - 实时获取机器人精确位置和朝向角度
   - 双追踪器配置（前后）提高精度
   - 自动访问3个Vive位置的序列导航

2. **自动导航系统**
   - 基于Vive坐标的精确导航
   - 单点导航（红色/蓝色目标点）
   - 多位置序列导航（3位置自动访问）

3. **壁障跟随**
   - 基于ToF传感器的右墙跟随
   - 自动避障和路径调整
   - 适用于封闭环境自主移动

4. **Web控制界面**
   - 实时显示Vive坐标数据
   - 手动控制模式
   - 自动模式开关
   - 直观的图形界面

5. **障碍物避让**
   - ToF传感器实时检测
   - 紧急停车机制
   - 智能转向避让

### 🚧 计划功能

- 完整一圈壁障跟随检测
- 目标识别和攻击机制
- 多目标序列导航优化

---

## 🏗️ 系统架构

### 硬件架构

```
┌─────────────────────────────────────────┐
│           Vive Lighthouse基站            │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│        Vive追踪器（前/后）               │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│         Servant板 (ESP32)               │
│  ┌─────────────┐  ┌─────────────┐     │
│  │ Vive追踪    │  │ 电机控制    │     │
│  │ Web服务器   │  │ 编码器反馈  │     │
│  └─────────────┘  └─────────────┘     │
└─────────────┬───────────────────────────┘
              │ UART
              ↓
┌─────────────────────────────────────────┐
│         Owner板 (ESP32)                 │
│  ┌─────────────┐  ┌─────────────┐     │
│  │ ToF传感器   │  │ 导航决策    │     │
│  │ 行为控制    │  │ 模式切换    │     │
│  └─────────────┘  └─────────────┘     │
└─────────────┬───────────────────────────┘
              ↓
        控制输出
```

### 数据流

```
Vive追踪器 → Servant板
    ↓ 计算位置和角度
    ↓ UART传输 (100ms周期)
Owner板接收Vive数据
    ↓ 结合ToF传感器数据
    ↓ 执行导航决策
    ↓ UART发送控制命令
Servant板执行电机控制
```

---

## 📁 项目结构

```
510 final/
├── README.md                           # 本文档（项目总览）
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
│       ├── tof-3.ino                  # ToF传感器驱动
│       └── README_VIVE_NAV.md         # Vive导航详细文档
│
├── meam510_final_project/              # 参考代码/旧版本
│   ├── motor/                          # 电机控制参考代码
│   └── sensor/                         # 传感器参考代码
│
└── [PDF文档]                           # 项目要求文档
```

### 目录说明

#### `510finalgagac/gagac-2/` - Servant板

**职责**：
- 电机PWM控制和编码器反馈
- Vive追踪器数据读取和计算
- Web服务器（WiFi AP模式）
- 接收Owner板的控制命令并执行

**关键文件**：
- `gagac-2.ino` - 主控制循环，集成所有功能
- `gagac-web.h` - Web界面HTML/CSS/JavaScript
- `vive_tracker.*` - Vive追踪器驱动库
- `vive_utils.*` - 坐标计算工具函数

#### `510finalgagac/owner-4/` - Owner板

**职责**：
- ToF传感器数据采集
- 导航决策和路径规划
- 行为模式切换
- 向Servant板发送控制命令

**关键文件**：
- `owner-4.ino` - 主程序，模式切换和命令处理
- `behavior-vive.ino` - Vive导航算法实现
- `behavior-wall.ino` - 壁障跟随算法实现
- `tof-3.ino` - ToF传感器驱动

---

## 🚀 快速开始

### 前置要求

- Arduino IDE 1.8+ 或 2.0+
- ESP32开发板支持（安装ESP32开发板包）
- 所需库：
  - `WebServer`（ESP32内置）
  - `WiFi`（ESP32内置）
  - `HardwareSerial`（ESP32内置）
- 硬件：
  - 2x ESP32开发板
  - Vive Lighthouse基站和追踪器
  - ToF传感器（VL53L0X）x3
  - 电机驱动器和编码器

### 安装步骤

1. **克隆仓库**
   ```bash
   git clone [repository-url]
   cd "510 final"
   ```

2. **安装ESP32开发环境**
   - 打开Arduino IDE
   - 文件 → 首选项 → 附加开发板管理器网址：
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - 工具 → 开发板 → 开发板管理器 → 搜索"ESP32"并安装

3. **配置开发板**
   - Servant板：工具 → 开发板 → ESP32 Arduino → "ESP32 Dev Module"
   - Owner板：工具 → 开发板 → ESP32 Arduino → "ESP32 Dev Module"

4. **上传代码**
   - 先上传 `gagac-2/gagac-2.ino` 到Servant板
   - 再上传 `owner-4/owner-4.ino` 到Owner板

### 首次运行

1. 连接硬件（参考[硬件连接](#硬件连接)）
2. 上电两块ESP32板
3. 等待Vive系统同步（约5秒）
4. 连接到Servant板的WiFi（默认：`Robot_Control`）
5. 访问Web界面：http://192.168.4.1
6. 通过串口发送命令测试功能

---

## 🔌 硬件连接

### Servant板连接

#### 电机控制
```
MOTOR_L_PWM  → GPIO 9   (左电机PWM)
MOTOR_L_IN1  → GPIO 10  (左电机方向1)
MOTOR_L_IN2  → GPIO 11  (左电机方向2)

MOTOR_R_PWM  → GPIO 14  (右电机PWM)
MOTOR_R_IN1  → GPIO 12  (右电机方向1)
MOTOR_R_IN2  → GPIO 13  (右电机方向2)
```

#### 编码器
```
ENCODER_L_A  → GPIO 4   (左编码器A相)
ENCODER_L_B  → GPIO 5   (左编码器B相)

ENCODER_R_A  → GPIO 1   (右编码器A相)
ENCODER_R_B  → GPIO 2   (右编码器B相)
```

#### Vive追踪器
```
VIVE_PIN_FRONT → GPIO 18  (前追踪器)
VIVE_PIN_BACK  → GPIO 19  (后追踪器)
```

#### UART通信（与Owner板）
```
Servant GPIO17 (TX) → Owner GPIO18 (RX)
Servant GPIO18 (RX) → Owner GPIO17 (TX)
```

### Owner板连接

#### ToF传感器
```
前传感器  → I2C (SDA/SCL) - 粉色标记
右前传感器 → I2C (SDA/SCL) - 绿色标记
右后传感器 → I2C (SDA/SCL) - 蓝色标记
```

#### UART通信（与Servant板）
```
Owner GPIO17 (TX) → Servant GPIO18 (RX)
Owner GPIO18 (RX) → Servant GPIO17 (TX)
```

⚠️ **重要**：UART必须交叉连接！

### 电源要求

- Servant板：5V/3A（电机驱动）
- Owner板：5V/1A（传感器和逻辑）
- Vive追踪器：3.3V（通过ESP32供电）

---

## ⚙️ 软件配置

### WiFi配置

在 `gagac-2/gagac-2.ino` 中配置：

```cpp
// AP模式（默认，机器人作为WiFi热点）
const char* AP_SSID = "Robot_Control";
const char* AP_PASSWORD = "";

// 或Station模式（连接到现有WiFi）
// const char* SSID = "Your_WiFi_Name";
// const char* PASSWORD = "Your_Password";
```

### 目标点坐标配置

在 `owner-4/owner-4.ino` 中配置：

```cpp
// 单点导航目标
const float TARGET_RED_X = 2000.0;   // 红色目标X坐标 (mm)
const float TARGET_RED_Y = 2000.0;   // 红色目标Y坐标 (mm)
const float TARGET_BLUE_X = 6000.0;  // 蓝色目标X坐标 (mm)
const float TARGET_BLUE_Y = 2000.0;  // 蓝色目标Y坐标 (mm)

// 3位置序列导航目标
const float TARGET_LOC3_X = 4000.0;  // 位置3 X坐标 (mm)
const float TARGET_LOC3_Y = 4000.0;  // 位置3 Y坐标 (mm)
```

### 导航参数调整

在 `owner-4/behavior-vive.ino` 中调整：

```cpp
const float ANGLE_TOLERANCE = 20.0f;      // 角度容差（度）
const float DISTANCE_TOLERANCE = 5.0f;    // 到达距离阈值（mm）
```

### 超时设置

在 `owner-4/owner-4.ino` 中配置：

```cpp
const unsigned long AUTO_NAV_TIMEOUT = 30000;  // 30秒超时（毫秒）
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

1. 连接到机器人WiFi：`Robot_Control`
2. 打开浏览器访问：http://192.168.4.1
3. 功能：
   - 实时查看Vive坐标数据
   - 方向键或按钮控制移动
   - 速度/转向滑块调整
   - 启用/禁用Vive追踪
   - 启动/停止自动模式

### 3位置序列导航（比赛任务）

**要求**：自动访问3个Vive位置，每个位置30秒，共9分

**步骤**：
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
- 自动保持与右墙8cm距离
- 前方障碍物检测和转向
- 适用于封闭环境探索

**使用方法**：
1. 发送 `W` 命令启动
2. 机器人开始跟随右墙移动
3. 可通过其他命令切换模式

---

## 🏆 比赛任务对应

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

### ❌ 待开发任务

- Attack moving TA bot 3x (15 pts)
- Autonomous capture low tower (3 pts)
- Autonomous attack nexus 4x's (4 pts)
- Autonomous capture high tower (4 pts)
- Autonomous attack both towers and nexus (5 pts)

**详细分析**：参见 [任务完成情况分析.md](任务完成情况分析.md)

---

## 📚 文档说明

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

### 代码文档

- **gagac-2.ino**：Servant板主程序，包含详细注释
- **owner-4.ino**：Owner板主程序，模式切换逻辑
- **behavior-vive.ino**：Vive导航算法实现
- **behavior-wall.ino**：壁障跟随算法实现

---

## 🐛 故障排除

### 常见问题

#### 1. Vive数据无效

**症状**：串口显示Vive坐标为0，自动导航降级为壁障跟随

**解决方案**：
- 检查UART连接（必须交叉连接）
- 确认Vive基站正常工作
- 检查追踪器是否在覆盖范围内
- 通过Web界面确认Vive系统已启用

#### 2. 无法到达目标点

**解决方案**：
- 检查目标点坐标是否正确
- 调整导航参数（角度容差、距离阈值）
- 检查障碍物是否阻挡路径
- 提高导航速度

#### 3. Web界面无法访问

**解决方案**：
- 确认连接到机器人WiFi：`Robot_Control`
- 检查IP地址：http://192.168.4.1
- 确认Servant板WiFi正常启动
- 重启Servant板

**详细故障排除**：参见 [owner-4/README_VIVE_NAV.md](510finalgagac/owner-4/README_VIVE_NAV.md#故障排除)

---

## 🔧 开发与调试

### 串口输出

系统会输出详细的状态信息：

```
===== OWNER BOARD (Right Wall Logic + Vive Nav) =====
UART to servant ready. Waiting for Start...
Mode: 3LOCS | Loc: 1/3 | Time: 5000/30000 ms | Auto: ON | Vive: X=2500.0, Y=1800.0, A=45.0° | ToF: F=500, R1=200, R2=180
```

### 调试技巧

1. **查看Vive数据**：
   - Servant板串口：显示原始Vive数据
   - Web界面：实时坐标显示
   - Owner板串口：接收到的Vive数据

2. **测试导航**：
   - 先测试单点导航（`R` 或 `B`）
   - 再测试3位置序列（`V`）
   - 观察串口输出的状态信息

3. **参数调整**：
   - 从保守值开始（慢速、大容差）
   - 逐步优化速度和精度
   - 记录不同参数的效果

---

## 👥 开发团队

本项目为MEAM510课程最终项目。

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
**版本**：1.0  
**状态**：活跃开发中

---

## 🎯 下一步计划

- [ ] 实现完整一圈壁障跟随检测（10分）
- [ ] 添加目标识别和攻击机制
- [ ] 优化导航速度和路径规划
- [ ] 添加更多传感器融合
- [ ] 实现多目标序列导航优化

---

**祝使用愉快！** 🚀

