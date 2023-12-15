#!/bin/bash
sleep 5
echo "setup mp3 player environment"
source /home/noah/venv/mp3player-test/bin/activate
echo "set python environment : finished"
cd /home/noah/Workspace/ssa-smartbin-nida/smartbin-esp32/mp3-player-test
python mp3-native-os.py >> /home/noah/log.txt 2>&1
