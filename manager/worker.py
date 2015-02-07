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

init = w.init
wait_for_tick = w.wait_for_tick
set_thruster_data = w.set_thruster_data
