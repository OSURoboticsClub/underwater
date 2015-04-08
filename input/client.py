import sys
import time
import socket
import pyjoy
import serial
import pygame
from math import *


joy_idx = int(sys.argv[1])


MAX_MOTOR_VALUE = 180 # Sets the highest motor value that is able to be sent to the Arduino
MIN_MOTOR_VALUE = 25 # Sets the lowest motor value that is able to be sent to the Arduino

def getArduinoData(joyAxis, joyButtons):
    """Gets passed all axis and button info from the joystick and returns a list
    containing four motor values and a button map to send to the Arduino"""
    xAxis = joyAxis[0]
    yAxis = joyAxis[1]
    rotAxis = joyAxis[2]
    # Joystick x and y are converted into polar coordinates and rotated 45 degrees
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

recvstr = "  "

# A list of axis values from the joystick
joyAxis = [0,0,0,0,0,0]
# Each boolean represents the value from the corresponding joystick button
joyButtons = [False, False, False, False, False, False]

UDP_IP = "192.168.1.177"
UDP_PORT = 8888

print "UDP target IP:", UDP_IP
print "UDP target port:", UDP_PORT

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
pygame.joystick.init()
pygame.display.init()
if not pygame.joystick.get_count():
    print"\nplug in a joystick\n"
    quit()

myjoy = pygame.joystick.Joystick(joy_idx)
myjoy.init()

print 'Wait a second and then press a button on the joystick'
start_time = time.time()
while time.time() - start_time < 0.5:
    pygame.event.wait()

print "Depress lower rack of buttons to quit\n"

command = [0,0,0,0,0,0]

while True:
    e = pygame.event.wait()
    if (e.type == pygame.JOYAXISMOTION or e.type == pygame.JOYBUTTONDOWN or e.type == pygame.JOYBUTTONUP):
        # Read and change information in joyAxis or joyButtons
        print e
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

        # Joystick x and y are converted into polar coordinates and rotated 45 degrees
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

        # Motor direction sent as a binary number with each bit representing the motors A-D
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
        command = [int(motorA), int(motorB), int(motorC), int(motorD), motorDirection, buttonMap]
    #MESSAGE = [chr(command[0]), chr(command[1]), chr(command[2]), chr(command[3]), chr(command[4]), chr(command[5])]
    #sock.sendto("".join(MESSAGE), (UDP_IP, UDP_PORT))
    #recvstr = sock.recv(4096)
    #print "MESSAGE = {} {} {} {} {} {}".format(ord(MESSAGE[0]), ord(MESSAGE[1]), ord(MESSAGE[2]), ord(MESSAGE[3]), ord(MESSAGE[4]), ord(MESSAGE[5]))
    #print "Received: {} {} {} {} {} {}".format(ord(recvstr[0]), ord(recvstr[1]), ord(recvstr[2]), ord(recvstr[3]), ord(recvstr[4]), ord(recvstr[5]))
