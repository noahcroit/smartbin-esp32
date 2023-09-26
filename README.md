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
Pre-built OpenCV-python can be used, Run on Ubuntu server.
```
sudo apt-get install python3-opencv
```

To run worker
```
cd object-detection
python yolo-smartbin.py -s cam -c config.json -d true
```
Worker will look for a trained YOLOv4 model file (.cfg) from Darknet framework (https://github.com/AlexeyAB/darknet).
The config.json file is used for select the camera url, redis tag, db etc.

Testing camera with ffmpeg or cheese
```
ffmpeg -f v4l2 -framerate 30 -video_size 1280x720 -input_format mjpeg -i /dev/v4l/by-id/your-camera-id out.mkv
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
playsound module is used to play the sample speech.
```
pip3 install playsound
```






