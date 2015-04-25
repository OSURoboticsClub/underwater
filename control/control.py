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


HOR_SC = 1.0
VERT_SC = 1.0
ROT_SC = 0.1


class State(object):
    IDLE = 0
    TALK = 1
    RUN = 2
    ERROR = 3


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
    stick = [0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
    buttons = [False] * 6

    ard = Arduino(host, port)
    ard.connect()
    init_joystick(joy_idx)

    transmit_time = time.time()

    fl = fr = bl = br = l = r = 0
    s1 = s2 = s3 = 90
    sc = 0.0

    state = State.RUN

    while True:
        msg = ard.recv()
        if msg:
            print 'counter = %02x' % ord(msg)
            print
        tick = time.time() >= transmit_time
        if tick:
            transmit_time += .25
            print stick, buttons
            ard.send(state, fl, fr, br, bl, l, r, s1, s2, s3)
            print

        if state == State.ERROR:
            continue

        # Get next event.
        e = pygame.event.poll()
        if e.type not in (pygame.JOYAXISMOTION, pygame.JOYBUTTONDOWN,
                pygame.JOYBUTTONUP):
            continue

        z = -stick[2]

        # Handle event.
        if e.type in (pygame.JOYBUTTONDOWN, pygame.JOYBUTTONUP):
            num = e.dict['button']

            if 6 <= num < 12:
                state = State.ERROR
                fl = fr = br = bl = l = r = 0
                s1 = s2 = s3 = 90
            else:
                buttons[e.dict['button']] = (e.type == pygame.JOYBUTTONDOWN)

                l = r = clamp8(int(
                    128 * sc * VERT_SC * buttons[4] - buttons[2]))

                s3 = 90 + 90 * (int(buttons[0]) - int(buttons[1]))
        elif e.type == pygame.JOYAXISMOTION:
            axis = e.dict['axis']
            value = e.dict['value']
            stick[axis] = value

            x = stick[0]
            y = -stick[1]
            sc = (-stick[3] + 1.0) / 2.0

            fl = br = sqrt(2) / 2 * (x + y)
            fr = bl = sqrt(2) / 2 * (-x + y)

            # rot is counter-clockwise
            rot = ROT_SC * z
            fl += -rot
            fr += rot
            br += rot
            bl += -rot

            fl, fr, br, bl = map(
                lambda x: clamp8(int(128 * sc * HOR_SC * x)),
                (fl, fr, br, bl))

            s1 = 90 + 90 * int(stick[4])
            s2 = 90 + 90 * int(stick[5])


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print 'Usage: client.py JOY# HOST PORT'
        exit(2)
    main(int(sys.argv[1]), sys.argv[2], int(sys.argv[3]))
