
modprobe ov5640_camera_mipi 2>/dev/null && echo "ov5640_camera_mipi loaded" || echo "ov5640_camera_mipi failed"
modprobe ov5640_camera 2>/dev/null && echo "ov5640_camera loaded" || echo "ov5640_camera failed"
modprobe mx6s_capture 2>/dev/null && echo "mx6s_capture loaded" || echo "mx6s_capture failed"
ls /dev/i2c* 2>/dev/null || echo "No /dev/i2c* found"
ls -la /dev/video* 2>/dev/null || echo "No /dev/video* found"
dmesg | grep -i -E "ov5640|camera ov|mx6s-csi|mipi csi|capture" 2>/dev/null || echo "No camera messages"
lsmod 2>/dev/null | grep -E "ov5640|mx6s|capture|csi" || echo "No camera modules"

export TSLIB_TSDEVICE=/dev/input/event1
ts_calibrate
export TSLIB_TSDEVICE=/dev/input/event1
export QT_QPA_FB_TSLIB=1
./camera -platform linuxfb
