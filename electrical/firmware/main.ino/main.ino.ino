#include "main.h"
#include "SabertoothSimplified.h"
#include "SerialUnderwaterROV.h"
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <Wire.h>

void setup() {
	// Setup UDP Communication to control computer
	ethernetSetup();
	// Setup communication with Saberteeth
	Serial1.begin(9600);
	Serial2.begin(9600);
	Serial3.begin(9600);
	
	// Initialize IMU
	IMUSetup();
	
	current_state = idle;
	watchdogCounter = 0;
}

void loop() {
	if (ethernetRead()) {
		// We have communication with the master
		watchdogCounter = 0;
		// TODO: Authentication packet
		// TODO: Parse incoming packet
		current_state = commanded_state;
		if (commanded_state == talk or commanded_state == run) {
			// Reads sensors
			accelRead();
			magnoRead();
			gyroRead();
			IMUPacketize();
		}
		if (commanded_state == run) {
			/* TODO: Check error
			if (check_error()) {
				current_state = error;
			}
			*/
			if (current_state == run) {
				// We're still in run so motors are safe
				ST1.motor(1, motorCommand.frontLeft);
				ST1.motor(2, motorCommand.frontRight);

				ST2.motor(1, motorCommand.backLeft);
				ST2.motor(2, motorCommand.backRight);

				ST3.motor(1, motorCommand.vertLeft);
				ST3.motor(2, motorCommand.vertRight);
			}
		}
		if (commanded_state == error) {
			EStop();
		}
		ethernetWrite();
	}
	else {
		// No packet was recieved, increment counter and wait
		watchdogCounter++;
		if (watchdogCounter > 1000) {
			// We've lost communication
			current_state = error;
			EStop();
		}
	}
}