#pragma once


#include "common.h"


struct robot {
    struct state* state;
    struct worker_ctl* ctl;
};


// Connect to the manager process.
struct robot init();

// Wait for manager to notify us; return fresh sensor data.
struct sensor_data wait_for_tick(struct robot* robot);

// Put thruster data somewhere the manager will see it the next time it looks.
void set_thruster_data(struct robot* robot, struct thruster_data* td);
