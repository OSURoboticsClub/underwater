#include "main.h"
#include "SabertoothSimplified.h"
#include "SerialUnderwaterROV.h"
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <Wire.h>

void setup() {
  // Setup UDP Communication to control computer
  // Setup communication with Saberteeth
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);
  
  // Initialize IMU
  accelBegin();
  magnoBegin();
  gyroBegin();
  
  current_state = idle;
  watchdogCounter = 0;
}

void loop() {
  if (Serial.available() > 0) {
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
      // TODO: Populate return packet
    }
    if (commanded_state == run) {
      /* TODO: Check error
      if (check_error()) {
        current_state = error;
      }
      */
      if (current_state == run) {
        // We're still in run so motors are safe
        // TODO: Command Saberteeth
      }
    }
    //TODO: Return packet to master
  }
  else {
    // No packet was recieved, increment counter and wait
    watchdogCounter++;
    if (watchdogCounter > 1000) {
      // We've lost communication
      current_state = error;
      // TODO: Return error packet
    }
    delay(10);
  }
}