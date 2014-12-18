#include "worker.h"


int main()
{
    struct robot robot = init();

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

    while (1) {
        wait_for_sensor_data(&robot);

        ++td.ls;
        ++td.rs;
        ++td.fl;
        ++td.fr;
        ++td.bl;
        ++td.br;
        nanosleep(&delay, NULL);

        set_thruster_data(&robot, &td);

        putchar('\n');
    }
}
