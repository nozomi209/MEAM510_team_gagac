# 手动规划使用说明（manual_planner.ino）

## 1. 路点配置
- 文件：`owner-4/manual_planner.ino`
- 修改 `route[]` 为你的路径点（单位 mm），顺序即行驶顺序：
  ```cpp
  static Waypoint route[] = {
    {1000.0f, 1000.0f}, // 起点
    {1500.0f, 2000.0f}, // 中间点
    {2000.0f, 2500.0f}  // 终点（按钮位置）
  };
  static const uint8_t ROUTE_COUNT = sizeof(route) / sizeof(route[0]);
  ```
- 坐标系：与 VIVE 数据一致，0° 沿 +Y 方向。

## 2. 接口说明
- `mp_setRoute(route, ROUTE_COUNT)`：启动/重置路点序列。
- `mp_step(viveX, viveY, viveAngle)`：输入当前姿态，返回一条 `"F/L/R/S"` 指令（含角度对齐、近距离减速）。到达最后路点后自动返回 `"S"` 并结束。
- `mp_stop()`：停止规划。
- `mp_isActive()`：是否仍在执行路点。

## 3. 参数可调
位于 `manual_planner.ino` 顶部：
- `MP_DIST_TOL`：到点距离阈值（默认 50 mm）。
- `MP_ANGLE_TOL`：朝向容差（默认 20°）。
- `MP_SPEED_FAR` / `MP_SPEED_NEAR`：远/近距离前进速度。
- `MP_TURN_RATE`：原地转向力度。

### 3.1（新增）低频“定位—再规划—执行”模式（推荐用于 Vive 角度/位置漂移）
当场地上出现 **Vive 角度/位置偏差较大** 时，可以开启低频滚动控制：**每走一小段就强制停车 → 多次采样确认当前位置稳定 → 从该位置重新规划剩余路径 → 再走下一小段**。

- **作为导航默认模式**：
  - 低频模式已设为**默认启用**，用于替代旧的连续高频纠偏方式（避免误用导致偏差放大）。
  - `MP_PARAM:MP_LF_ENABLED=1` 仍可重复发送（等价于“确保开启”）。
- **常用可调参数**（单位见注释，均可用 `MP_PARAM:键=值` 动态修改）：
  - `MP_LF_EXEC_MS`：每段执行时长（默认 900ms）
  - `MP_LF_STOP_MIN_MS`：每段定位的最短停车时间（默认 250ms）
  - `MP_LF_FIX_N`：定位阶段需要的采样数（默认 8 次，`mp_step()` 每调用一次计一次采样）
  - `MP_LF_POS_STD_MAX`：位置稳定阈值（默认 30mm，标准差）
  - `MP_LF_ANG_STD_MAX`：角度稳定阈值（默认 12°，标准差）

## 4. 在 Owner loop 中接入（示例）
假设已从 Servant 收到 VIVE 坐标并存入 `viveX, viveY, viveAngle`：
```cpp
// 初始化（如在 setup 或接收到新路线命令时）
mp_setRoute(route, ROUTE_COUNT);
isViveGoto = false;      // 避免与 GOTO 逻辑冲突
isAutoRunning = false;   // 关闭巡墙

// 在 loop 中：
if (mp_isActive()) {
  String cmd = mp_step(viveX, viveY, viveAngle);
  sendToServant(cmd);    // 复用现有下发接口
}
```

## 5. 数据来源
- Owner 需持续接收 Servant 发来的 `VIVE:x,y,a` 字符串，并更新 `viveX/viveY/viveAngle`（现有代码已支持）。

## 6. 注意事项
- 确认 `VIVE_ANGLE_OFFSET` 方向正确，角度需归一化到 [-180,180]（现有代码已处理）。
- 路点间距较大时，可适当放宽角度容差或提高远距离速度；靠近终点时速度自动降档。
- 若需要中途停止，调用 `mp_stop()` 或手动发送 `"S"`。***

