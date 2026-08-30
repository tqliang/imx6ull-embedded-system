# i.MX6ULL 智能终端系统

基于 NXP i.MX6ULL（ARM Cortex-A7）+ Qt 5 + Buildroot 构建的嵌入式 Linux 智能终端系统，包含 7 个独立子应用，通过主页面统一管理，支持 MQTT 远程控制和 HTTP 仪表盘。

## 硬件平台

| 项目 | 规格 |
|---|---|
| SoC | NXP i.MX6ULL (ARM Cortex-A7, 800MHz) |
| 开发板 | 正点原子 ALIENTEK i.MX6ULL (EMMC) |
| 内存 | 512MB DDR3 |
| 存储 | 8GB EMMC |
| 屏幕 | 7 寸 800×480 LCD（linuxfb） |
| 触摸 | 电容触摸屏（多点触控） |
| 摄像头 | OV5640（MJPEG / YUYV） |
| 音频 | WM8960 编解码器 |
| 传感器 | MAG3110 磁力计 |

## 系统架构

```
┌─────────────────────────────────────────────────┐
│                    mainpage                     │
│  ┌─────────┐  ┌──────────┐  ┌────────────────┐  │
│  │ 应用卡片 │  │ 背光控制  │  │ 触摸手势识别      │  │
│  │ 启动器   │  │ Panel    │  │ (三指下滑)      │  │
│  └─────────┘  └──────────┘  └────────────────┘  │
│  ┌──────────────────────────────────────────┐   │
│  │          NetworkManager                  │   │
│  │  ┌──────────┐  ┌───────────────────────┐ │   │
│  │  │ MQTT 客户端│  │ HTTP 服务器 (8080)    │ │   │
│  │  │ (自实现)  │  │ 仪表盘 + MJPEG 推流     │ │   │
│  │  └──────────┘  └───────────────────────┘ │   │
│  └──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
         │ QProcess 启动子应用
         ▼
┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
│camera│ │audio │ │game  │ │file  │ │term  │ │net   │
│      │ │      │ │      │ │mgr   │ │inal  │ │work  │
└──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘
```

## 目录结构

```
app/
├── mainpage/              # 主页面（启动器 + MQTT + HTTP 服务器）
├── camera/                # 相机应用（V4L2 + MJPEG 录制）
├── audio/                 # 音乐播放器（ALSA + libmad MP3）
├── game/                  # 飞船射击游戏（自绘引擎 30fps）
├── filemanager/           # 文件管理器
├── terminal/              # 终端模拟器（QProcess 虚拟终端）
├── network/               # 网络参数设置
├── board/                 # 板级支持文件
│   ├── kernel/            # 自定义 DTS 和 defconfig
│   └── buildroot/         # Buildroot 配置文件
├── README.md
├── .gitignore
├── trans.sh               # 部署脚本
├── start_mainpage.sh      # 主页面启动脚本
├── start_camera.sh        # 相机启动脚本
└── start_audio.sh         # 音乐启动脚本
```

## 技术栈

| 类别 | 技术 | 说明 |
|---|---|---|
| 系统构建 | Buildroot | 交叉编译工具链 + rootfs |
| 内核 | Linux 4.1.15 (NXP i.MX) | 自定义裁剪 + DTB |
| UI 框架 | Qt 5 Widgets | linuxfb 平台插件，无 X11/Wayland |
| 构建工具 | qmake | 交叉编译 ARM 平台 |
| 网络协议 | 自实现 MQTT 3.1.1 | 手写协议栈，无第三方依赖 |
| HTTP 服务 | 自实现 HTTP Server | 内嵌仪表盘 + REST API |
| 视频采集 | V4L2 (Video4Linux2) | mmap 零拷贝抓帧 |
| 视频编码 | MJPEG | 摄像头直出 + 录制 |
| 视频推流 | MJPEG over HTTP | multipart/x-mixed-replace |
| 音频播放 | ALSA-lib + libmad | WAV/PCM 和 MP3 软解码 |
| 音频控制 | ALSA Mixer | WM8960 扬声器/耳机控制 |
| 图像优化 | ARM NEON SIMD | YUYV→RGB 加速转换 |
| 外设控制 | sysfs | 背光亮度（/sys/class/backlight） |
| 输入设备 | Linux input 子系统 | 触摸屏手势识别、磁力计 |

## 快速开始

### 环境要求

- **Buildroot 交叉编译工具链**：`arm-buildroot-linux-gnueabihf`
- **Qt 5**：通过 Buildroot 交叉编译至 ARM 平台
- **工具链路径**：`~/imx6ull_work/buildroot/output/host`

### 编译全部

```bash
cd app
for dir in mainpage camera audio game filemanager terminal network; do
    cd "$dir" && ./build.sh && cd ..
done
```

### 编译单个应用

```bash
cd app/camera
./build.sh
```

### 部署到目标板

```bash
sudo ./trans.sh
```

### 在目标板上运行

```bash
# 主页面（首先启动）
/usr/bin/mainpage -platform linuxfb

# 或者使用启动脚本
./start_mainpage.sh
```

## 子应用说明

### 1. mainpage（主页面）— 核心应用

主启动器，整个系统的入口，负责：

- **应用管理**：6 个自定义绘制卡片（`AppCard`，纯 QPainter 绘制图标），通过 `QProcess` 管理子应用进程生命周期
- **时钟显示**：10 秒刷新间隔
- **网络状态指示**：MQTT 连接状态实时显示（在线/离线）
- **背光控制**：`BacklightPanel` 独立模块，滑入/滑出动画（250ms 缓动），3 秒自动隐藏
- **触摸手势**：`TouchGestureDetector` 独立模块，直接读取 `/dev/input/event*` 原始事件，三指下滑调出背光面板
- **MQTT 客户端**：自实现 MQTT 3.1.1 协议栈，支持 CONNECT/SUBSCRIBE/PUBLISH/PINGREQ，断线自动重连（5 秒间隔）
- **HTTP 服务器**：内嵌仪表盘网页 + 12 个 REST API 端点 + MJPEG 视频推流
- **设备状态上报**：每 10 秒自动上报 CPU 温度、亮度、应用状态、推流状态

### 2. camera（相机）

- V4L2 摄像头采集（MJPEG / YUYV 双格式支持），`mmap` 零拷贝取帧
- NEON 指令集加速 YUYV→RGB 转换
- 实时预览（FPS 显示）
- 拍照保存为 JPEG（`/home/user/photo_YYYYMMDD_HHMMSS.jpg`）
- 图片画廊浏览（`GalleryDialog`）
- MJPEG 视频录制（`.mjpeg` 格式）
- 亮度/对比度调节（V4L2 `V4L2_CID_BRIGHTNESS`/`V4L2_CID_CONTRAST`）

### 3. audio（音乐播放器）

- WAV（PCM 16bit）和 MP3 音频播放
- 基于 libmad 的 MP3 软解码（定点运算，适合 ARM 无 FPU 场景）
- ALSA PCM 播放（`snd_pcm_writei`，双缓冲机制）
- ALSA Mixer 音量控制：扬声器、耳机独立音量
- 频谱可视化（`SpectrumWidget`，8 条频段柱状图）
- 播放列表管理（文件对话框选择）

### 4. game（飞船射击）

- 2D 太空飞船射击游戏，30fps 定时器驱动
- QPainter 自绘引擎：
  - 星云背景 + 2 层视差滚动星空 + 闪烁星
  - 精密飞船模型（翼尖、驾驶舱光晕、引擎辉光）
  - 陨石坑细节 + 浮动得分弹出
- 三连发子弹射击 + 爆炸粒子特效
- 背景缓存优化（QPixmap 预渲染，消除每帧重复绘制）
- 屏幕震动反馈（撞击时 `QPoint` 偏移）
- 最高分持久化保存（JSON 文件）
- 难度递增（每 500 分升一级，陨石速度/数量增加）
- 暂停功能（P 键）
- 键盘/触摸控制，MAG3110 磁力计传感器控制（倾斜机身操控飞船）

### 5. filemanager（文件管理器）

- 文件浏览（目录/文件列表，`QListWidget` 渲染）
- 上级目录导航（`QDir::cdUp`）
- 文件大小和修改时间显示
- 目录优先排序
- 刷新功能

### 6. terminal（终端模拟器）

- 基于 `QProcess` 的虚拟终端，执行 `/bin/sh`
- 独立 `stdout`/`stderr` 通道，实时输出到 `QPlainTextEdit`
- 自定义软键盘（`SoftKeyboard`，支持字母/数字/符号）
- 清屏功能
- 键入历史（通过 `QLineEdit`）

### 7. network（网络设置）

- MQTT 服务器地址和端口配置
- 设备名称设置
- WiFi 开关
- 虚拟键盘输入（140px 紧凑布局，设置内容可滚动，不溢出 800×480 屏幕）
- 配置持久化（`SettingsManager`，JSON 格式，与主页面共享 `settings.json`）

## MQTT 远程控制

### 通信架构

```
  MQTT Broker (172.24.184.154:1883)
       │
       ├── 订阅: imx6ull/{设备名}/status
       │
       └── 发布: imx6ull/{设备名}/cmd
                    │
                    ▼
              i.MX6ULL 设备
```

### 支持的命令

发布 JSON 到 topic `imx6ull/{设备名}/cmd`：

| 命令 | 参数 | 功能 |
|---|---|---|
| `set_brightness` | `{"level": 0-7}` | 设置背光亮度 |
| `capture` | 无 | 打开相机 |
| `launch_app` | `{"name": "camera/audio/game/filemanager/terminal/network"}` | 启动指定应用 |
| `stop_app` | 无 | 停止当前应用 |
| `start_stream` | 无 | 开启摄像头推流 |
| `stop_stream` | 无 | 停止摄像头推流 |
| `get_status` | 无 | 立即上报设备状态 |
| `reboot` | 无 | 重启设备 |
| `shutdown` | 无 | 关机 |

### 使用 MQTTX 测试

1. 新建连接，填入 MQTT 服务器地址和端口
2. 订阅 `imx6ull/IMX6ULL-001/status` 查看设备状态上报
3. 发布到 `imx6ull/IMX6ULL-001/cmd` 发送命令

| 操作 | Payload |
|---|---|
| 设置亮度 | `{"level":5}` |
| 启动相机 | `{"name":"camera"}` |
| 停止应用 | `{}` |
| 开启推流 | `{}` |

## HTTP 仪表盘

内置 HTTP 服务器（端口 8080），打开浏览器访问 `http://设备IP:8080` 即可使用。

### 仪表盘功能

- **毛玻璃风格卡片布局**，响应式设计（手机/桌面自适应）
- **设备状态总览**：亮度、CPU 温度、推流状态、应用运行状态
- **亮度滑块调节**（拖动实时生效，JavaScript fetch API）
- **6 个应用一键启动按钮**（相机/音乐/游戏/文件/终端/设置）
- **应用停止按钮**
- **MJPEG 实时视频流显示**（`<img src="/stream">`）
- **推流开关控制**（自动检测推流状态）
- **系统重启/关机确认按钮**（带 confirm 弹窗）
- **Toast 通知反馈**

### API 端点

| 端点 | 方法 | 说明 |
|---|---|---|
| `/` | GET | 仪表盘 HTML 页面 |
| `/stream` | GET | MJPEG 实时视频流 |
| `/api/status` | GET | 设备状态 JSON（device/brightness/cpuTemp/streaming/appRunning） |
| `/api/brightness/up` | GET | 亮度 +1 |
| `/api/brightness/down` | GET | 亮度 -1 |
| `/api/brightness/set?level=N` | GET | 设置亮度（0-7） |
| `/api/start_stream` | GET | 开启摄像头推流 |
| `/api/stop_stream` | GET | 停止摄像头推流 |
| `/api/app/launch?name=xxx` | GET | 启动指定应用 |
| `/api/app/stop` | GET | 停止当前应用 |
| `/api/reboot` | GET | 重启设备 |
| `/api/shutdown` | GET | 关机 |

## 设备状态上报

设备每 10 秒自动上报状态到 topic `imx6ull/{设备名}/status`：

```json
{
  "brightness": 7,
  "appRunning": false,
  "streaming": false,
  "cpuTemp": 42
}
```

## 配置文件

配置文件路径：`~/.config/SmartDevice/settings.json`

```json
{
  "mqttHost": "172.24.184.154",
  "mqttPort": 1883,
  "deviceName": "IMX6ULL-001",
  "brightness": 7,
  "wifiEnabled": false
}
```

## 触摸手势

| 手势 | 功能 |
|---|---|
| 三指下滑 | 调出背光控制面板（3 秒自动隐藏） |

## 游戏操作

| 操作 | 功能 |
|---|---|
| ← → 方向键 | 左右移动飞船 |
| 空格 | 射击（三连发） |
| P | 暂停/继续 |
| R | 游戏结束后重新开始 |
| ESC | 退出游戏 |
| 触摸屏 | 按住移动飞船 + 自动射击 |
| 磁力计 | 倾斜开发板控制飞船左右移动 |

## 硬件接口

| 接口 | 路径 | 说明 |
|---|---|---|
| 背光控制 | `/sys/class/backlight/backlight/brightness` | 8 级亮度（0-7） |
| 触摸设备 | `/dev/input/event*` | 自动探测多点触控设备 |
| 摄像头 | `/dev/video0` | V4L2 接口 |
| 音频 | ALSA (default) | WM8960 编解码器 |
| CPU 温度 | `/sys/class/thermal/thermal_zone0/temp` | 单位：毫摄氏度 |
| 磁力计 | `/dev/input/event*` | MAG3110 传感器 |

## 板级支持（Board Support Package）

### 内核配置

- **内核版本**：Linux 4.1.15 (NXP i.MX)
- **源码**：https://github.com/nxp-imx/linux-imx (branch imx_4.1.15_2.0.0_ga)
- **自定义 DTS**：`board/kernel/imx6ull-alientek-emmc.dts`
- **自定义 defconfig**：`board/kernel/imx_alientek_emmc_defconfig`

### 内核编译

```bash
cd linux-imx-4.1.15
make ARCH=arm CROSS_COMPILE=arm-buildroot-linux-gnueabihf- imx_alientek_emmc_defconfig
make ARCH=arm CROSS_COMPILE=arm-buildroot-linux-gnueabihf- zImage dtbs -j$(nproc)
```

### Buildroot 配置

Buildroot 配置文件位于 `board/buildroot/` 目录，包含 Qt 5、ALSA、libmad 等必要组件的配置。

## 关键设计决策

### 为什么用 linuxfb 而不是 X11/Wayland？

i.MX6ULL 资源有限（512MB RAM），无 GPU，linuxfb 直接写入 framebuffer，零开销，启动快，适合单一全屏应用场景。

### 为什么手写 MQTT 协议栈？

- 避免引入 paho-mqtt 等大型库的交叉编译依赖
- 对 MQTT 3.1.1 协议有深入理解
- 代码量小（约 200 行），可控性强

### 为什么不使用 GStreamer/FFmpeg？

- 交叉编译 FFmpeg 到 ARM 平台依赖复杂
- V4L2 直接抓取 MJPEG 帧，不需要转码
- 系统资源受限，避免引入重量级框架

## 许可证

MIT License
