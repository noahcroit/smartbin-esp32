import threading
import queue
import time
import cv2
import numpy as np
import argparse
import json
import redis
import base64
import paho.mqtt.client as mqtt



# Global Variables for Threads
worker_isrun = False
snapshot_isrun = False
frame_yolo = None
lock = threading.Lock()



# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    global snapshot_isrun

    print(msg.topic+" "+str(msg.payload))
    if msg.topic == "YOLO/Request?":
        if str(msg.payload) == "b\'Y\'":
            print("Requested!")
            snapshot_isrun = True
    else:
        print("Topic is not in the list of smartbin")

def task_mqttsub():
    global worker_isrun
    global snapshot_isrun

    broker="127.0.0.1"
    port=1883
    client = mqtt.Client()
    client.on_connect=on_connect
    client.on_message=on_message
    client.connect(broker, port, 60)
    client.subscribe("YOLO/Request?")
    while worker_isrun:
        print("Run MQTT sub")
        client.loop_forever()



def task_snapshot(queue_snapshot, source, source_type):
    global worker_isrun
    global snapshot_isrun

    while worker_isrun:
        if snapshot_isrun:
            stream = cv2.VideoCapture(source)
            print("start capture")
            try:
                # looping
                skip_frame=0
                while stream.isOpened() and snapshot_isrun:
                    ret, frame = stream.read()
                    if not frame is None:
                        skip_frame += 1
                        if skip_frame>=30:
                            queue_snapshot.put(frame) # put frame into queue
                            skip_frame=0
                    if source_type == 'video':
                        time.sleep(0.1) # slow down the snapshot in video mode to prevent memory usage creep
                stream.release()
                snapshot_isrun = False
            except:
                print("something wrong in task snapshot!")
                stream.release()
                snapshot_isrun = False
            snapshot_isrun = False

        time.sleep(1)

def task_overlay(queue_roi, displayflag):
    global snapshot_isrun
    global frame_yolo

    # looping
    time.sleep(1)
    while snapshot_isrun:
        try:
            if not queue_roi.empty():
                roi = queue_roi.get()
                frame = roi['frame']
                obj_class = roi['class']
                x1 = roi['x1']
                y1 = roi['y1']
                x2 = roi['x2']
                y2 = roi['y2']
                color = roi['color']
                print(obj_class)

                if not obj_class is None:
                    if not type(obj_class) is list:
                        obj_class = [obj_class]
                        x1 = [x1]
                        y1 = [y1]
                        x2 = [x2]
                        y2 = [y2]
                        color = [color]

                    for i in range(len(obj_class)):
                        print(type(frame))
                        # draw a bounding box rectangle and label on the frame
                        cv2.rectangle(frame, (x1[i], y1[i]), (x2[i], y2[i]), [102, 220, 225], 2)
                        #text = "{}: {:.4f}".format(obj_class[i], confidences[i])
                        text = "{}".format(obj_class[i])
                        cv2.putText(frame, text, (x1[i], y1[i] - 5), cv2.FONT_HERSHEY_SIMPLEX, 1.5, [102, 220, 225], 5)

                    # resize image frame for 1/2
                    #w_resize = int(frame.shape[1] * 0.5)
                    #h_resize = int(frame.shape[0] * 0.5)
                    #dim = (w_resize, h_resize)
                    #frame_resize = cv2.resize(frame, dim, interpolation=cv2.INTER_AREA)

                    # global frame for yolo debug
                    frame_yolo = frame

                    if displayflag == 'true':
                        # draw overlay
                        cv2.imshow("output frame", frame_yolo)
                        cv2.waitKey(1000)
                else:
                    # global frame for yolo debug
                    frame_yolo = frame

            time.sleep(0.1)

        except Exception as e:
            print("something is wrong in task overlay")
            print(e)

    cv2.destroyAllWindows()

def task_find_roi(queue_in, q_to_overlay, q_to_redis, coeff):
    global snapshot_isrun

    time.sleep(1)
    if snapshot_isrun:
        # Label File Configuration
        # load the class output labels of input YOLO model
        label_file = "yolov4-smartbin/obj.names"
        LABELS = open(label_file).read().strip().split("\n")
        print(LABELS)

        # initialize a list of colors to represent each possible class label
        np.random.seed(42)
        CLASS_COLORS = np.random.randint(0, 255, size=(len(LABELS), 3), dtype="uint8")

        # YOLO Configuration
        # load YOLO object detector (.weight file and .cfg file)
        cfg_file = "yolov4-smartbin/yolov4-smartbin.cfg"
        weight_file = "yolov4-smartbin/backup/yolov4-smartbin_2000.weights"
        print("[INFO] loading YOLO from disk...")
        net = cv2.dnn.readNetFromDarknet(cfg_file, weight_file)

        # determine only the *output* layer names that we need from YOLO
        ln = net.getLayerNames()
        print("getUnconectedOutLayer", net.getUnconnectedOutLayers())
        #ln = [ln[i[0] - 1] for i in net.getUnconnectedOutLayers()]
        ln = [ln[i - 1] for i in net.getUnconnectedOutLayers()]

        # Confidence & Threshold for making the decision in YOLO
        user_confidence=0.5
        user_threshold=0.3

    # looping for YOLO
    detected_obj = []
    detected_count=0

    while snapshot_isrun:
        try:
            # read image frame from queue
            frame_ready = False
            if not queue_in.empty():
                # read input buffer
                frame_in = queue_in.get()
                if not frame_in is None:
                    frame_ready = True

            # processing
            # YOLO object detection
            if frame_ready:
                # "frame" needs to be opencv's image-dtype
                # Input Image Configuration
                print("[INFO] accessing image ...")
                (H, W) = frame_in.shape[:2]

                # construct a blob from the input frame and then perform a forward
                # pass of the YOLO object detector, giving us our bounding boxes
                # and associated probabilities
                blob = cv2.dnn.blobFromImage(frame_in, 1 / 255.0, (416, 416), swapRB=True, crop=False)
                net.setInput(blob)

                layerOutputs = net.forward(ln)

                # initialize our lists of detected bounding boxes, confidences,
                # and class IDs, respectively
                boxes = []
                confidences = []
                classIDs = []

                # loop over each of the layer outputs
                for output in layerOutputs:
                    # loop over each of the detections
                    for detection in output:
                        # extract the class ID and confidence (i.e., probability)
                        # of the current object detection
                        scores = detection[5:]
                        classID = np.argmax(scores)
                        confidence = scores[classID]

                        # filter out weak predictions by ensuring the detected
                        # probability is greater than the minimum probability
                        if confidence > user_confidence:
                            # scale the bounding box coordinates back relative to
                            # the size of the image, keeping in mind that YOLO
                            # actually returns the center (x, y)-coordinates of
                            # the bounding box followed by the boxes' width and
                            # height
                            box = detection[0:4] * np.array([W, H, W, H])
                            (centerX, centerY, width, height) = box.astype("int")

                            # use the center (x, y)-coordinates to derive the top
                            # and and left corner of the bounding box
                            x = int(centerX - (width / 2))
                            y = int(centerY - (height / 2))

                            # update our list of bounding box coordinates,
                            # confidences, and class IDs
                            boxes.append([x, y, int(width), int(height)])
                            confidences.append(float(confidence))
                            classIDs.append(classID)

                # apply non-maxima suppression to suppress weak, overlapping
                # bounding boxes
                idxs = cv2.dnn.NMSBoxes(boxes, confidences, user_confidence, user_threshold)

                output_class = None
                pos_x = None
                pos_y = None
                pos_x1 = None
                pos_y1 = None
                pos_x2 = None
                pos_y2 = None
                output_class = None
                classes_color = None

                # ensure at least one detection exists
                if len(idxs) > 0:
                    pos_x1 = []
                    pos_y1 = []
                    pos_x2 = []
                    pos_y2 = []
                    output_class = []
                    classes_color = []

                    # loop over the indexes we are keeping
                    for i in idxs.flatten():
                        # extract the bounding box coordinates
                        (x1, y1) = (boxes[i][0], boxes[i][1])
                        (w, h) = (boxes[i][2], boxes[i][3])
                        x2 = x1 + w
                        y2 = y1 + h
                        # output the result with this format
                        # output_class = [output_class[0], output_class[1], output_class[2], ...]
                        # [x1[0], x1[1], x1[2], ...]
                        # [y1[0], y1[1], y1[2], ...]
                        # [x2[0], x2[1], x2[2], ...]
                        # [y2[0], y2[1], y2[2], ...]
                        color = [int(c) for c in CLASS_COLORS[classIDs[i]]]
                        output_class.append(LABELS[classIDs[i]])
                        pos_x1.append(x1)
                        pos_y1.append(y1)
                        pos_x2.append(x2)
                        pos_y2.append(y2)
                        classes_color.append(color)

                        # Decide which class of the object is, by using frequency count
                        for i in range(len(output_class)):
                            detected_obj.append(output_class[i])
                        detected_count += 1
                        from collections import Counter
                        if detected_count >= 5:
                            occurence_count = Counter(detected_obj)
                            objclass = occurence_count.most_common(1)[0][0]
                            print("detected object is ", objclass)
                            q_to_redis.put(objclass)
                            detected_obj = []
                            detected_count=0
                            snapshot_isrun = False

                # write to output buffer after finished the process
                print("Yolo finished")

                # prepare data for overlaying
                dict_output = {"frame":frame_in,
                               "class":output_class,
                               "x1":pos_x1,
                               "y1":pos_y1,
                               "x2":pos_x2,
                               "y2":pos_y2,
                               "color":classes_color
                               }
                q_to_overlay.put(dict_output)

            else:
                print("frame=None")
            time.sleep(0.1)

        except Exception as e:
            print("something wrong in task YOLO")
            print(e)

def task_write_to_redis(q_redis, tag_objclass):
    global snapshot_isrun

    # REDIS client
    r = redis.Redis(host='127.0.0.1', port=6379)

    # MQTT Client
    mqtt_broker="127.0.0.1"
    mqtt_port=1883
    client = mqtt.Client()
    client.on_connect=on_connect
    client.connect(mqtt_broker, mqtt_port, 60)

    # looping
    time.sleep(2)
    while worker_isrun:
        try:
            if not q_redis.empty():
                # Read dict and select only the first staffgauge detection
                lock.acquire()
                objclass = q_redis.get()
                lock.release()

                # Set REDIS tags
                r.publish(tag_objclass, objclass)
                #print("redis data sent")

                # Publish MQTT
                client.publish("YOLO/Result", "HOT")

            time.sleep(0.5)

        except Exception as e:
            print("something wrong in task REDIS")
            print(e)



if __name__ == "__main__":

    # Initialize parser
    parser = argparse.ArgumentParser()
    # Adding optional argument
    parser.add_argument("-s", "--source", help="source-type (webcam, video)", default='video')
    parser.add_argument("-j", "--json", help="JSON file for the configuration", default='config.json')
    parser.add_argument("-d", "--displayflag", help="display image with cv or not (true, false)", default='false')

    # Read arguments from command line
    args = parser.parse_args()

    # URL for video or camera source
    f = open(args.json)
    data = json.load(f)
    if args.source == "cam":
        source = data['cam']
    if args.source == "video":
        source = data['video']
    tag_objclass = data['tag_objclass']
    f.close()



    # Task: Snapshot
    # @Description: Take snapshot a frame from web-camera with opencv, Then queue frame to image queue
    # @Wait for task:

    # Task: ROI Extraction
    # @Description: ROI extraction with Yolov4 StaffGauge Detection
    # @Wait for task:
    # - Snapshot

    # Task: Image Overlay
    # @Description: Create new image with ROI overlay on the original image,
    # @Wait for task:
    # - ROI Extraction
    # - Snapshot

    # Task: Redis
    # @Description: Put JSON file of measurement data into REDIS



    # shared queue
    queue_snapshot = queue.Queue()
    queue_roi = queue.Queue()
    queue_redis = queue.Queue()

    # config tasks
    t1 = threading.Thread(target=task_snapshot, args=(queue_snapshot, source, args.source))
    t2 = threading.Thread(target=task_find_roi, args=(queue_snapshot, queue_roi, queue_redis, data['coeff']))
    t3 = threading.Thread(target=task_overlay, args=(queue_roi, args.displayflag))
    t4 = threading.Thread(target=task_write_to_redis, args=(queue_redis, tag_objclass))
    t5 = threading.Thread(target=task_mqttsub)

    # start tasks
    worker_isrun = True
    t1.start()
    t2.start()
    t3.start()
    t4.start()
    t5.start()

    # wait for all threads to finish
    while worker_isrun:
        try:
            if not t1.is_alive():
                print("restart snapshot task")
                time.sleep(1)
                t1 = threading.Thread(target=task_snapshot, args=(queue_snapshot, source, args.source))
                t1.start()
            if not t2.is_alive():
                print("restart task find roi")
                t2 = threading.Thread(target=task_find_roi, args=(queue_snapshot, queue_roi, queue_redis, data['coeff']))
                t2.start()
            if not t3.is_alive():
                print("restart task overlay")
                t3 = threading.Thread(target=task_overlay, args=(queue_roi, args.displayflag))
                t3.start()
            if not t4.is_alive():
                print("restart task redis")
                t4 = threading.Thread(target=task_write_to_redis, args=(queue_redis, tag_objclass))
                t4.start()
            if not t5.is_alive():
                print("restart task redis")
                t5 = threading.Thread(target=task_mqttsub)
                t5.start()

            time.sleep(1)

        except KeyboardInterrupt:
            print("main thread interrupted. Stop worker")
            worker_isrun = False
            snapshot_isrun = False
