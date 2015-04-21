#ifndef SERIALUNDERWATERROV_H_INCLUDED
#define SERIALUNDERWATERROV_H_INCLUDED

#include <SabertoothSimplified.h>

SabertoothSimplified ST1 = SimplifiedSerial(NOT_A_PIN, 1); //serial TX on pin 1, no RX pin
SabertoothSimplified ST2 = SimplifiedSerial(NOT_A_PIN, 2); //serial TX on pin 2
SabertoothSimplified ST3 = SimplifiedSerial(NOT_A_PIN, 3); //serial TX on pin 3

void saberToothSetup()
{
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);
}
#endif // SERIALUNDERWATERROV_H_INCLUDED
