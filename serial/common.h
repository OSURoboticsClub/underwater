#pragma once


#include <inttypes.h>
#include <stdint.h>


#define SOCKET_FILENAME "socket"


struct sensor_data {
    uint16_t a;
    uint16_t b;
    uint8_t c;
    double d;
    uint32_t e;
};


inline static void print_sensor_data(struct sensor_data* data)
{
    printf("%"PRIu16"  %"PRIu16"  %"PRIu8"  %f  %"PRIu32"\n",
        data->a, data->b, data->c, data->d, data->e);
}
