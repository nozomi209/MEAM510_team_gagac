## MEAM510 最终项目：Vive + ToF 自主导航（双 ESP32）

本仓库实现了一套 **双 ESP32（Servant/Owner）** 的移动底盘系统：Servant 负责 **电机/编码器 + Vive 计算 + Web UI**，Owner 负责 **ToF 感知 + 行为/规划 + 下发运动指令**。

### 代码入口（你真正需要看的）

- **Servant（电机 + Vive + Web）**：`510finalgagac/gagac-2/gagac-2.ino`
- **Owner（ToF + 巡墙 + 点到点 + 规划）**：`510finalgagac/owner-4/owner-4.ino`
- **巡墙逻辑（右墙 + 避障序列）**：`510finalgagac/owner-4/behavior-wall.ino`
- **Vive 导航函数（gotoPoint/避障）**：`510finalgagac/owner-4/behavior-vive.ino`
- **路径规划/手动规划库**：`510finalgagac/owner-4/manual_planner.*`
- **网页界面**：`510finalgagac/gagac-2/gagac-web.h`

---

### 硬件连接（以代码为准）

#### 1) UART（Servant ↔ Owner）

两块板都使用 UART1，并且都配置为：**TX=GPIO17，RX=GPIO18**。实际接线必须交叉：

- **Servant TX(GPIO17) → Owner RX(GPIO18)**
- **Servant RX(GPIO18) ← Owner TX(GPIO17)**

#### 2) Vive tracker（接在 Servant）

- `VIVE_PIN_FRONT` = **GPIO6**（车后左侧 tracker）
- `VIVE_PIN_BACK`  = **GPIO7**（车后右侧 tracker）

> 说明：代码里变量名是 `viveFront/viveBack`，但注释已写明实际安装是“车后两侧（左右）”。

#### 3) 电机/编码器（接在 Servant）

- 左电机：`PWM=9, IN1=10, IN2=11`
- 右电机：`PWM=14, IN1=12, IN2=13`
- 左编码器：`A=4, B=5`
- 右编码器：`A=2, B=1`

#### 4) ToF（接在 Owner）

Owner 通过 I2C 读取 3 路 ToF（前/右前/右后），具体初始化在 `ToF_init()`（`tof` 源文件）中。

#### 5) 其他（可选）

- TopHat I2C（Servant）：`SDA=GPIO15, SCL=GPIO16`
- 攻击伺服（Servant）：`SERVO_PIN=GPIO8`

---

### 烧录与启动（最短流程）

1. **先烧录 Servant**：打开 `510finalgagac/gagac-2/gagac-2.ino` 上传到 Servant 板  
2. **再烧录 Owner**：打开 `510finalgagac/owner-4/owner-4.ino` 上传到 Owner 板  
3. 上电两块板，打开串口监视器（115200）
4. 连接 WiFi：`ESP32-MobileBase`（密码：`12345678`），浏览器打开 `http://192.168.4.1`

---

### 运行方式（你怎么“用”）

#### 1) Web 手动控制（Servant 直接执行）

网页按钮/滑条通过 `/cmd?data=...` 下发运动命令，Servant 直接执行：

- `F<speed>` 前进，`B<speed>` 后退
- `L<rate>` 左转，`R<rate>` 右转
- `S` 停车

#### 2) Auto 开关（网页 → 转发给 Owner → Owner 输出运动命令）

网页发送：

- `AUTO_ON`：Owner 开始跑“自动行为”（由 Owner 当前状态决定输出什么）
- `AUTO_OFF`：Owner 停止自动并下发 `S`

#### 3) 点到点导航（Owner：Vive goto）

Owner 内置一个“点到点”控制器（先对角、再前进、近距离减速），核心参数在 `owner-4.ino`：

- `GOTO_DIST_TOL / GOTO_ANGLE_TOL / GOTO_SPEED_FAR / GOTO_SPEED_NEAR / GOTO_TURN_RATE`

它输出的指令仍然是同一套 `"F/L/R/S"`，通过 UART 发给 Servant 执行。

#### 4) 右墙巡航/避障（Owner：wall-follow）

`behavior-wall.ino` 的 `decideWallFollowing(F, R1, R2)` 用 3 路 ToF 做右墙跟随 + 前向避障序列 + 卡死脱困序列，输出同一套 `"F/L/R/S"` 指令。

#### 5) 网页“Plan to Target”（Owner：规划并执行）

网页发送规划命令（Servant 负责接收并 **原样转发** 给 Owner），Owner 在 `owner-4.ino` 解析后调用 `manual_planner`：

- `PLAN1:x,y`：从当前 Vive 起点规划到单目标并开始执行
- `PLAN_STOP`：停止规划执行（停车）
- `PLAN_BOUND:xmin,xmax,ymax,ymin`：设置外边界
- `PLAN_OBS:left,right,top,bottom[,margin]`：设置障碍框（+可选安全边距）
- `PLAN_OBS_OFF`：禁用障碍
- `PLAN_SET_START` 或 `PLAN_SET_START:x,y`：锁定起点
- `PLAN_CLEAR_START`：取消锁定起点（使用实时 Vive 起点）

---

### 功能是怎么实现的（只讲实现）

#### 1) Vive 位姿（Servant 计算）

Servant 同时读取两只 tracker 的坐标，计算：

- **位置**：两点中点作为 `viveX/viveY`
- **朝向**：两点连线方向经过 `atan2` + `VIVE_ANGLE_OFFSET` 得到 `viveAngle`（并归一化到 \([-180,180]\)）

然后通过 UART 以一行文本的方式发给 Owner（`VIVE:x,y,a`），Owner 用它做导航/规划。

#### 2) 指令协议（Owner → Servant）

Owner 永远只下发 **短命令字符串**：`F.. / B.. / L.. / R.. / S`。Servant 在 `handleCommand()` 里解析并转成左右轮目标转速，再由 PID/前馈闭环去驱动电机。

#### 3) 自动逻辑归属（Owner 负责“决策”）

Owner 读取 ToF + Vive（从 UART 来），再根据模式输出运动指令；Servant 负责“执行”并回报新的位姿。

---

### 常见要改的参数在哪

- **Vive 角度偏置**：`gagac-2.ino` 里的 `VIVE_ANGLE_OFFSET`
- **点到点导航阈值/速度**：`owner-4.ino` 里的 `GOTO_*`
- **右墙巡航/避障阈值与序列时间**：`behavior-wall.ino` 顶部的一组参数
- **规划边界/障碍/安全边距**：网页命令 `PLAN_BOUND/PLAN_OBS`（对应 Owner 里的 `mp_setBoundary/mp_setObstacleBox/mp_setPathMargin`）

---

### 相关文档（都只保留“怎么用 + 怎么实现”）

- `网页更新说明.md`：网页控制与规划命令（从网页到代码的对应关系）
- `任务完成情况分析.md`：比赛/演示时的最短操作流程（怎么跑起来）
- `510finalgagac/gagac-2/VIVE_TEST_GUIDE.md`：Vive 双点测试与校准
- `510finalgagac/owner-4/MANUAL_PLANNER_GUIDE.md`：`manual_planner` 接口与接入方式


