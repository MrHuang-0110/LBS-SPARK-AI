# Spark AI — STM32F103 教育机器人中枢固件

[![Platform](https://img.shields.io/badge/platform-STM32F103-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103.html)
[![Framework](https://img.shields.io/badge/framework-Bare--metal%20HAL-green)](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)
[![Python](https://img.shields.io/badge/embedded-PikaPython%201.13.4-orange)](https://github.com/pikastech/pikapython)

基于 STM32F103（正点原子 MiniSTM32 V4）的 **Spark AI 教育机器人中枢固件**。裸机 C（无 RTOS），内置 **PikaPython** 解释器，用户通过 USB/蓝牙推送 Python 脚本即可驱动 4 个热插拔传感器端口上的电机与传感器。LED 点阵作为交互界面。

---

## 功能与特性

### 🤖 Python 脚本执行

- 固件内置 **PikaPython v1.13.4** 解释器，用户编写 `.py` 脚本存入 Flash 文件系统即可运行
- 支持 **Pauto 自动演示模式**：根据传感器类型自动切换避障/巡线/跟随行为
- 脚本通过 USB CDC 或蓝牙 BLE 下发，亦可通过板载按键选择运行

### 🔌 4 端口热插拔传感器中枢

| 端口 | 传感器类型 | 设备 ID | 说明 |
|------|-----------|---------|------|
| 0–3 | 颜色传感器 | `0xA2` | 光强检测、阈值校准 |
| 0–3 | 超声波传感器 | `0xA3` | 距离测量 (cm) |
| 0–3 | 触摸传感器 | `0xA4` | 触碰状态检测 |

- **自动识别**：插拔传感器自动检测并绑定，无需手动配置
- **优先级规则**：每组 (端口 0/1 为组 0，端口 2/3 为组 1) 中超声波优先，同组内触摸/颜色被抑制
- 电机配对：组 0 → 电机 4/5，组 1 → 电机 6/7

### 📡 双通道通信（USB CDC + 蓝牙 BLE）

| 通道 | 物理接口 | 用途 |
|------|---------|------|
| USB CDC | USB 虚拟串口 | 指令下发、脚本传输、监控上报、OTA 固件升级 |
| 蓝牙 BLE | UART5 (115200 bps) | 指令下发、脚本传输、监控上报、OTA 固件升级 |

- 统一协议帧格式（`0x5A` 帧头，CRC 校验）
- 支持 `0xB6`/`0xB9` 进入/退出 Python 模式
- 支持 `0xBE`/`0xBA` 禁用/启用监控上报
- 支持 `0xDA`/`0xBB`/`0xBC` OTA 固件更新
- 支持 `0xC1` 蓝牙遥控数据

### 📊 实时监控上报（JSON）

通过 USB CDC（每轮主循环）和蓝牙 BLE（每 25ms）输出 JSON 格式的设备状态，字段包括：

```json
{
  "deviceList": [
    { "port": 0, "ultrasion": { "cm": "45" } },
    { "port": 1, "touch": { "state": 0 } },
    { "port": 2, "color": { "lux": "120", "state": "1", "min": "0", "max": "255", "threadValue": "128" } }
  ],
  "flash": { "total": "1024 kb", "free": "512 kb" },
  "adc": { "bat": "85%" },
  "version": 1,
  "heap": "45",
  "WillAiState": "stop"
}
```

- **脚本运行时**：由 btim 定时器中断驱动的 `monitor_send_usb`（1ms）/ `monitor_send_blue`（50ms）接管发送，避免主循环阻塞导致监控断流
- **空闲时**：由主循环直接发送，二者互斥不重复

### 🔄 OTA 固件升级

- 支持通过 **USB CDC** 和 **蓝牙 BLE** 双通道进行固件升级
- 升级流程：`0xDA`（开始）→ 文件数据传输 → `0xBB`（完成）/ `0xBC`（完成并自动运行 Python）
- 升级期间自动关闭监控上报，完成后自动恢复
- 应用向量表位于 `0x08010000`（64KB bootloader 偏移），支持 IAP 在线升级

### 🎮 板载交互

| 操作 | 功能 |
|------|------|
| 短按 KEY1 | 切换 UI 菜单项 / 停止脚本 |
| 长按 KEY1 (1.5s) | 保存时间到 Flash → 关机动画 → 关机 |
| 长按持续 (每 500ms) | 蜂鸣器反馈 |

- **LED 点阵 (TM1640)**：滚动显示 UI 菜单项（0.o–9.o），显示传感器连接状态
- 开机/关机动画过渡
- 蓝牙连接状态 LED 指示

### ⚡ 电源管理

- 电池电压 ADC 采样 + 去抖动滤波
- 电量百分比计算（临界低压 → 满电高值）
- 电池状态实时上报至监控 JSON

### 🗄️ 文件系统 (FATFS)

- 内部 Flash 上的 FATFS 文件系统
- 存储 `.py` 脚本、`blue_cfg.cfg`（蓝牙配置）、`remote_cfg.cfg`（遥控偏移校准）、`version.txt`（版本号）
- 支持文件创建、读取、删除

---

## Python API 参考

固件向 Python 脚本暴露以下模块（通过 `.pyi` 桩生成的 C 绑定）：

### `_motor` — 电机控制

```python
_motor.run_power(port, duty)          # 指定端口电机以 duty% 功率运行
_motor.stop(port)                     # 停止指定电机
_motor.run_for_power_seconds(port, duty, degrees)  # 定时/定角度运行
_motor.stop_module(port, stop)        # 设置电机停止模式
_motor.pair(port1, port2, state)      # 配对两个电机
_motor.mov_power(duty1, duty2)        # 移动平台双轮功率控制
_motor.mov_dir_power(dir, duty)       # 移动平台方向控制
_motor.mov_stop()                     # 移动平台停止
_motor.mov_set_stop_module(mode)      # 移动平台停止模式
_motor.mov_find_line_init()           # 巡线初始化
_motor.mov_find_line_run(gray_left, gray_right, power1, power2, kp, kd)  # PID 巡线
_motor.mov_set_advance_offset(offset1, offset2)    # 前进偏移校准
_motor.mov_set_retreat_offset(offset1, offset2)    # 后退偏移校准
```

### `_ultrasion` — 超声波传感器

```python
_ultrasion.value(port)                  # 读取距离 (cm)，255 = 超时
_ultrasion.cmp_value(port, judgment, value)  # 比较判断 (">", "<", "==", "!=")
```

### `_color` — 颜色传感器

```python
_color.lux(port)                        # 读取光强值 (0–255)
_color.lux_state(port)                  # 读取光强状态
_color.cmp_lux(port, judgment, value)   # 比较判断
_color.one_calibrate(port, timers)      # 单端口校准
_color.two_calibrate(port1, port2, timers)  # 双端口同步校准
_color.set_color_threshold_value(port, value)  # 设置阈值
```

### `_touch` — 触摸传感器

```python
_touch.state(port)                      # 读取触摸状态 (0/1)
```

### `_key` — 按键与遥控

```python
_key.key_mast(keyname, state)           # 读取板载按键状态 ("left"/"right", 0/1)
_key.key_remote(keys, coor)             # 蓝牙遥控数据
_key.read_adcance_left_offset()         # 读取前进左偏移
_key.read_advance_right_offset()        # 读取前进右偏移
_key.read_retreat_left_offset()         # 读取后退左偏移
_key.read_retreat_right_offset()        # 读取后退右偏移
```

### `_matrix` — LED 点阵

```python
_matrix.set_pixel(x, y)                 # 点亮像素
_matrix.set_pixel_brightness(x, y, brightness)  # 设置像素亮度
_matrix.set_brightness(brightness)      # 全局亮度
_matrix.show(buf1–buf7)                 # 显示自定义图案 (7 行 × 5 列)
_matrix.show_roll(text)                 # 滚动显示文字
_matrix.clear()                         # 清屏
```

### `_beep` — 蜂鸣器

```python
_beep.play_muic(feq, ms)               # 播放指定频率/时长音调
```

### `_os` — 系统

```python
_os.sleep_s(tick)                       # 延时 (秒)
_os.timer()                             # 获取运行时间
_os.resetTimer()                        # 重置计时器
_os.stop_exit()                         # 退出脚本
_os.get_port_linke(port)                # 获取端口连接的设备 ID (0xA2/0xA3/0xA4/0)
```

---

## 项目结构

```
LBS-SPARK-AI/
├── Users/                     # 主程序入口 (main.c, main.h)，事件循环，Pauto 演示
├── application/               # 外设驱动
│   ├── motor/                 # 电机驱动 (PWM)
│   ├── ultrasion/             # 超声波传感器
│   ├── color/                 # 颜色传感器
│   ├── touch/                 # 触摸传感器
│   ├── matrix/                # LED 点阵 (TM1640) + UI 管理器 + 动画
│   ├── blue/                  # 蓝牙 BLE 模块 (UART5 AT 指令)
│   └── beep/                  # 蜂鸣器
├── Middlewares/               # 中间件
│   ├── monitor/               # JSON 监控上报 (USB + BLE 双通道)
│   ├── protocol/              # 协议帧解析 (_AGREEMENT)
│   ├── event_manager/         # 事件管理器 (定时器 ISR 驱动)
│   ├── deviceIdentify/        # 热插拔设备识别与绑定
│   ├── lbs_file_manager/      # Python 脚本加载 + OTA 文件处理
│   ├── file_manager/          # 文件操作封装
│   ├── fatfs/                 # FATFS 文件系统
│   ├── w25q80x/               # SPI Flash 驱动
│   ├── malloc/                # 内存管理 (mymalloc/myfree)
│   ├── RTC/                   # 实时时钟
│   ├── bat_manager/           # 电池管理
│   └── usb/                   # STM32 USB Device Library (CDC)
├── python/                    # PikaPython 集成
│   ├── *.pyi                  # C↔Python 绑定桩 (API 定义)
│   ├── main.py                # 导入清单
│   ├── pikaPackage.exe        # 绑定代码生成器
│   ├── pikascript-api/        # 生成的绑定代码 (__pikaBinding.c)
│   ├── pikascript-core/       # PikaPython 解释器内核
│   └── pikascript-lib/        # 标准库 (PikaStdLib, math, random, ExternLib)
├── Drivers/                   # 硬件驱动
│   ├── BSP/                   # 板级支持包 (ADC, IIC, KEY, LED, SPI, TIMER, WDG)
│   ├── STM32F1xx_HAL_Driver/  # STM32 HAL 库
│   ├── CMSIS/                 # ARM CMSIS
│   └── SYSTEM/                # 系统组件 (delay, sys, usart)
├── Projects/MDK-ARM/          # Keil µVision 工程文件
├── Output/                    # 构建产物 (被 .gitignore 排除)
└── postbuild.bat / keilkill.bat  # 构建后脚本
```

---

## 通信协议

### 帧格式

| 字节 | 字段 | 说明 |
|------|------|------|
| 0 | Head | `0x5A` 帧头 |
| 1 | sID | 源 ID (`0x97`) |
| 2 | oID | 目标 ID (`0x98`) |
| 3–4 | length | 数据长度 |
| 5 | index | 指令类型 |
| 6–N | data | 数据载荷 |
| N+1 | crc | 校验和 |
| N+2 | tard | `0xA5` 帧尾 |

### 指令索引

| Index | 功能 | 说明 |
|-------|------|------|
| `0xB6` | 进入 Python 模式 | 设置 `start_py = true`，触发按键短按事件 |
| `0xB9` | 退出 Python 模式 | 与 `0xB6` 相同逻辑（toggle） |
| `0xBE` | 禁用监控上报 | 关闭 `monitor_event` + USB/BLE 发送 |
| `0xBA` | 启用监控上报 | 恢复监控上报 |
| `0xDA` | OTA 开始 | 文件名 + 关闭监控 |
| `0xBB` | OTA 完成 | 文件收尾 + 恢复监控 |
| `0xBC` | OTA 完成并运行 | 同 `0xBB` + 自动执行 Python |
| `0x6F` | 删除 updata.txt | 强制 IWDG 复位 |
| `0xC1` | 蓝牙遥控 | 遥控器数据 |
| `0xC3` | 查询存储使用 | 保留 |
| `0xC4` | 查询文件列表 | 保留 |

---

## 构建与烧录

### 环境要求

- **Keil µVision 5 (MDK-ARM)** — 唯一支持的构建环境
- STM32F1xx 设备包
- ST-Link / J-Link (SWD 模式，JTAG 已禁用)

### 构建步骤

1. 打开 `Projects/MDK-ARM/atk_f103.uvprojx`
2. 选择目标 `atk_f103`
3. 构建 (F7) → 输出 `Output/atk_f103.bin`
4. `postbuild.bat` 自动将 `.bin` 复制到下载工具目录

> ⚠️ **注意**：`postbuild.bat` 中的路径为硬编码绝对路径，如果仓库位置变更，需手动修正，否则下载工具会烧录旧版本固件。

### 修改 Python API 后的步骤

如果修改了 `python/_*.pyi` 桩文件或添加了新模块：

1. 在 `python/` 目录下运行 `pikaPackage.exe` 重新生成绑定代码
2. 将生成的 `pikascript-api/__pikaBinding.c` 提交到仓库
3. Keil 会编译新的绑定代码

---

## 内存与约束

| 项目 | 说明 |
|------|------|
| 应用起始地址 | `0x08010000`（64KB bootloader 保留） |
| 堆分配 | `mymalloc(SRAMIN, ...)` / `myfree`，禁止使用标准 `malloc` |
| 调度模型 | 协作式事件循环（无 RTOS，无抢占） |
| 看门狗 | IWDG，10ms 喂狗，超时 ≈ 1s |
| 调试接口 | SWD（JTAG 已禁用） |

---

## 架构设计

```
┌─────────────────────────────────────────────────────┐
│                    main() 事件循环                    │
│  ui_manager → Main_Loop_Process → OTA → 监控 → 5ms  │
├─────────────────────────────────────────────────────┤
│  事件管理器 (btim ISR 驱动)                          │
│  iwdg_feed │ scan_adc │ key_scan │ monitor_event     │
│  usb_monitor_send │ blue_monitor_send │ matrix       │
├─────────────────────────────────────────────────────┤
│  Python 运行时 (PikaPython VM)                       │
│  _motor._ultrasion._color._touch._key._matrix._beep │
├─────────────────────────────────────────────────────┤
│  设备识别层 (deviceIdentify)                         │
│  4 端口自动检测 → 绑定传感器/电机                     │
├─────────────────────────────────────────────────────┤
│  通信层                                              │
│  USB CDC ←→ busDataparsing ←→ 协议帧                 │
│  BLE UART5 ←→ busDataparsing ←→ 协议帧               │
├─────────────────────────────────────────────────────┤
│  存储层                                              │
│  FATFS (内部 Flash) │ W25Q80x (SPI Flash)            │
│  .py 脚本 │ .cfg 配置 │ version.txt                  │
└─────────────────────────────────────────────────────┘
```

### 关键设计决策

- **脚本运行时监控双通道**：主循环阻塞时，btim ISR 事件驱动的 `monitor_send_usb`/`monitor_send_blue` 接管发送，确保 USB/蓝牙监控不断流
- **BLE 非阻塞发送**：`blue_send_it()` 使用 IT 中断发送，`blue_printf()` 使用阻塞轮询（仅主循环使用）
- **OTA 互斥**：OTA 期间关闭监控上报，避免 JSON 数据与文件数据交错
- **传感器优先级**：超声波 > 触摸/颜色，每组只有一个传感器生效

---

## License

本项目为教育用途，基于正点原子 MiniSTM32 V4 开发板。

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)