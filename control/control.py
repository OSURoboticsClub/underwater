#!/usr/bin/env python2

from __future__ import unicode_literals

import errno
import pygame
import pyjoy
import serial
import socket
import sys
import time
from math import sqrt
from sys import stdout


ROT_SC = 0.1


def itb16(i):
    return (i >> 8, i & (2**8 - 1))


def clamp8(i):
    return max(min(i, 127), -128)


def clamp16(i):
    return max(min(i, 32767), -32768)


def clamp16u(i):
    return max(min(i, 65536), 0)


def fti8(f):
    return clamp8(int(f * 128))


def fti16(f):
    return clamp16(int(f * 32768))


def format_msg(msg):
    return (
        ''.join('%02x' % (byte % 256) for byte in msg) + '\n' +
        '  '.join('% 3d' % (byte % 256) for byte in msg) + '\n'
    )


def get_arduino_data(joyAxis, joyButtons):
    """
    Gets passed all axis and button info from the joystick and returns a list
    containing four motor values and a button map to send to the Arduino
    """
    xAxis = joyAxis[0]
    yAxis = joyAxis[1]
    rotAxis = joyAxis[2]
    # Joystick x and y are converted into polar coordinates and rotated 45
    # degrees
    mag = sqrt(xAxis**2 + yAxis**2)
    theta = atan(yAxis/xAxis) + pi/4
    motorA = mag*cos(theta)/2
    motorB = -mag*sin(theta)/2
    motorC = motorB
    motorD = motorA
    # Create a binary button map from joyButtons
    buttonMap = 0
    for index, value in enumerate(joyButtons):
        if value:
            buttonMap += 2**index

    # Build and output command list
    return [motorA, motorB, motorC, motorD, buttonMap]


class Arduino(object):
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.local_addr = ('0.0.0.0', port)
        self.remote_addr = (host, port)
        self.remote_addr_str = '{}:{}'.format(host, port)

    def connect(self):
        print 'Connecting to {}:{}'.format(self.host, self.port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('0.0.0.0', self.port))
        self.sock.setblocking(0)

    def send(self, state, fl, fr, br, bl, l, r, s1, s2, s3):
        msg = (state, fl, fr, br, bl, l, r, s1, s2, s3)
        self.sock.sendto(
            b''.join(chr(byte % 256) for byte in msg), 0, self.remote_addr)
        stdout.write(format_msg(msg).encode('utf8'))

    def recv(self):
        try:
            msg = self.sock.recv(1)
            return msg
        except socket.error as e:
            if e.errno not in (errno.EAGAIN, errno.EWOULDBLOCK):
                raise
            return


def init_joystick(joy_idx):
    print '=== Setting up ===\n'

    pygame.joystick.init()
    pygame.display.init()
    j = pygame.joystick.Joystick(joy_idx)
    j.init()

    print "Flushing event queue (don't touch the joystick)..."
    pygame.event.get()
    print 'Event queue flushed'
    print '\n=== Ready ===\n'


def main(joy_idx, host, port):
    stick = [0] * 6
    buttons = [False] * 6

    ard = Arduino(host, port)
    ard.connect()
    init_joystick(joy_idx)

    transmit_time = time.time()

    fl = fr = bl = br = l = r = 0

    estop = 0
    s1 = s2 = s3 = 0

    while True:
        msg = ard.recv()
        if msg:
            print 'counter = %02x' % ord(msg)
            print
        tick = time.time() >= transmit_time
        if tick:
            if not estop:
                s1 = 90 + 90 * int(stick[4])
                s2 = 90 + 90 * int(stick[5])
                s3 = 90 + 90 * (int(buttons[0]) - int(buttons[1]))

            transmit_time += .25
            print stick, buttons
            ard.send(estop, fl, fr, br, bl, l, r, s1, s2, s3)
            print

        if estop:
            continue

        # Get next event.
        e = pygame.event.poll()
        if e.type not in (pygame.JOYAXISMOTION, pygame.JOYBUTTONDOWN,
                pygame.JOYBUTTONUP):
            continue

        # Handle event.
        if e.type == pygame.JOYBUTTONDOWN:
            num = e.dict['button']
            if 6 <= num < 12:
                estop = 1
            else:
                buttons[e.dict['button']] = True
        elif e.type == pygame.JOYBUTTONUP:
            if not (6 <= num < 12):
                buttons[e.dict['button']] = False
        elif e.type == pygame.JOYAXISMOTION:
            axis = e.dict['axis']
            value = e.dict['value']
            stick[axis] = value

        x = stick[0]
        y = -stick[1]
        z = -stick[2]
        sc = (-stick[3] + 1.0) / 2.0

        fl = br = sqrt(2) / 2 * (x + y)
        fr = bl = sqrt(2) / 2 * (-x + y)
        l = r = int(buttons[4]) - int(buttons[2])

        # rot is counter-clockwise
        rot = ROT_SC * z
        fl += -rot
        fr += rot
        br += rot
        bl += -rot

        # TODO: Make this prettier.
        fl = clamp8(int(128 * sc * fl))
        fr = clamp8(int(128 * sc * fr))
        br = clamp8(int(128 * sc * br))
        bl = clamp8(int(128 * sc * bl))
        l = clamp8(int(128 * sc * l))
        r = clamp8(int(128 * sc * r))


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print 'Usage: client.py JOY# HOST PORT'
        exit(2)
    main(int(sys.argv[1]), sys.argv[2], int(sys.argv[3]))
