//#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"


int main()
{
    int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "test: ERROR: Could not connect to socket (%s)\n",
            strerror(errno));
        return 1;
    };

    int i = 0;
    struct sensor_data sensor_data;
    struct thruster_data thruster_data;
    while (1) {
        fputs("thing <-- serial ...\n", stdout);
        ssize_t count = recv(s, &sensor_data, sizeof(sensor_data), 0);
        if (count == -1) {
            fprintf(stderr, "test: ERROR: recv() failed (%s)\n",
                strerror(errno));
            return 1;
        } else if (count == 0) {
            fputs("Socket disappeared\n", stdout);
            return 0;
        } else if ((size_t)count < sizeof(sensor_data)) {
            fprintf(stderr, "test: ERROR: Received only %zu of %zu bytes\n",
                (size_t)count, sizeof(sensor_data));
            return 1;
        }
        print_sensor_data(&sensor_data);

        if (i++ == 4)
            sleep(3);

        fputs("thing --> serial ...\n", stdout);
        thruster_data.ls = 20;
        thruster_data.rs = 20;
        thruster_data.fl = 800;
        thruster_data.fr = 300;
        thruster_data.bl = 800;
        thruster_data.br = 300;
        count = send(s, &thruster_data, sizeof(thruster_data),
            MSG_NOSIGNAL);
        if (count == -1) {
            if (errno == EPIPE) {
                fputs("Socket disappeared\n", stdout);
                return 0;
            }
            fprintf(stderr, "test: ERROR: send() failed (%s)\n",
                strerror(errno));
            return 1;
        } else if ((size_t)count < sizeof(thruster_data)) {
            fprintf(stderr, "serial: ERROR: Sent only %zu of %zu bytes\n",
                (size_t)count, sizeof(thruster_data));
            return 1;
        }
        print_thruster_data(&thruster_data);
    }
}
