#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


#define SOCKET_FILENAME "serial.socket"


int init_socket()
{
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) {
        fprintf(stderr, "serial: ERROR: Could not create socket (%s)\n",
            strerror(errno));
        return -1;
    }

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);
    if (bind(s, (struct sockaddr*)&sa, sizeof(struct sockaddr_un)) == -1) {
        fprintf(stderr, "serial: ERROR: Could not bind (%s)\n",
            strerror(errno));
        return -1;
    }

    if (listen(s, 5) == -1) {
        fprintf(stderr, "serial: ERROR: Could not listen (%s)\n",
            strerror(errno));
        return -1;
    }

    return s;
}


int main()
{
    int s = init_socket();
    if (s == -1) {
        unlink(SOCKET_FILENAME);
        return 1;
    }

    pause();
}
