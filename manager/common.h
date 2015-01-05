#pragma once

#define _POSIX_C_SOURCE 200112L

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


#define SOCKET_FILENAME "socket"


struct sensor_data {
    uint16_t a;
    uint16_t b;
    uint8_t c;
    double d;
    uint32_t e;
};


struct thruster_data {
    uint8_t ls;
    uint8_t rs;
    uint16_t fl;
    uint16_t fr;
    uint16_t bl;
    uint16_t br;
};


struct state {
    struct sensor_data sensor_data;
    pthread_mutex_t sensor_data_mutex;
    struct thruster_data thruster_data;
    pthread_mutex_t thruster_data_mutex;
};


struct worker_ctl {
    pthread_mutex_t n_mutex;
    pthread_cond_t n_cond;
    bool n;
};


inline static void print_sensor_data(struct sensor_data* data)
{
    printf("%"PRIu16"  %"PRIu16"  %"PRIu8"  %f  %"PRIu32,
        data->a, data->b, data->c, data->d, data->e);
}


inline static void print_thruster_data(struct thruster_data* data)
{
    printf("%"PRIu8"  %"PRIu8"  %"PRIu16"  %"PRIu16"  %"PRIu16"  %"PRIu16,
        data->ls, data->rs, data->fl, data->fr, data->bl, data->br);
}
