# 快速开始指南

本指南将帮助您快速搭建和运行蝴蝶孵化箱控制系统。

## 准备工作（15分钟）

### 1. 安装Arduino IDE

- 下载 [Arduino IDE 2.x](https://www.arduino.cc/en/software) 或 [Arduino IDE 1.8.x](https://www.arduino.cc/en/software)
- 安装到您的电脑

### 2. 安装ESP32开发板支持

1. 打开Arduino IDE
2. **文件** → **首选项** → **附加开发板管理器网址**
3. 添加以下网址：
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. **工具** → **开发板** → **开发板管理器**
5. 搜索 "esp32" 并安装 **esp32 by Espressif Systems**

### 3. 安装所需库

在Arduino IDE中，**工具** → **管理库**，搜索并安装以下库：

1. **Blynk** (版本 1.2.0) - 作者：Volodymyr Shymanskyy
2. **DHT sensor library** - 作者：Adafruit
3. **Adafruit GFX Library** - 作者：Adafruit
4. **Adafruit SSD1306** - 作者：Adafruit
5. **WiFiManager** - 作者：tzapu

### 4. 配置Blynk（可选，用于手机APP控制）

1. 在手机上下载 **Blynk** APP
2. 注册账号并创建新项目
3. 选择硬件：**ESP32**，连接类型：**WiFi**
4. 复制 **Auth Token** 到代码中

详细配置请参考 [BLYNK_SETUP.md](BLYNK_SETUP.md)

## 硬件连接（30分钟）

### 快速接线清单

按照以下顺序连接：

1. **AM2302传感器**
   - VCC → 3.3V
   - DATA → GPIO 2
   - GND → GND
   - DATA到3.3V之间接4.7kΩ电阻（如果模块没有内置）

2. **OLED显示屏**
   - VCC → 3.3V
   - GND → GND
   - SDA → GPIO 4
   - SCL → GPIO 5

3. **按键（3个）**
   - 设置按键：一端 → GPIO 0，另一端 → GND
   - 增加按键：一端 → GPIO 1，另一端 → GND
   - 减少按键：一端 → GPIO 3，另一端 → GND

4. **继电器模块（2个）**
   - 继电器1（加热）：IN → GPIO 6，VCC → 5V，GND → GND
   - 继电器2（加湿）：IN → GPIO 7，VCC → 5V，GND → GND

5. **风扇**
   - 正极 → 5V（或通过继电器）
   - 负极 → GND
   - PWM控制 → GPIO 10（如果支持PWM）

6. **220V设备（⚠️ 专业电工操作）**
   - 加热垫通过继电器1控制
   - 加湿器通过继电器2控制

详细接线图请参考 [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md)

## 软件配置（10分钟）

### 1. 打开代码

1. 打开 `butterfly_incubator.ino` 文件
2. 修改Blynk配置（如果使用Blynk）：

```cpp
#define BLYNK_AUTH_TOKEN "你的Auth Token"
```

### 2. 选择开发板

1. **工具** → **开发板** → **ESP32 Arduino** → **ESP32C3 Dev Module**
2. **工具** → **端口** → 选择ESP32-C3的串口
3. **工具** → **Upload Speed** → **921600**

### 3. 上传代码

1. 点击 **上传** 按钮（→）
2. 等待编译和上传完成
3. 如果上传失败，尝试降低Upload Speed到115200

## 首次运行（5分钟）

### 1. WiFi配置

1. 上电后，ESP32-C3会创建WiFi热点：**ButterflyIncubator**
2. 用手机连接该热点（无密码或查看串口输出）
3. 浏览器会自动打开配置页面（或访问 http://192.168.4.1）
4. 选择您的WiFi网络并输入密码
5. 点击保存

### 2. 验证运行

1. 打开Arduino IDE的串口监视器（115200波特率）
2. 查看输出信息：
   - WiFi连接状态
   - Blynk连接状态（如果配置了）
   - 传感器数据

3. 观察OLED显示屏：
   - 应该显示当前温湿度
   - 显示目标温湿度
   - 显示设备状态
   - 显示温湿度曲线

## 功能测试

### 测试传感器

- 观察OLED上的温度湿度数值
- 用手触摸传感器，温度应该上升
- 向传感器哈气，湿度应该上升

### 测试按键

1. **设置键**：按一次进入温度设置模式
2. **增加键**：在设置模式下增加数值
3. **减少键**：在设置模式下减少数值
4. **设置键**：再按一次进入湿度设置模式
5. **设置键**：再按一次退出设置模式

### 测试控制逻辑

1. **温度控制**：
   - 设置目标温度高于当前温度
   - 观察加热继电器是否开启（LED指示灯）
   - 设置目标温度低于当前温度
   - 观察风扇是否开启

2. **湿度控制**：
   - 设置目标湿度高于当前湿度
   - 观察加湿继电器是否开启

### 测试Blynk（如果配置了）

1. 打开Blynk APP
2. 进入项目并点击播放按钮
3. 检查数据是否更新
4. 尝试通过APP设置目标温湿度
5. 观察设备是否响应

## 常见问题快速解决

### 上传失败

- 检查USB线是否支持数据传输
- 尝试按住BOOT按钮再点击上传
- 降低Upload Speed到115200
- 检查驱动是否正确安装

### WiFi连接失败

- 检查WiFi密码是否正确
- 确保WiFi是2.4GHz（ESP32-C3不支持5GHz）
- 检查WiFi信号强度
- 重新配置WiFi（重启设备）

### 传感器无数据

- 检查接线是否正确
- 检查上拉电阻（4.7kΩ）
- 检查传感器供电
- 查看串口输出错误信息

### OLED无显示

- 检查I2C连接（SDA/SCL）
- 检查I2C地址（尝试0x3C或0x3D）
- 检查供电电压
- 检查接线是否牢固

### Blynk连接失败

- 检查Auth Token是否正确
- 确保设备已连接到互联网
- 检查Blynk服务器状态
- 查看串口输出连接状态

## 下一步

- 阅读 [README.md](README.md) 了解详细功能
- 阅读 [BLYNK_SETUP.md](BLYNK_SETUP.md) 配置手机APP
- 阅读 [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md) 了解详细接线
- 根据实际需求调整控制参数

## 安全提醒

⚠️ **重要安全提示**

1. **220V高压危险**：必须由专业电工操作
2. **防火安全**：不要在无人看管时长时间运行
3. **设备保护**：确保加湿器有足够的水
4. **定期检查**：定期检查线路和连接

## 技术支持

如果遇到问题：

1. 查看串口输出获取错误信息
2. 检查所有连接
3. 参考详细文档
4. 查看Arduino和ESP32官方文档

祝您使用愉快！🦋

