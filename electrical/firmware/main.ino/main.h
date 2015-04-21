#include <Arduino.h>
#include <Servo.h>

#include "ethernetROV.h"
#include "getIMUData.h"
#include "serialUnderwaterROV.h"

int watchdogCounter;

void EStop() {
	ST1.motor(1, 0);
	ST1.motor(2, 0);

	ST1.motor(1, 0);
	ST1.motor(2, 0);

	ST1.motor(1, 0);
	ST1.motor(2, 0);

	while (1);
}