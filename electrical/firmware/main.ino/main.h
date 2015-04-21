#include <Arduino.h>
#include <Servo.h>

#include "ethernetROV.h"
#include "getIMUData.h"
#include "serialUnderwaterROV.h"

// Define states
enum STATE {
  idle,
  talk,
  run,
  error
} current_state, commanded_state;

struct motors {
	int frontLeft;
	int frontRight;
	int backLeft;
	int backRight;
	int vertLeft;
	int vertRight;
} motorCommand;

struct armServos {
	int elbow;
	int wrist;
	int grasp;
} armCommand;


int counter;
