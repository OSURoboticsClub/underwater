#include "worker.h"


int main()
{
    struct robot robot = init();

    fputs("test: ready\n", stdout);

    while (1) {
        wait_for_tick(&robot);

        fputs("thrusters = {", stdout);
        print_thruster_data(&robot.state->thruster_data);
        fputs("}\n", stdout);
    }
}
