#include "utils.h"


uint8_t clamp_d(double x)
{
    if (x >= 256.0)
        return 0xFF;
    if (x < 0.0)
        return 0x00;
    return (uint8_t)x;
}


uint8_t clamp_2(int16_t x)
{
    if (x > 0xFF)
        return 0xFF;
    if (x < 0x00)
        return 0x00;
    return (uint8_t)x;
}
