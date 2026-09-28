本教程包含详细的引脚连接表、电源分配方案（支持 12V 电磁锁）以及完整的 Arduino C++ 源码（支持多张 RFID 卡片注册与动态修改密码）。
This tutorial includes a detailed pinout table, power distribution scheme (for 12V solenoid lock), and complete Arduino C++ source code.

一、 引脚分配与接线总表 / Pinout & Wiring Table

由于 Arduino UNO R3 引脚有限，RFID 复位 (RST) 接 Arduino 的 RESET 引脚，4x4 键盘使用前 3 列 (3x4 配置)，红灯与蜂鸣器共用信号引脚。

⚠️ 重要提示（程序上传注意事项）/ Important Precaution (Serial Uploading):
D0 (RX) 和 D1 (TX) 引脚参与串口通信。在向 Arduino 开发板上传程序时，请务必先拔掉 D0 和 D1 上的杜邦线；上传成功后再将杜邦线插回。
D0 (RX) and D1 (TX) are used for Serial communication. Disconnect wires on D0 and D1 when uploading code, then reconnect them after upload completes.


详细接线表 / Detailed Wiring Table

| 模块名称 / Component | 模块引脚 / Pin | 连接 Arduino UNO 引脚 / Arduino Pin | 备注说明 / Remarks |

| LCD 1602 (I2C) | GND | GND | 共地 / Common Ground |
|  | VCC | 5V | 电源正极 / 5V Power |

|  | SDA | A4 | I2C 数据线 / I2C Data Line |

|  | SCL | A5 | I2C 时钟线 / I2C Clock Line |


| RFID RC522 | VCC | 3.3V | ⚠️ 严禁接 5V！/ 3.3V ONLY! |
|  | RST | Arduino RESET | 接 Arduino 板载复位脚 / Hardware Reset Pin |

|  | GND | GND | 共地 / Common Ground |

|  | MISO | D12 | SPI MISO |

|  | MOSI | D11 | SPI MOSI |

|  | SCK | D13 | SPI SCK |

|  | SDA (SS) | D10 | SPI 片选 Pin / SPI Chip Select |


| 4x4 矩阵键盘 / Keypad | Row 1 (Pin 1) | D2 | 键盘第 1 行 / Keypad Row 1 |
|  | Row 2 (Pin 2) | D3 | 键盘第 2 行 / Keypad Row 2 |

|  | Row 3 (Pin 3) | D4 | 键盘第 3 行 / Keypad Row 3 |

|  | Row 4 (Pin 4) | D5 | 键盘第 4 行 / Keypad Row 4 |

|  | Col 1 (Pin 5) | D6 | 键盘第 1 列 (按键 1,4,7,*) / Col 1 |

|  | Col 2 (Pin 6) | D7 | 键盘第 2 列 (按键 2,5,8,0) / Col 2 |

|  | Col 3 (Pin 7) | D8 | 键盘第 3 列 (按键 3,6,9,#) / Col 3 |

|  | Col 4 (Pin 8) | 悬空 / NC | 仅用前 3 列即可输入所有数字与 * # / Unused |


| 超声波 HC-SR04 | VCC | 5V | 电源正极 / 5V Power |
|  | GND | GND | 共地 / Common Ground |

|  | Trig | A2 | 触发引脚 / Trigger Pin |

|  | Echo | A3 | 接收引脚 / Echo Pin |


| SG90 舵机 / Servo | 棕色/黑色 (GND) | GND | 地线 / Ground |
|  | 红色 (VCC) | 5V | 电源正极 / 5V Power |

|  | 橙色/黄色 (Signal) | A0 | PWM 控制线 / PWM Signal |

| 5V 继电器 / Relay | VCC | 5V | 继电器供电 / 5V Power |

|  | GND | GND | 共地 / Common Ground |

|  | IN (Signal) | A1 | 控制电磁锁通断 / Relay Signal |


| 室内按键 / Push Button | 引脚 1 / Pin 1 | D1 | 按键一端 / Button Pin 1 |
|  | 引脚 2 / Pin 2 | GND | 按键另一端（内置上拉） / Button Pin 2 |


| 蜂鸣器与红灯 / Buzzer & Red LED | Buzzer VCC + 红灯阳极(+) | D0 | 红灯串联 220Ω 电阻后与蜂鸣器并联接 D0 |
|  | Buzzer GND + 红灯阴极(-) | GND | 共地 / Common Ground |

| 绿色 LED / Green LED | 阳极 (+) | D9 | 串联 220Ω 电阻后接 D9 / Connect to D9 via 220Ω |

|  | 阴极 (-) | GND | 共地 / Common Ground |


二、 12V 电源与电磁锁接线 / Power & 12V Solenoid Lock Wiring
12V 电磁锁（Solenoid Lock）工作电流较大（1A~2A），绝不能直接使用 Arduino 的 5V 供电，必须使用 3 节 18650 电池或 12V 电源适配器。

1. 电磁锁高压回路 / Solenoid Lock High-Voltage Circuit:
18650 电池盒正极 (+11.1V~12.6V) ➔ 继电器 COM (公共端)
继电器 NO (常开端) ➔ 12V 电磁锁正极线
12V 电磁锁负极线 ➔ 18650 电池盒负极 (GND)

2. 共地（Common Ground，极度重要 / Crucial）:
18650 电池盒负极 (GND) 必须与 Arduino 的 GND 用杜邦线连接在一起！
18650 Battery Holder (-/GND) MUST be connected to Arduino GND.

三、 所需库文件 / Required Libraries
请在 Arduino IDE 中安装以下库文件 (Tools -> Manage Libraries)：
1. LiquidCrystal_I2C (by Frank de Brabander)
2. Keypad (by Mark Stanley, Alexander Brevig)
3. MFRC522 (by GitHubCommunity)
4. Servo (Arduino Built-in)

功能特征 / function feature ：

1. 密码门拥有双系统开门可以选择使用密码盘或感应卡功能，也可以选择室内开门
2. 这套感应功能拥有多卡，就代表说只要提前在代码里注册就可以用了
3. 当输入三次错误的密码后，buzzer 和 红色LED 他会响和亮 持续10s
4. 在buzzer 和 红色LED 我是选择parallel circuit 好处就是buzzer 不受resistor的影响，输出可以到达最大化
5. 当sensor 5s 内感应到没人，他就会自动关闭LCD


1. The keypad lock features a dual-access system, allowing entry via the keypad or a proximity card, as well as an indoor unlock option.
2. The proximity system supports multiple cards; once a card is registered in the code, it is ready for use.
3. If an incorrect password is entered three times, the buzzer sounds and the red LED lights up for 10 seconds.
4. For the buzzer and the red LED, I chose a parallel circuit; the advantage is that the buzzer is not affected by the resistor, allowing the output to be maximized.
5. The LCD automatically turns off if the sensor detects no presence for 5 seconds.

   
   
