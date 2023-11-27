#!/bin/bash
sleep 10
echo "setup ngrok for ssh"
/snap/bin/ngrok tcp 22 &
sleep 10
echo "setup redis-server"
cd /home/smartbin/redis-server
node server.js &
echo "setup kiosk"
cd /home/smartbin/kioskface
npm start &
sleep 60
echo "setup smartbin worker"
cd /home/smartbin/smartbin-esp32/object-detection
/bin/python3 yolo-smartbin.py -s cam -c config.json -d false >> /home/smartbin/log.txt 2>&1
