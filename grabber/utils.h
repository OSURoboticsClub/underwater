#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

// Clamp double to uint8_t.
uint8_t clamp_d(double x);

// Clamp int16_t to uint8_t.
uint8_t clamp_2(int16_t x);

#endif  // UTILS_H
