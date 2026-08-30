#!/bin/sh
# 加载摄像头模块
modprobe ov5640_camera_mipi 2>/dev/null
modprobe ov5640_camera 2>/dev/null
modprobe mx6s_capture 2>/dev/null

# 触摸屏校准
export TSLIB_TSDEVICE=/dev/input/event1
ts_calibrate
export TSLIB_TSDEVICE=/dev/input/event1
export QT_QPA_FB_TSLIB=1

# 启动主页
mainpage -platform linuxfb