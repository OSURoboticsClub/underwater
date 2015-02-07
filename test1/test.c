#include "worker.h"


int main()
{
    struct robot robot = init();

    fputs("test: ready\n", stdout);

    struct thruster_data td = {
        .ls = 20,
        .rs = 20,
        .fl = 800,
        .fr = 300,
        .bl = 800,
        .br = 300
    };

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 300000000
    };

    for (int i = 0; i <= 4; ++i) {
        struct sensor_data sd = wait_for_sensor_data(&robot);
        fputs("sensors = {", stdout);
        print_sensor_data(&sd);
        fputs("}\n", stdout);

        ++td.ls;
        ++td.rs;
        ++td.fl;
        ++td.fr;
        ++td.bl;
        ++td.br;
        nanosleep(&delay, NULL);

        set_thruster_data(&robot, &td);
        fputs("thrusters = {", stdout);
        print_thruster_data(&td);
        fputs("}\n", stdout);
    }

    while (1) { __asm(""); }  // spin forever
}
