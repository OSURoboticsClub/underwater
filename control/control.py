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


TIME_SC = 0.05

HOR_SC = 1.0
VERT_SC = 1.0
ROT_SC = 0.6
ROLL_SC = 0.9

S1_INC = int(round(40 * TIME_SC))


class State(object):
    IDLE = 0
    TALK = 1
    RUN = 2
    ERROR = 3


def itb16(i):
    return (i >> 8, i & (2**8 - 1))


def clamp8s(i):
    return max(min(i, 127), -128)


def clamp8u(i):
    return max(min(i, 255), 0)


def clamp16s(i):
    return max(min(i, 32767), -32768)


def clamp16u(i):
    return max(min(i, 65536), 0)


def fti8s(f):
    return clamp8s(int(round(f)))


def fti8u(f):
    return clamp8u(int(round(f)))


def fti16s(f):
    return clamp16s(int(round(f)))


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

    def send(self, state, fl, fr, bl, br, l, r, s1):
        msg = [state] + map(fti8s, (fr, -fl, bl, -br, l, -r)) + [fti8u(s1), fti8u(s1), fti8u(s1)]
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

    x = y = z = th = hx = hy = 0
    fl = fr = bl = br = l = r = 0
    s1 = 45

    state = State.RUN
    estop = False

    while True:
        msg = ard.recv()
        if msg:
            print 'counter = %02x' % ord(msg)
            print
        tick = time.time() >= transmit_time
        if tick:
            if not estop:
                s1 = min(max(s1 + S1_INC * hy, 30), 120)
            transmit_time += TIME_SC
            print stick, buttons
            ard.send(state, fl, fr, bl, br, l, r, s1)
            print

        if estop:
            continue

        # Get next event.
        e = pygame.event.poll()
        if e.type not in (pygame.JOYAXISMOTION, pygame.JOYBUTTONDOWN,
                pygame.JOYBUTTONUP):
            continue

        # Handle event.
        if e.type in (pygame.JOYBUTTONDOWN, pygame.JOYBUTTONUP):
            num = e.dict['button']

            if 6 <= num < 12:
                estop = True
                fl = fr = br = bl = l = r = 0
                s1 = s2 = s3 = 90
            else:
                buttons[e.dict['button']] = (e.type == pygame.JOYBUTTONDOWN)
                hx = int(buttons[3]) - int(buttons[2])
                hy = int(buttons[0]) - int(buttons[1])
        elif e.type == pygame.JOYAXISMOTION:
            axis = e.dict['axis']
            value = e.dict['value']
            stick[axis] = value

            x = stick[0]
            y = -stick[1]
            z = -stick[2]
            th = -stick[3]

            fl = br = sqrt(2) / 2 * (x + y) * HOR_SC
            fr = bl = sqrt(2) / 2 * (-x + y) * HOR_SC

            # rot is counter-clockwise
            rot = ROT_SC * z
            fl += -rot
            fr += rot
            br += rot
            bl += -rot

            fl, fr, br, bl = map(
                lambda n: fti8s(128.0 * n),
                (fl, fr, br, bl))


        l = r = fti8s(128.0 * th * VERT_SC)
        l += 128.0 * hx * ROLL_SC
        r += 128.0 * -hx * ROLL_SC


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print 'Usage: client.py JOY# HOST PORT'
        exit(2)
    main(int(sys.argv[1]), sys.argv[2], int(sys.argv[3]))
