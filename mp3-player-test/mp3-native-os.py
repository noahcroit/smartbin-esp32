#/bin/python
# import required module
import os
import time

# play sound
filepath = "/home/smartbin/smartbin-esp32/db/sound/facelogin.mp3"
player_cmd = "mpg123 -o alsa:hw:1,0 -f 30000"
print("Sound will be played after every 1 minute.")
time.sleep(60)
while True:
    print('playing sound using native player')
    os.system(player_cmd + " " + filepath)
    time.sleep(60)

