import cv2
import face_recognition
import os
import numpy as np



def face_compare(face_input, face_ref_encodings, face_names):
    face_locations = face_recognition.face_locations(face_input)
    if face_locations == None or len(face_locations) != 1 :
        return False, face_input # No face is detected or More than 1 face

    matched = False
    face_input_encodings = face_recognition.face_encodings(face_input, face_locations)
    for face_input_encoding in face_input_encodings:
        face_distances = face_recognition.face_distance(face_ref_encodings, face_input_encoding)

        best_match_index = np.argmin(face_distances)
        if face_distances[best_match_index] < 0.5:
            matched = True
            top, right, bottom, left = face_locations[0]

            # Draw a box around the face
            cv2.rectangle(face_input, (left, top), (right, bottom), (0, 0, 255), 2)

            # Draw a label with a name below the face
            cv2.rectangle(face_input, (left, bottom - 35), (right, bottom), (0, 0, 255), cv2.FILLED)
            font = cv2.FONT_HERSHEY_DUPLEX
            cv2.putText(face_input, face_names[best_match_index], (left + 6, bottom - 6), font, 1.0, (255, 255, 255), 1)


    return matched, face_input

def demo_app():

    cam_source = "/dev/v4l/by-id/usb-046d_HD_Pro_Webcam_C920_BA15B86F-video-index0"
    user_face_folder = "face/"
    dir_list = os.listdir(user_face_folder)
    face_ref_encodings = []
    face_names = []
    for file in dir_list:
        face_name = os.path.splitext(file)[0]
        face_ref = cv2.imread(user_face_folder + file, cv2.IMREAD_COLOR)
        face_ref_encoding = face_recognition.face_encodings(face_ref)[0]
        face_ref_encodings.append(face_ref_encoding)
        face_names.append(face_name)
    print(face_names)

    stream = cv2.VideoCapture(cam_source)
    print("start capture")
    # looping
    skip_frame=0
    while stream.isOpened():
        ret, frame = stream.read()
        if not frame is None:
            skip_frame += 1
            if skip_frame>=5:
                # face recognition in here!
                matched, frame = face_compare(frame, face_ref_encodings, face_names)
                skip_frame=0
            cv2.imshow("output frame", frame)
            cv2.waitKey(1)
    stream.release()



if __name__ == '__main__':
    demo_app()
