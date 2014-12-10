//#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <err.h>
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
        warn("Could not connect to socket");
        return 1;
    };

    fputs("Connected to socket.\n", stdout);

    int i = 0;
    struct sensor_data sensor_data;
    struct thruster_data thruster_data;
    while (1) {
        fputs("thing <-- manager ...\n", stdout);
        ssize_t count = recv(s, &sensor_data, sizeof(sensor_data), 0);
        if (count == -1) {
            warn("recv() failed");
            return 1;
        } else if (count == 0) {
            warnx("Socket disappeared");
            return 0;
        } else if ((size_t)count < sizeof(sensor_data)) {
            warnx("Received only %zu of %zu bytes", (size_t)count,
                sizeof(sensor_data));
            return 1;
        }
        print_sensor_data(&sensor_data);

        if (i++ == 4)
            sleep(3);

        fputs("thing --> manager ...\n", stdout);
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
                warnx("Socket disappeared");
                return 0;
            }
            warn("send() failed");
            return 1;
        } else if ((size_t)count < sizeof(thruster_data)) {
            warnx("Sent only %zu of %zu bytes", (size_t)count,
                sizeof(thruster_data));
            return 1;
        }
        print_thruster_data(&thruster_data);
    }
}
