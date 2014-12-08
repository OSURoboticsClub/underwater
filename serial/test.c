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
    int server = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(server, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "test: ERROR: Could not connect to socket (%s)\n",
            strerror(errno));
        return 1;
    };

    struct sensor_data data;
    while (1) {
        fputs("thing <-- serial ...\n", stdout);
        ssize_t received = recv(server, &data, sizeof(data), 0);
        if (received == -1) {
            fprintf(stderr, "test: ERROR: recv() failed (%s)\n",
                strerror(errno));
            return 1;
        } else if (received == 0) {
            fputs("Server disappeared\n", stdout);
            return 0;
        } else if ((size_t)received < sizeof(data)) {
            fprintf(stderr, "test: ERROR: Received only %zu of %zu bytes\n",
                (size_t)received, sizeof(data));
            continue;
        }
        print_sensor_data(&data);
    }
}
