import cv2


def main():
    cap = cv2.VideoCapture("rtsp://8199-108-30-53-54.ngrok-free.app/stream")

    while cap.isOpened():
        _, frame = cap.read()
        cv2.imshow('frame', frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    cap.release()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
