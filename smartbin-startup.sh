#!/bin/bash
echo "setup ngrok for ssh"
/snap/bin/ngrok tcp 22 &
echo "setup redis-server"
cd /home/smartbin/redis-server
node server.js &
echo "setup kiosk"
cd /home/smartbin/kioskface
npm start &
sleep 5
echo "setup smartbin worker"
cd /home/smartbin/smartbin-esp32/object-detection
/bin/python3 yolo-smartbin.py -s cam -c config.json -d false
