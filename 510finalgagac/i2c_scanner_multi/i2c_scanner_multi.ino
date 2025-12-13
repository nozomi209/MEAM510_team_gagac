#include <Wire.h>

/*
 * i2c_scanner_multi.ino
 *
 * 用途：
 * - 当普通 scanner 扫不到任何设备时，用这个“多引脚/多频率”扫描器快速定位：
 *   1) 是不是 SDA/SCL 接错到别的 GPIO
 *   2) 是不是时钟太快（尝试 100k/50k/10k）
 *
 * 使用：
 * - 烧录到 Servant 板
 * - 打开串口监视器 115200 / Newline
 * - 观察每组 (SDA,SCL,Clock) 是否能扫到任何地址
 */

struct Pair { int sda; int scl; const char* name; };

// 常见候选：你工程里用 15/16；另外加几组 ESP32 常用组合以防接线接到默认 I2C
static const Pair PAIRS[] = {
  {15, 16, "project(15/16)"},
  {16, 15, "swap(16/15)"},
  {21, 22, "esp32-default(21/22)"},
  {22, 21, "swap(22/21)"},
  {8,  9,  "alt(8/9)"},
  {9,  8,  "swap(9/8)"},
};

static const uint32_t CLOCKS[] = {100000, 50000, 10000};

static void printLineLevels(int sda, int scl) {
  // 先用内部上拉把线拉高（弱上拉），再读电平用来判断是否“被拉死/短路”
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delay(5);
  int sdaLevel = digitalRead(sda);
  int sclLevel = digitalRead(scl);
  Serial.printf("  [LINES] SDA=%d(%s) SCL=%d(%s)\n",
                sda, (sdaLevel ? "HIGH" : "LOW"),
                scl, (sclLevel ? "HIGH" : "LOW"));
  if (!sdaLevel || !sclLevel) {
    Serial.println("  [WARN] 线被拉低：常见原因=接反/短路到GND/模块未上电但拉低/上拉缺失且外部电路强拉低");
  }
}

static void scanOnce(int sda, int scl, uint32_t hz) {
  // ESP32 内部上拉（约 30k~50k，偏弱）：用于“临时验证总线是否能起来”
  // 正式接线仍建议在 SDA/SCL 上各加 4.7k~10k 外部上拉到 3.3V
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);

  printLineLevels(sda, scl);

  Wire.begin(sda, scl);
  Wire.setClock(hz);
  Wire.setTimeOut(50);

  // 轻微延时让总线稳定
  delay(20);

  int n = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  FOUND: 0x%02X\n", addr);
      n++;
    }
  }

  if (n == 0) Serial.println("  (no devices)");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== I2C Scanner Multi (ESP32 Arduino) ===");
  Serial.println("Tip: 若仍然全扫不到，99% 是供电/共地/上拉/电平/接线问题。");
}

void loop() {
  for (uint32_t hz : CLOCKS) {
    Serial.printf("\n--- Clock: %lu Hz ---\n", (unsigned long)hz);
    for (auto &p : PAIRS) {
      Serial.printf("[SCAN] %s SDA=%d SCL=%d\n", p.name, p.sda, p.scl);
      scanOnce(p.sda, p.scl, hz);
      delay(50);
    }
  }

  Serial.println("Rescan in 3s...\n");
  delay(3000);
}


