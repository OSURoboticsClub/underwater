#include <Arduino.h>
#include <Servo.h>

#include "ethernetROV.h"
#include "getIMUData.h"
#include "serialUnderwaterROV.h"

int watchdogCounter;

Servo elbowServo;
Servo wristServo;
Servo graspServo;

void EStop() {
	ST1.motor(1, 0);
	ST1.motor(2, 0);

	ST1.motor(1, 0);
	ST1.motor(2, 0);

	ST1.motor(1, 0);
	ST1.motor(2, 0);

	elbowServo.write(90);
	wristServo.write(90);
	graspServo.write(90);

	while (1);
}