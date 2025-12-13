## turn_test_vive（90°转向标定）

### 怎么用
- 用 Arduino IDE 打开文件夹：`510finalgagac/turn_test_vive/`
- 编译并烧录 `turn_test_vive.ino` 到 **Servant**（电机/VIVE 在这块板上）
- 打开串口监视器：`115200`，行尾选择“Newline”

### 常用命令
- `HELP`：看全部命令
- `P`：打印当前参数
- `R` / `L`：默认 90° 右转/左转
- `R45` / `L120`：指定角度
- `F=520`：快转 PWM
- `S=260`：慢转 PWM
- `SW=25`：剩余角小于等于 25° 时切慢转
- `LEAD=3`：提前 3° 准备停（减少过冲）
- `BRAKE=80,180`：反向刹车 80ms，PWM=180（`BRAKE=0,0` 关闭）
- `TOL=2`：允许误差 2°
- `STOP`：立刻停


