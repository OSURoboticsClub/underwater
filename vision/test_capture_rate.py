import os
import sys

import cv2


def test_capture_rate(device):
    try:
        os.mkdir('imgs')
    except OSError:
        pass

    c = cv2.VideoCapture(device)
    for i in range(20):
        print 'Capturing image {}...'.format(i)
        retval, img = c.read()
        if not retval:
            raise Exception("Couldn't capture frame.")
        cv2.imwrite('imgs/img{}.pnm'.format(i), img)


if __name__ == '__main__':
    test_capture_rate(int(sys.argv[1]))
