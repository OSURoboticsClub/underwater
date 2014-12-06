//#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>


int main()
{
    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(server, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "test: ERROR: Could not connect to socket (%s)\n",
            strerror(errno));
        return 1;
    };

    char buf;
    while (1) {
        ssize_t received = recv(server, &buf, 1, 0);
        if (received == -1) {
            fprintf(stderr, "test: ERROR: recv() failed (%s)\n",
                strerror(errno));
            return 1;
        }
        if (received != 1) {
            fputs("Server disappeared.\n", stdout);
            return 0;
        }
        putchar(buf);
        putchar('\n');
    }
}
