# smartbin-esp32
smartbin prototyping with esp32 as sorting mechanical controller, Interface wtih YOLO linux machine

## Bin's Mechanical Controller
ESP32 is used for microcontroller, in folder bin-mechanic-esp32. ESP-IDF is used for development environment.
To build and flash the firmware,
```
$ cd bin-mechanic-esp32

# To setup WIFI & MQTT Broker
$ idf.py menuconfig

# To build & flash
$ idf.py build
$ idf.py flash -p /dev/ttyUSB0
```

Pinout for sensors \
VL53L0X distance sensor \
- SDA -> D21 \
- SCL -> D22 \
HX711 weight sensor \
- Data -> D33 \
- CLK  -> D32 \

## YOLO worker for garbage classifier
Pre-built OpenCV-python can be used, Run on Ubuntu server.
```
$ sudo apt-get install python3-opencv
```

To run worker
```
$ cd object-detection
$ python yolo-smartbin.py -s cam -c config.json -d true
```
Worker will look for a trained YOLOv4 model file (.cfg) from Darknet framework (https://github.com/AlexeyAB/darknet).
The config.json file is used for select the camera url, redis tag, db etc.

Testing camera with ffmpeg or cheese
```
$ ffmpeg -f v4l2 -framerate 30 -video_size 1280x720 -input_format mjpeg -i /dev/v4l/by-id/your-camera-id out.mkv
```

### face recognition for smartbin login
Dlib and face_recognition python package are used in the yolo-smartbin.py for face login. 
Follow the instruction in here https://gist.github.com/MikeTrizna/4964278bb6378de72ba4b195553a3954
Need to build dlib in no CUDA mode when you want to use only CPU. By set -DDLIB_USE_CUDA=0 in CMake process.
```
$ git clone https://github.com/davisking/dlib.git
$ cd dlib
$ mkdir build
$ cd build
$ cmake .. -DDLIB_USE_CUDA=0 -DUSE_AVX_INSTRUCTIONS=1
$ cmake --build .
$ cd ..
$ python setup.py install --set DLIB_USE_CUDA=0
$ pip3 install face-recognition
```

### play sound in smartbin
mpg123 player is used to play the sample speeches in form of mp3 files. The player is called inside YOLO worker in native way (os module).
To install mpg123 player,
```
$ sudo apt-get update
$ sudo apt-get install mpg123
```
To test player,
```
$ mpg123 -o alsa:hw:1,0 -f 30000
```
-o => audio output target
-f => scale of volume control (0 - 32768)

### Data for smartbin
The user face, .json for user info and .mp3 files are stored in `db` folder. To use this project, create `db` folder first and put the data like this \
db                                                          \
|                                                           \
|--/face                                                    \
|    |- User face images                                    \
|                                                           \
|--/sound                                                   \
|    |- .mp3 files                                          \
|                                                           \
|-- img_coupon.jpg                                          \
|                                                           \
|-- userinfo.json files                                     \

The example format for userinfo.json format can be found in `object-detection/user.json` 


### Startup script
Use crontab to run startup script smartbin-startup.sh.
```
$ crontab -e
```
Then, put this line after run "crontab -e" command.
@reboot /bin/bash /home/smartbin/smartbin-esp32/smartbin-startup.sh


### Web GUI for Smartbin Kiosk
UI components will not be in this repository. Only software dependencies for this Kiosk.
Dependencies consist of
- nodejs
- npm
- redis server
```
$ sudo apt install nodejs npm
$ sudo apt install redis-server
$ sudo systemctl status redis-server
```



