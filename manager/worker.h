#pragma once


#include "common.h"


struct robot {
    struct state* state;
    struct worker_ctl* ctl;
};


// Connect to the manager process.
struct robot init();

// Wait for manager to notify us of fresh sensor data; return it.
struct sensor_data wait_for_sensor_data(struct robot* robot);

// Put thruster data somewhere the manager will see it the next time it looks.
void set_thruster_data(struct robot* robot, struct thruster_data* td);
