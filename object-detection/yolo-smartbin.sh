#!/bin/bash
export SMARTBIN=/home/smartbin/smartbin-esp32/object-detection/
cd $SMARTBIN
python3 yolo-smartbin.py -s video -j config.json -d false

