
export TSLIB_TSDEVICE=/dev/input/event1    
ts_calibrate
export TSLIB_TSDEVICE=/dev/input/event1  
export QT_QPA_FB_TSLIB=1
./audio -platform linuxfb 
