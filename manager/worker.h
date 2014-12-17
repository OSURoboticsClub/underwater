#pragma once


#include "common.h"


struct robot {
    struct state* state;
};


struct robot init();
void wait_for_sensor_data(struct robot* robot);
void set_thruster_data(struct robot* robot, struct thruster_data* td);
