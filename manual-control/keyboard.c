#define _BSD_SOURCE 1

#include "worker.h"

#include <stdio.h>
#include <termios.h>
#include <unistd.h>


#define KEY_7 55
#define KEY_8 56
#define KEY_9 57
#define KEY_U 117
#define KEY_I 105
#define KEY_O 111
#define KEY_J 106
#define KEY_K 107
#define KEY_L 108


int main()
{
    struct robot robot = init();
    (void)robot;

    fputs("keyboard: ready\n", stdout);

    {
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
    }

    struct thruster_data td = {0}; // all zeroes

    while (1) {
        char key;
        ssize_t count = read(STDIN_FILENO, &key, 1);
        if (count == -1) {
            perror("Can't read char from stdin");
            continue;
        } else if (count == 0) {
            fputs("Got EOF on stdin\n", stdout);
            return 0;
        }

        printf("key = %d\n", key);

        if (key == KEY_8 || key == KEY_9 || key == KEY_O) {
            td.fl = -500;
            td.br = 500;
        } else if (key == KEY_K || key == KEY_J || key == KEY_U) {
            td.fl = 500;
            td.br = -500;
        } else {
            td.fl = td.br = 0;
        }

        if (key == KEY_U || key == KEY_7 || key == KEY_8) {
            td.fr = -500;
            td.bl = 500;
        } else if (key == KEY_O || key == KEY_L || key == KEY_K) {
            td.fr = 500;
            td.bl = -500;
        } else {
            td.fr = td.bl = 0;
        }

        set_thruster_data(&robot, &td);
    };
}
