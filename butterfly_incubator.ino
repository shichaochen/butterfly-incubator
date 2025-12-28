/*
 * 蝴蝶孵化箱控制系统
 * 硬件：ESP32-C3, AM2302, OLED SSD1306, 继电器x2, 加热垫, 风扇, 加湿器
 * 功能：温湿度控制、OLED显示曲线、Blynk手机APP控制、按键本地设置
 * 作者：Shichao Chen  12/20/2025
 */

#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6-RWgnQzO"
#define BLYNK_TEMPLATE_NAME "Butterfly Incubator"
#define BLYNK_DEVICE_NAME "ButterflyIncubator"
#define BLYNK_AUTH_TOKEN "your_auth_token"

#include <WiFi.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>

// ========== 硬件引脚定义（ESP32-C3） ==========
// ESP32-C3 GPIO引脚：0,1,2,3,4,5,6,7,8,9,10,18,19,20,21
// 注意：GPIO8和GPIO9用于USB，避免使用
#define DHTPIN 2           // AM2302数据引脚 (GPIO2)
#define DHTTYPE DHT22     // AM2302等同于DHT22
#define SDA_PIN 4         // OLED I2C SDA (GPIO4)
#define SCL_PIN 5         // OLED I2C SCL (GPIO5)
#define RELAY_HEAT_PIN 6  // 加热垫继电器 (GPIO6)
#define RELAY_HUMID_PIN 7 // 加湿器继电器 (GPIO7)
#define FAN_PIN 10        // 风扇控制（PWM）(GPIO10)
#define BTN_SET_PIN 0     // 设置按键 (GPIO0, 注意：有些板子GPIO0是BOOT按钮)
#define BTN_UP_PIN 1      // 增加按键 (GPIO1)
#define BTN_DOWN_PIN 3    // 减少按键 (GPIO3)

// ========== PWM通道设置 ==========
#define FAN_PWM_CHANNEL 0
#define FAN_PWM_FREQ 5000
#define FAN_PWM_RESOLUTION 8

// ========== OLED显示设置 ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== DHT传感器 ==========
DHT dht(DHTPIN, DHTTYPE);

// ========== 温湿度控制参数 ==========
float targetTemp = 25.0;      // 目标温度（摄氏度）
float targetHumidity = 60.0;  // 目标湿度（%）
float tempHysteresis = 1.0;    // 温度滞回（避免频繁开关）
float humHysteresis = 5.0;     // 湿度滞回

// ========== 数据记录 ==========
const int MAX_DATA_POINTS = 120;  // OLED可显示的数据点数（128像素宽度）
float tempHistory[MAX_DATA_POINTS];
float humHistory[MAX_DATA_POINTS];
int dataIndex = 0;
unsigned long lastDataUpdate = 0;
const unsigned long DATA_UPDATE_INTERVAL = 30000; // 30秒更新一次数据
float currentTemp = 0;
float currentHum = 0;

// ========== 按键状态 ==========
bool btnSetState = false;
bool btnUpState = false;
bool btnDownState = false;
bool lastBtnSetState = false;
bool lastBtnUpState = false;
bool lastBtnDownState = false;
unsigned long lastDebounceTimeSet = 0;
unsigned long lastDebounceTimeUp = 0;
unsigned long lastDebounceTimeDown = 0;
const unsigned long DEBOUNCE_DELAY = 50;
bool btnSetPressed = false;
bool btnUpPressed = false;
bool btnDownPressed = false;

// ========== 设置模式 ==========
enum SettingMode {
  MODE_NORMAL,
  MODE_SET_TEMP,
  MODE_SET_HUMIDITY,
  MODE_SAVED
};
SettingMode currentMode = MODE_NORMAL;
unsigned long modeTimeout = 0;
const unsigned long MODE_TIMEOUT_MS = 15000; // 15秒无操作退出设置模式
unsigned long savedModeTimeout = 0;
const unsigned long SAVED_MODE_DURATION = 2000; // 保存提示显示2秒

// ========== 定时器 ==========
BlynkTimer timer;

// ========== 函数声明 ==========
void readSensor();
void updateDisplay();
void drawGraph();
void controlDevices();
void handleButtons();
void enterSettingMode(SettingMode mode);
void exitSettingMode();
void updateBlynk();

// ========== Blynk虚拟引脚 ==========
BLYNK_WRITE(V0) { // 目标温度设置
  targetTemp = param.asFloat();
  Serial.print("Blynk设置目标温度: ");
  Serial.println(targetTemp);
}

BLYNK_WRITE(V1) { // 目标湿度设置
  targetHumidity = param.asFloat();
  Serial.print("Blynk设置目标湿度: ");
  Serial.println(targetHumidity);
}

BLYNK_CONNECTED() {
  // 同步虚拟引脚值
  Blynk.syncVirtual(V0);
  Blynk.syncVirtual(V1);
  // 立即更新一次状态
  updateBlynk();
}

// ========== 初始化 ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 初始化引脚
  pinMode(RELAY_HEAT_PIN, OUTPUT);
  pinMode(RELAY_HUMID_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BTN_SET_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  
  // 初始化继电器（默认关闭）
  digitalWrite(RELAY_HEAT_PIN, LOW);
  digitalWrite(RELAY_HUMID_PIN, LOW);
  
  // 配置PWM通道（用于风扇控制）
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcAttachPin(FAN_PIN, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  
  // 初始化OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 Init Error"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Butterfly Incubator"));
  display.println(F("Initializing..."));
  display.display();
  
  // 初始化DHT传感器
  dht.begin();
  delay(2000);
  
  // 初始化数据数组
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    tempHistory[i] = 0;
    humHistory[i] = 0;
  }
  
  // 初始化WiFi和Blynk
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // 3分钟超时
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  
  // 显示WiFi配置提示
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("WiFi Config..."));
  display.println(F("AP: ButterflyIncubator"));
  display.println(F("Visit: 192.168.4.1"));
  display.display();
  
  if (!wifiManager.autoConnect("ButterflyIncubator")) {
    Serial.println("WiFi连接失败，重启中...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("WiFi Failed"));
    display.println(F("Restart in 3s..."));
    display.display();
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("WiFi已连接！");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
  
  // 连接Blynk
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Connecting Blynk..."));
  display.display();
  
  Blynk.config(BLYNK_AUTH_TOKEN);
  int retryCount = 0;
  while (!Blynk.connect() && retryCount < 10) {
    Serial.print("Blynk连接中... (");
    Serial.print(retryCount + 1);
    Serial.println(")");
    delay(1000);
    retryCount++;
  }
  
  if (Blynk.connected()) {
    Serial.println("Blynk已连接！");
  } else {
    Serial.println("Blynk连接失败，但系统将继续运行");
  }
  
  // 设置定时器
  timer.setInterval(2000L, readSensor);      // 每2秒读取传感器
  timer.setInterval(1000L, updateDisplay);   // 每1秒更新显示
  timer.setInterval(5000L, controlDevices);  // 每5秒控制设备
  timer.setInterval(50L, handleButtons);     // 每50ms处理按键（提高响应速度）
  timer.setInterval(5000L, updateBlynk);     // 每5秒更新Blynk
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("System Ready"));
  display.print(F("IP: "));
  display.println(WiFi.localIP());
  display.display();
  delay(2000);
}

// ========== 主循环 ==========
void loop() {
  Blynk.run();
  timer.run();
  
  // 检查设置模式超时
  if (currentMode != MODE_NORMAL && currentMode != MODE_SAVED && millis() > modeTimeout) {
    exitSettingMode();
  }
  
  // 检查保存提示模式超时
  if (currentMode == MODE_SAVED && millis() > savedModeTimeout) {
    currentMode = MODE_NORMAL;
  }
}

// ========== 读取传感器数据 ==========
void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT传感器读取失败！");
    return;
  }
  
  // 更新当前值
  currentTemp = t;
  currentHum = h;
  
  // 更新数据历史（每30秒记录一次）
  if (millis() - lastDataUpdate >= DATA_UPDATE_INTERVAL) {
    tempHistory[dataIndex] = t;
    humHistory[dataIndex] = h;
    dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
    lastDataUpdate = millis();
  }
  
  Serial.print("温度: ");
  Serial.print(t);
  Serial.print("°C, 湿度: ");
  Serial.print(h);
  Serial.println("%");
}

// ========== 更新OLED显示 ==========
void updateDisplay() {
  display.clearDisplay();
  
  if (currentMode == MODE_NORMAL) {
    // 正常显示模式：显示当前值和曲线
    // 显示当前温湿度
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("T:"));
    display.print(currentTemp, 1);
    display.print(F("C T:"));
    display.print(targetTemp, 1);
    display.print(F("C"));
    
    display.setCursor(0, 10);
    display.print(F("H:"));
    display.print(currentHum, 0);
    display.print(F("% H:"));
    display.print(targetHumidity, 0);
    display.print(F("%"));
    
    // 显示设备状态
    display.setCursor(0, 20);
    display.print(F("H:"));
    display.print(digitalRead(RELAY_HEAT_PIN) ? "ON " : "OFF");
    display.print(F(" M:"));
    display.print(digitalRead(RELAY_HUMID_PIN) ? "ON" : "OFF");
    
    // 绘制温湿度曲线
    drawGraph();
  } else if (currentMode == MODE_SET_TEMP) {
    // 设置温度模式
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Set Temperature"));
    display.setTextSize(3);
    display.setCursor(15, 25);
    display.print(targetTemp, 1);
    display.print(F("C"));
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print(F("UP+  DOWN-  SET Next"));
  } else if (currentMode == MODE_SET_HUMIDITY) {
    // 设置湿度模式
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Set Humidity"));
    display.setTextSize(3);
    display.setCursor(20, 25);
    display.print(targetHumidity, 0);
    display.print(F("%"));
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print(F("UP+  DOWN-  SET Save"));
  } else if (currentMode == MODE_SAVED) {
    // 保存成功提示
    display.setTextSize(2);
    display.setCursor(25, 20);
    display.print(F("Saved"));
    display.setTextSize(1);
    display.setCursor(15, 45);
    display.print(F("Back to Normal"));
  }
  
  display.display();
}

// ========== 绘制温湿度曲线 ==========
void drawGraph() {
  // 找到最小最大值
  float minTemp = 100, maxTemp = -100;
  float minHum = 100, maxHum = 0;
  int validTempPoints = 0;
  int validHumPoints = 0;
  
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    if (tempHistory[i] > 0) {
      validTempPoints++;
      if (tempHistory[i] < minTemp) minTemp = tempHistory[i];
      if (tempHistory[i] > maxTemp) maxTemp = tempHistory[i];
    }
    if (humHistory[i] > 0) {
      validHumPoints++;
      if (humHistory[i] < minHum) minHum = humHistory[i];
      if (humHistory[i] > maxHum) maxHum = humHistory[i];
    }
  }
  
  // 如果没有有效数据，使用当前值作为默认范围
  if (validTempPoints == 0) {
    minTemp = currentTemp - 5;
    maxTemp = currentTemp + 5;
  }
  if (validHumPoints == 0) {
    minHum = currentHum - 10;
    maxHum = currentHum + 10;
  }
  
  // 添加一些边距
  float tempRange = maxTemp - minTemp;
  float humRange = maxHum - minHum;
  if (tempRange < 5) {
    float center = (minTemp + maxTemp) / 2;
    minTemp = center - 2.5;
    maxTemp = center + 2.5;
    tempRange = 5;
  }
  if (humRange < 10) {
    float center = (minHum + maxHum) / 2;
    minHum = center - 5;
    maxHum = center + 5;
    humRange = 10;
  }
  
  // 绘制温度曲线（上半部分，32-40像素）
  int graphYTop = 32;
  int graphYBottom = 40;
  int graphHeight = graphYBottom - graphYTop;
  int graphWidth = 128;
  
  // 只绘制有效数据点
  int pointsToDraw = min(MAX_DATA_POINTS, graphWidth);
  
  for (int i = 1; i < pointsToDraw; i++) {
    int idx1 = (dataIndex - i - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    int idx2 = (dataIndex - i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    
    if (tempHistory[idx1] > 0 && tempHistory[idx2] > 0) {
      int x1 = graphWidth - i;
      int x2 = graphWidth - i + 1;
      int y1 = graphYBottom - map((int)(tempHistory[idx1] * 10), (int)(minTemp * 10), (int)(maxTemp * 10), 0, graphHeight);
      int y2 = graphYBottom - map((int)(tempHistory[idx2] * 10), (int)(minTemp * 10), (int)(maxTemp * 10), 0, graphHeight);
      // 限制Y坐标在范围内
      y1 = constrain(y1, graphYTop, graphYBottom);
      y2 = constrain(y2, graphYTop, graphYBottom);
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
  }
  
  // 绘制湿度曲线（下半部分，42-57像素）
  graphYTop = 42;
  graphYBottom = 57;
  graphHeight = graphYBottom - graphYTop;
  
  for (int i = 1; i < pointsToDraw; i++) {
    int idx1 = (dataIndex - i - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    int idx2 = (dataIndex - i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    
    if (humHistory[idx1] > 0 && humHistory[idx2] > 0) {
      int x1 = graphWidth - i;
      int x2 = graphWidth - i + 1;
      int y1 = graphYBottom - map((int)(humHistory[idx1] * 10), (int)(minHum * 10), (int)(maxHum * 10), 0, graphHeight);
      int y2 = graphYBottom - map((int)(humHistory[idx2] * 10), (int)(minHum * 10), (int)(maxHum * 10), 0, graphHeight);
      // 限制Y坐标在范围内
      y1 = constrain(y1, graphYTop, graphYBottom);
      y2 = constrain(y2, graphYTop, graphYBottom);
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
  }
  
  // 绘制分隔线
  display.drawLine(0, 40, 128, 40, SSD1306_WHITE);
  display.drawLine(0, 42, 128, 42, SSD1306_WHITE);
}

// ========== 控制设备 ==========
void controlDevices() {
  // 使用全局变量中的当前值（已在readSensor中更新）
  if (isnan(currentTemp) || isnan(currentHum)) {
    return;
  }
  
  // 控制加热垫
  if (currentTemp < targetTemp - tempHysteresis) {
    digitalWrite(RELAY_HEAT_PIN, HIGH);
  } else if (currentTemp > targetTemp + tempHysteresis) {
    digitalWrite(RELAY_HEAT_PIN, LOW);
  }
  
  // 控制加湿器
  if (currentHum < targetHumidity - humHysteresis) {
    digitalWrite(RELAY_HUMID_PIN, HIGH);
  } else if (currentHum > targetHumidity + humHysteresis) {
    digitalWrite(RELAY_HUMID_PIN, LOW);
  }
  
  // 控制风扇（当温度过高时开启）
  if (currentTemp > targetTemp + 3) {
    ledcWrite(FAN_PWM_CHANNEL, 255); // 全速
  } else if (currentTemp > targetTemp + 1) {
    ledcWrite(FAN_PWM_CHANNEL, 128); // 半速
  } else {
    ledcWrite(FAN_PWM_CHANNEL, 0); // 关闭
  }
}

// ========== 处理按键 ==========
void handleButtons() {
  // 跳过保存提示模式下的按键处理
  if (currentMode == MODE_SAVED) {
    return;
  }
  
  // 读取按键状态（低电平有效）
  bool currentBtnSet = !digitalRead(BTN_SET_PIN);
  bool currentBtnUp = !digitalRead(BTN_UP_PIN);
  bool currentBtnDown = !digitalRead(BTN_DOWN_PIN);
  
  // 设置按键防抖处理
  if (currentBtnSet != lastBtnSetState) {
    lastDebounceTimeSet = millis();
  }
  if ((millis() - lastDebounceTimeSet) > DEBOUNCE_DELAY) {
    // 防抖时间已过，检查状态是否稳定
    if (currentBtnSet != btnSetState) {
      // 状态发生变化，检查是否是按下事件（从低到高）
      if (currentBtnSet && !btnSetState) {
        btnSetPressed = true;
      }
      btnSetState = currentBtnSet;
    }
    lastBtnSetState = btnSetState;
  } else {
    // 防抖中，保持上次状态
    lastBtnSetState = currentBtnSet;
  }
  
  // 增加按键防抖处理
  if (currentBtnUp != lastBtnUpState) {
    lastDebounceTimeUp = millis();
  }
  if ((millis() - lastDebounceTimeUp) > DEBOUNCE_DELAY) {
    if (currentBtnUp != btnUpState) {
      if (currentBtnUp && !btnUpState) {
        btnUpPressed = true;
      }
      btnUpState = currentBtnUp;
    }
    lastBtnUpState = btnUpState;
  } else {
    lastBtnUpState = currentBtnUp;
  }
  
  // 减少按键防抖处理
  if (currentBtnDown != lastBtnDownState) {
    lastDebounceTimeDown = millis();
  }
  if ((millis() - lastDebounceTimeDown) > DEBOUNCE_DELAY) {
    if (currentBtnDown != btnDownState) {
      if (currentBtnDown && !btnDownState) {
        btnDownPressed = true;
      }
      btnDownState = currentBtnDown;
    }
    lastBtnDownState = btnDownState;
  } else {
    lastBtnDownState = currentBtnDown;
  }
  
  // 处理按键事件（只处理一次按下事件）
  if (btnSetPressed) {
    btnSetPressed = false;
    // 设置按键：切换设置模式
    if (currentMode == MODE_NORMAL) {
      enterSettingMode(MODE_SET_TEMP);
      Serial.println("进入温度设置模式");
    } else if (currentMode == MODE_SET_TEMP) {
      enterSettingMode(MODE_SET_HUMIDITY);
      Serial.println("进入湿度设置模式");
    } else if (currentMode == MODE_SET_HUMIDITY) {
      exitSettingMode();
      Serial.println("保存设置并退出");
    }
  }
  
  if (btnUpPressed) {
    btnUpPressed = false;
    // 增加按键
    if (currentMode == MODE_SET_TEMP) {
      targetTemp += 0.5;
      if (targetTemp > 40) targetTemp = 40;
      modeTimeout = millis() + MODE_TIMEOUT_MS;
      Serial.print("设置温度: ");
      Serial.println(targetTemp);
    } else if (currentMode == MODE_SET_HUMIDITY) {
      targetHumidity += 5;
      if (targetHumidity > 95) targetHumidity = 95;
      modeTimeout = millis() + MODE_TIMEOUT_MS;
      Serial.print("设置湿度: ");
      Serial.println(targetHumidity);
    }
  }
  
  if (btnDownPressed) {
    btnDownPressed = false;
    // 减少按键
    if (currentMode == MODE_SET_TEMP) {
      targetTemp -= 0.5;
      if (targetTemp < 10) targetTemp = 10;
      modeTimeout = millis() + MODE_TIMEOUT_MS;
      Serial.print("设置温度: ");
      Serial.println(targetTemp);
    } else if (currentMode == MODE_SET_HUMIDITY) {
      targetHumidity -= 5;
      if (targetHumidity < 20) targetHumidity = 20;
      modeTimeout = millis() + MODE_TIMEOUT_MS;
      Serial.print("设置湿度: ");
      Serial.println(targetHumidity);
    }
  }
}

// ========== 进入设置模式 ==========
void enterSettingMode(SettingMode mode) {
  currentMode = mode;
  modeTimeout = millis() + MODE_TIMEOUT_MS;
}

// ========== 退出设置模式 ==========
void exitSettingMode() {
  // 同步到Blynk
  Blynk.virtualWrite(V0, targetTemp);
  Blynk.virtualWrite(V1, targetHumidity);
  
  // 显示保存成功提示
  currentMode = MODE_SAVED;
  savedModeTimeout = millis() + SAVED_MODE_DURATION;
  
  Serial.println("设置已保存");
}

// ========== 更新Blynk ==========
void updateBlynk() {
  // 使用全局变量中的当前值
  if (!isnan(currentTemp) && !isnan(currentHum)) {
    Blynk.virtualWrite(V2, currentTemp);  // 当前温度
    Blynk.virtualWrite(V3, currentHum);   // 当前湿度
    Blynk.virtualWrite(V4, targetTemp);   // 目标温度
    Blynk.virtualWrite(V5, targetHumidity); // 目标湿度
    Blynk.virtualWrite(V6, digitalRead(RELAY_HEAT_PIN) ? 1 : 0); // 加热状态
    Blynk.virtualWrite(V7, digitalRead(RELAY_HUMID_PIN) ? 1 : 0); // 加湿状态
  }
}

