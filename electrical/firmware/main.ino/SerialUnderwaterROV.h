#ifndef SERIALUNDERWATERROV_H_INCLUDED
#define SERIALUNDERWATERROV_H_INCLUDED

#include "SabertoothSimplified.h"

SabertoothSimplified ST1 = SabertoothSimplified(Serial1); //serial TX on pin 1, no RX pin
SabertoothSimplified ST2 = SabertoothSimplified(Serial2); //serial TX on pin 2
SabertoothSimplified ST3 = SabertoothSimplified(Serial3); //serial TX on pin 3

#endif // SERIALUNDERWATERROV_H_INCLUDED
