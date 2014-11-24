import cv2


def capture(device):
    """Capture frames from the device named or indexed by `device`"""

    c = cv2.VideoCapture(device)
    while True:
        retval, image = c.read()
        if retval:
            yield image
        else:
            raise Exception("Couldn't read() frame")
