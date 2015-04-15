#!/usr/bin/env python2

import sys
import time
import socket
import pyjoy
import serial
import pygame
from math import atan, cos, pi, sin, sqrt


MAX_MOTOR_VALUE = 180
MIN_MOTOR_VALUE = 25


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


def connect():
    UDP_IP = "192.168.1.177"
    UDP_PORT = 8888
    print "UDP target IP:", UDP_IP
    print "UDP target port:", UDP_PORT
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


def init_joystick(joy_idx):
    pygame.joystick.init()
    pygame.display.init()
    j = pygame.joystick.Joystick(joy_idx)
    j.init()

    print "Flushing event queue (don't touch the joystick)..."
    pygame.event.get()
    print '\nReady'


def send_command(command):
    message = [
        chr(command[0]), chr(command[1]), chr(command[2]), chr(command[3]),
        chr(command[4]), chr(command[5])]
    print message
    sock.sendto("".join(MESSAGE), (UDP_IP, UDP_PORT))
    recvstr = sock.recv(4096)
    print "send   {} {} {} {} {} {}".format(
        ord(MESSAGE[0]), ord(MESSAGE[1]), ord(MESSAGE[2]), ord(MESSAGE[3]),
        ord(MESSAGE[4]), ord(MESSAGE[5]))
    print "recv   {} {} {} {} {} {}".format(
        ord(recvstr[0]), ord(recvstr[1]), ord(recvstr[2]), ord(recvstr[3]),
        ord(recvstr[4]), ord(recvstr[5]))

    return recvstr

def main(joy_idx):
    # A list of axis values from the joystick
    joyAxis = [0,0,0,0,0,0]
    # Each boolean represents the value from the corresponding joystick button
    joyButtons = [False, False, False, False, False, False]

    command = [0,0,0,0,0,0]

    sock = connect()
    init_joystick(joy_idx)

    transmit_time = time.time()

    while True:
        if time.time() >= transmit_time:
            transmit_time += .25
            print command

        e = pygame.event.poll()
        if e.type not in (pygame.JOYAXISMOTION, pygame.JOYBUTTONDOWN,
                pygame.JOYBUTTONUP):
            continue
        # Read and change information in joyAxis or joyButtons
        #print e
        if e.type == pygame.JOYBUTTONDOWN:
            if (e.dict['button'] in range(6,12)):
                print "Bye!\n"
                quit()
            else:
                joyButtons[e.dict['button']] = True

        if e.type == pygame.JOYBUTTONUP:
            if (e.dict['button'] in range(0,6)):
                joyButtons[e.dict['button']] = False

        if e.type == pygame.JOYAXISMOTION:
            axis = e.dict['axis']
            value = e.dict['value']
            joyAxis[axis] = value

        # Construct Arduino command from all data
        xAxis = joyAxis[0]
        if xAxis == 0: # Prevents a divide by zero error
            xAxis = 10**-24
        yAxis = -joyAxis[1]
        rotAxis = joyAxis[2]

        # Joystick x and y are converted into polar coordinates and rotated
        # 45 degrees
        mag = sqrt(xAxis**2 + yAxis**2)
        if mag > 1:
            mag = 1
        theta = atan(yAxis/xAxis) + pi/4
        #print theta/pi
        motorA = abs(mag*sin(theta)/2)
        motorA *= 2 * MAX_MOTOR_VALUE # Values usable by the Arduino Servo library
        motorB = abs(mag*cos(theta)/2)
        motorB *= 2 * MAX_MOTOR_VALUE
        motorC = motorB
        motorD = motorA

        # Motor direction sent as a binary number with each bit
        # representing the motors A-D
        # x x x x
        # A B C D
        motorDirection = 0
        if xAxis > yAxis:
            motorDirection &= 0b1001
        else:
            motorDirection |= 0b0110
        if -xAxis > yAxis:
            motorDirection &= 0b0110
        else:
            motorDirection |= 0b1001

        # Create a binary button map from joyButtons
        buttonMap = 0
        for index, value in enumerate(joyButtons):
            if value:
                buttonMap |= 2**index

        # Build and output command list
        command = [int(motorA), int(motorB), int(motorC), int(motorD),
            motorDirection, buttonMap]


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage: client.py JOYSTICK_NUMBER'
        exit(2)
    main(int(sys.argv[1]))
