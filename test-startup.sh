#!/bin/bash
sleep 5
#echo "setup mp3 player environment"
#source /home/noah/venv/mp3player-test/bin/activate
#echo "set python environment : finished"
cd /home/smartbin/smartbin-esp32/mp3-player-test
python3 mp3-native-os.py >> /home/smartbin/log.txt 2>&1
