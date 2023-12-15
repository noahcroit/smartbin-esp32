#/bin/python
# import required module
import os
import time

# play sound
file = "welcome-please-login.mp3"
print('playing sound using native player')
time.sleep(60)
while True:
    os.system("mpg123 " + "/home/noah/Workspace/ssa-smartbin-nida/smartbin-esp32/db/sound/" + file)
    time.sleep(60)

