#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// ================= 引脚定义 / Pin Definitions =================
#define SERVO_PIN        A0
#define RELAY_PIN        A1
#define TRIG_PIN         A2
#define ECHO_PIN         A3
#define BUZZER_RED_PIN   0   // D0: 控制蜂鸣器与红灯 / Buzzer & Red LED
#define BUTTON_PIN       1   // D1: 室内开门按钮 / Indoor Push Button
#define GREEN_LED_PIN    9   // D9: 绿灯 / Green LED
#define RFID_SS_PIN      10  // D10: RFID SS/SDA Pin
#define RFID_RST_PIN     MFRC522::UNUSED_PIN // RST 接 Arduino RESET 脚

// ================= 硬件对象初始化 / Hardware Initialization =================
LiquidCrystal_I2C lcd(0x27, 16, 2); // 若屏幕未亮，尝试地址 0x3F / Address 0x27 or 0x3F
Servo doorServo;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// 矩阵键盘配置 (4行3列) / Keypad Configuration (4x3)
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================= 系统全局变量 / Global Variables =================
String correctPassword = "1234"; // 默认开门密码 / Default Password
String inputPassword = "";
int wrongAttempts = 0;
const int maxAttempts = 3;

// ================= 多卡注册支持 / Multi-Card Support =================
// 设置允许开门授权卡片总数 / Number of authorized cards
const byte NUM_CARDS = 2; 

// 存储多张已授权卡片 16 进制 UID / Array storing authorized card UIDs
byte registeredUIDs[NUM_CARDS][4] = {
  {0x5B, 0x57, 0xB6, 0xE3}, // 第 1 张卡（白色 IC 薄卡）） / Card 1
  {0x10, 0x12, 0x88, 0x56}, // 第 2 张卡（蓝色钥匙扣）  / Card 2
};


// 人体感应与 LCD 背光计时 / LCD Backlight Timer
unsigned long lastMotionTime = 0;
const unsigned long lcdTimeout = 5000; // 5秒无人靠近自动熄屏 / 5s Timeout

void setup() {
  // 引脚模式设置 / Pin Modes
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_RED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 内部上拉 / Internal Pullup

  // 默认初始状态 / Default Initial State
  digitalWrite(RELAY_PIN, HIGH);       // 锁舌伸出 (闭锁) / Solenoid Locked
  digitalWrite(BUZZER_RED_PIN, LOW);  // 蜂鸣器/红灯关闭 / Buzzer & Red LED Off
  digitalWrite(GREEN_LED_PIN, LOW);   // 绿灯关闭 / Green LED Off

  // 舵机初始化 / Servo Setup
  doorServo.attach(SERVO_PIN);
  doorServo.write(0);                 // 关门角度 0° / Closed Position 0°

  // LCD 初始化 / LCD Setup
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" Smart Door Lock ");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing... ");
  delay(1500);

  // RFID 初始化 / RFID Setup
  SPI.begin();
  rfid.PCD_Init();

  updateDisplay(" System Ready ", " Enter Password ");
  lastMotionTime = millis();
}

void loop() {
  // 1. 超声波检测：无人靠近时熄灭屏幕 / Proximity Check for LCD Backlight
  checkProximity();

  // 2. 检测室内开门按钮 / Indoor Button Check
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // 消抖 / Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      turnOnScreen();
      updateDisplay(" Indoor Button ", " Opening Door...");
      unlockDoor();
    }
  }

  // 3. 检测 RFID 刷卡开门 / RFID Card Check
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    turnOnScreen();
    
    // 获取卡片的 16 进制 UID 字符串 / Get Hex UID String
    String cardUID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) cardUID += "0";
      cardUID += String(rfid.uid.uidByte[i], HEX);
      if (i < rfid.uid.size - 1) cardUID += " ";
    }
    cardUID.toUpperCase(); // 转为大写，例如 "5B 57 B6 E3"

    if (checkRFIDUID(rfid.uid.uidByte, rfid.uid.size)) {
      updateDisplay(" RFID Verified ", " Welcome Home! ");
      unlockDoor();
    } else {
      // 刷错卡时，第 2 行直接显示读到的卡号 UID，方便记录！
      updateDisplay(" Card UID: ", cardUID);
      triggerShortWarning();
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

   // 4. 检测矩阵键盘输入 / Keypad Input Check
  char key = keypad.getKey();
  if (key) {
    turnOnScreen();
    playKeyTone(); // ⚠️ 每次按下密码键盘，蜂鸣器短响 40ms 提示音

    if (key == '#') { // '#' 键确认 / '#' as Confirm Key
      if (inputPassword == correctPassword) {
        wrongAttempts = 0;
        updateDisplay(" Pass Correct! ", " Opening Door...");
        unlockDoor();
      } else {
        wrongAttempts++;
        inputPassword = "";
        if (wrongAttempts >= maxAttempts) {
          triggerAlarm(); // 错 3 次触发报警 / Trigger Alarm after 3 Failures
          wrongAttempts = 0;
        } else {
          updateDisplay(" Wrong Password ", " Try Again: " + String(wrongAttempts) + "/3");
          triggerShortWarning();
        }
      }
    } else if (key == '*') { // '*' 键清空 / '*' as Clear Key
      inputPassword = "";
      updateDisplay(" Clear Input ", " Enter Password ");
    } else {
      if (inputPassword.length() < 8) { // 限制输入长度 / Limit Length
        inputPassword += key;
        String mask = "";
        for (int i = 0; i < inputPassword.length(); i++) mask += "*";
        updateDisplay("Enter Password:", mask);
      }
    }
  }
}

// ================= 功能子函数 / Functions =================

// 按键短促提示音 / Keypress Beep Tone
void playKeyTone() {
  digitalWrite(BUZZER_RED_PIN, HIGH);
  delay(40); // 响 40 毫秒短音
  digitalWrite(BUZZER_RED_PIN, LOW);
}

// 执行完整的开门逻辑 / Unlock Door Routine
// 通电收回铁锁 -> SG90 驱动开门 -> 延时 -> 关门闭锁
void unlockDoor() {
  // 1. 绿灯亮起 / Green LED On
  digitalWrite(GREEN_LED_PIN, HIGH);

  // 2. Solenoid lock 通电收回铁锁 / Retract Solenoid Lock Pin
  digitalWrite(RELAY_PIN, LOW);
  delay(1000); // 等待锁舌完全收回 / Wait for pin to retract

  // 3. SG90 舵机旋转 90 度打开门 / Rotate Servo to 90°
  doorServo.write(90);
  delay(4000); // 保持开门状态 4 秒 / Keep Door Open for 4s

  // 4. SG90 舵机旋转归位 0 度关门 / Servo back to 0°
  doorServo.write(0);
  delay(1200); // 等待门完全合上 / Wait for door to close

  // 5. 电磁锁断电，锁舌弹回锁定 / De-energize Relay, Lock Solenoid
  digitalWrite(RELAY_PIN, HIGH);

  // 6. 绿灯熄灭，恢复默认界面 / Green LED Off, Reset Screen
  digitalWrite(GREEN_LED_PIN, LOW);
  inputPassword = "";
  updateDisplay(" Door Locked ", " Enter Password ");
}

// 密码错 3 次触发 10 秒连续报警与红灯 / 10-Second Alarm & Red LED
void triggerAlarm() {
  updateDisplay(" SYSTEM LOCKED! ", " Alarm 10 Seconds ");
  digitalWrite(BUZZER_RED_PIN, HIGH); // 蜂鸣器响，红灯持续亮 / Buzzer & Red LED On

  for (int i = 10; i > 0; i--) {
    lcd.setCursor(0, 1);
    lcd.print(" Wait Time: ");
    if (i < 10) lcd.print("0");
    lcd.print(i);
    lcd.print("s ");
    delay(1000);
  }

  digitalWrite(BUZZER_RED_PIN, LOW); // 关闭蜂鸣器与红灯 / Buzzer & Red LED Off
  inputPassword = "";
  updateDisplay(" System Reset ", " Enter Password ");
}

// 单次错误提示音与红灯闪烁 / Short Warning Tone & Red LED Flash
void triggerShortWarning() {
  digitalWrite(BUZZER_RED_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_RED_PIN, LOW);
  delay(1000);
  updateDisplay(" Enter Password ", inputPassword);
}

// 超声波测距控制屏幕背光 / Ultrasonic Sensor LCD Backlight Control
void checkProximity() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms 超时 / Timeout
  float distance = duration * 0.034 / 2.0;

  // 检测前方 10cm 内有人 / Detect Person within 10cm 
  if (distance > 0 && distance <= 10) {
    turnOnScreen();
  } else {
    // 超时熄灭背光 / Turn off backlight when idle
    if (millis() - lastMotionTime > lcdTimeout) {
      lcd.noBacklight();
    }
  }
}

// 唤醒屏幕 / Wake LCD Backlight
void turnOnScreen() {
  lcd.backlight();
  lastMotionTime = millis();
}

// 多卡 UID 循环比对 / Verify RFID UID against multiple registered cards
bool checkRFIDUID(byte *readUID, byte bufferSize) {
  if (bufferSize != 4) return false;
  
  // 遍历所有已注册的卡片 UID / Loop through registered cards
  for (byte c = 0; c < NUM_CARDS; c++) {
    bool match = true;
    for (byte i = 0; i < 4; i++) {
      if (readUID[i] != registeredUIDs[c][i]) {
        match = false;
        break;
      }
    }
    if (match) return true; // 只要匹配到其中任意一张，直接开门 / Match found
  }
  return false;
}


// LCD 辅助显示函数 / LCD Display Helper
void updateDisplay(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}
