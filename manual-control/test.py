#!/usr/bin/env python2


import ctypes
from ctypes import byref
from time import sleep


class SensorData(ctypes.Structure):
    _fields_ = (
        ('a', ctypes.c_uint16),
        ('b', ctypes.c_uint16),
        ('c', ctypes.c_uint8),
        ('d', ctypes.c_double),
        ('e', ctypes.c_uint32),
    )


class ThrusterData(ctypes.Structure):
    _fields_ = (
        ('ls', ctypes.c_uint8),
        ('rs', ctypes.c_uint8),
        ('fl', ctypes.c_uint16),
        ('fr', ctypes.c_uint16),
        ('bl', ctypes.c_uint16),
        ('br', ctypes.c_uint16),
    )



class Robot(ctypes.Structure):
    _fields_ = (
        ('state', ctypes.c_void_p),
    )


w = ctypes.cdll.LoadLibrary('./libworker.so')

w.init.argtypes = ()
w.init.restype = Robot

w.wait_for_tick.argtypes = (ctypes.POINTER(Robot),)
w.wait_for_tick.restype = SensorData

w.set_thruster_data.argtypes = (
    ctypes.POINTER(Robot), ctypes.POINTER(ThrusterData))
w.set_thruster_data.restype = None


robot = w.init()
thruster_data = ThrusterData(20, 20, 800, 300, 800, 300)
while True:
    sensor_data = w.wait_for_sensor_data(byref(robot))

    thruster_data.ls += 1
    thruster_data.rs += 1
    thruster_data.fl += 1
    thruster_data.fr += 1
    thruster_data.bl += 1
    thruster_data.br += 1
    sleep(.3)

    w.set_thruster_data(byref(robot), byref(thruster_data))

    print
