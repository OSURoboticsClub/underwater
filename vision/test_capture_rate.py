#!/usr/bin/env python2

import os
import sys
from time import sleep

from opencv import cv2


def test_capture_rate(device):
    try:
        os.mkdir('imgs')
    except OSError:
        pass

    c = cv2.VideoCapture(device)
    print 'Preparing camera...'
    c.read()  # Capture a frame to turn the camera on.
    sleep(2)  # Wait for it to adjust to the current light level.

    # Capture some frames to get the camera up to full capturing speed. I don't
    # know why this is necessary.
    for i in range(5):
        c.read()

    for i in range(20):
        print 'Capturing image {}...'.format(i)
        retval, img = c.read()
        if not retval:
            raise Exception("Couldn't capture frame.")
        cv2.imwrite('imgs/img{}.pnm'.format(i), img)


if __name__ == '__main__':
    test_capture_rate(int(sys.argv[1]))
