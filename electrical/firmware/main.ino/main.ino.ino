#include "main.h"

void setup() {
  // Setup communication with ITX-Box
  Serial.begin(9600);
  // Setup communication with Saberteeth
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);
  
  current_state = idle;
  watchdog_counter = 0;
}

void loop() {
  if (Serial.available() > 0) {
    // We have communication with the master
    watchdog_counter = 0;
    // TODO: Authentication packet
    // TODO: Parse incoming packet
    current_state = commanded_state;
    if (commanded_state == talk or commanded_state == run) {
      // TODO: Read sensors
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
    watchdog_counter++;
    if (watchdog_counter > 100) {
      // We've lost communication
      current_state = error;
      // TODO: Return error packet
    }
    delay(10);
  }
}
