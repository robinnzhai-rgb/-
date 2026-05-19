/*
 * 智能家居手势控制平台 v2.0
 * 主控: ESP32-S3
 * 模块: PAJ7620U2 | SSD1306 OLED | WS2812B | 无源蜂鸣器
 */

#include <Wire.h>
#include "paj7620.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- 引脚定义 ----
#define RGB_PIN       13
#define BUZZER_PIN    15
#define SCL_PIN       11
#define SDA_PIN       12
#define NUM_LEDS      16

// ---- OLED ----
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// ---- 状态变量 ----
bool  lightsOn    = false;
int   brightness  = 128;   // 0-255
int   modeIndex   = 0;     // 0=温馨 1=影院 2=阅读 3=浪漫

const char* modeNames[] = {"Warm", "Cinema", "Reading", "Romantic"};

// 各模式 RGB 颜色
const uint8_t modeColors[4][3] = {
  {255, 147,  41},  // 温馨 - 暖橙
  { 20,  20,  60},  // 影院 - 深蓝紫
  {255, 255, 200},  // 阅读 - 冷白
  {255,  20, 100},  // 浪漫 - 玫红
};

// ---- 蜂鸣器 ----
void beep(int freq, int dur) {
  tone(BUZZER_PIN, freq, dur);
}

// ---- 灯带更新 ----
void updateStrip() {
  if (!lightsOn) {
    strip.clear();
    strip.show();
    return;
  }
  uint8_t r = (modeColors[modeIndex][0] * brightness) / 255;
  uint8_t g = (modeColors[modeIndex][1] * brightness) / 255;
  uint8_t b = (modeColors[modeIndex][2] * brightness) / 255;
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// ---- OLED 更新 ----
void updateOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // 标题
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart Home Gesture");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // 灯光状态
  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(lightsOn ? "ON " : "OFF");

  // 模式
  display.setTextSize(1);
  display.setCursor(50, 14);
  display.print("Mode:");
  display.setCursor(50, 24);
  display.print(modeNames[modeIndex]);

  // 亮度进度条
  display.setCursor(0, 36);
  display.print("Bright:");
  display.drawRect(50, 36, 78, 8, SSD1306_WHITE);
  int barW = (brightness * 76) / 255;
  display.fillRect(51, 37, barW, 6, SSD1306_WHITE);

  // 亮度百分比
  display.setCursor(50, 48);
  display.print(brightness * 100 / 255);
  display.print("%");

  // 能耗估算 (简单线性)
  display.setCursor(80, 48);
  display.print("~");
  display.print(lightsOn ? (brightness * NUM_LEDS * 60 / 255 / 1000) : 0);
  display.print("W");

  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // OLED 初始化
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 28);
  display.print("Gesture Control");
  display.display();

  // 灯带初始化
  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  strip.show();

  // PAJ7620 初始化
  if (paj7620Init()) {
    Serial.println("PAJ7620 init failed");
    display.clearDisplay();
    display.setCursor(0, 28);
    display.print("Gesture ERR!");
    display.display();
  }

  delay(500);
  updateOLED();
  beep(1000, 200);
}

void loop() {
  uint8_t data = 0;
  paj7620ReadReg(0x43, 1, &data);

  if (data) {
    switch (data) {

      case GES_UP_FLAG:          // 上挥 - 亮度增加
        brightness += 51;
        if (brightness > 255) brightness = 255;
        if (lightsOn) updateStrip();
        beep(1200, 80);
        break;

      case GES_DOWN_FLAG:        // 下挥 - 亮度降低
        brightness -= 51;
        if (brightness < 0) brightness = 0;
        if (lightsOn) updateStrip();
        beep(800, 80);
        break;

      case GES_LEFT_FLAG:        // 左挥 - 关灯
        lightsOn = false;
        updateStrip();
        beep(500, 150);
        break;

      case GES_RIGHT_FLAG:       // 右挥 - 开灯
        lightsOn = true;
        updateStrip();
        beep(1500, 150);
        break;

      case GES_CLOCKWISE_FLAG:   // 顺时针 - 切换模式
        modeIndex = (modeIndex + 1) % 4;
        if (lightsOn) updateStrip();
        // 差异化音效：不同模式不同音调
        beep(800 + modeIndex * 200, 100);
        delay(120);
        beep(800 + modeIndex * 200 + 100, 100);
        break;

      case GES_FORWARD_FLAG:     // 前推 - 切换开关（toggle）
        lightsOn = !lightsOn;
        updateStrip();
        beep(lightsOn ? 1400 : 600, 200);
        break;
    }

    updateOLED();
    Serial.printf("Gesture:%d  Light:%s  Mode:%s  Bright:%d\n",
                  data, lightsOn ? "ON" : "OFF", modeNames[modeIndex], brightness);
  }

  delay(150);
}
