/*
Arduino Library for Sabertooth Simplified Serial
Copyright (c) 2012-2013 Dimension Engineering LLC
http://www.dimensionengineering.com/arduino

ermission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SabertoothSimplified_h
#define SabertoothSimplified_h

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#if defined(USBCON)
#define SabertoothTXPinSerial Serial1 // Arduino Leonardo has TX->1 on Serial1, not Serial.
#else
#define SabertoothTXPinSerial Serial
#endif
#define SyRenTXPinSerial SabertoothTXPinSerial

class SabertoothSimplified
{
public:
  SabertoothSimplified();

  SabertoothSimplified(Print& port);

public:
     void motor(int power);

     void motor(byte motor, int power);

     void drive(int power);

     void turn(int power);

     void stop();

  private:
    void mixedMode(boolean enable);
    void mixedUpdate();
    void raw(byte motor, int power);

  private:
    boolean _mixed;
    int     _mixedDrive, _mixedTurn;
    boolean _mixedDriveSet, _mixedTurnSet;
    Print&  _port;
  };

  #endif
