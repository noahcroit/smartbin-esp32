# smartbin-esp32
smartbin prototyping with esp32 as sorting mechanical controller, Interface wtih YOLO linux machine

## Bin's Mechanical Controller
ESP32 is used for microcontroller, in folder bin-mechanic-esp32. ESP-IDF is used for development environment.
To build and flash the firmware,
```
cd bin-mechanic-esp32

# To setup WIFI & MQTT Broker
idf.py menuconfig

# To build & flash
idf.py build
idf.py flash -p /dev/ttyUSB0
```

## YOLO worker for garbage classifier
Use OpenCV-python, Run on Ubuntu server. To run this
```
cd object-detection
python yolo-smartbin.py -s cam -j config.json -d true
```



