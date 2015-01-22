#include "main.h"

void setup() {
  // Setup communication with ITX-Box
  Serial.begin(9600);
  // Setup communication with Saberteeth
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);
}

void loop() {
  if (Serial.available > 0) {
    // We have communication with the master
    /* TODO: Read sensors
     * Return packet
     * Check commanded state
     * Check error
     * Command Saberteeth
     */
    if (commanded_state = 0)
  }
}
