#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>


#define SOCKET_FILENAME "serial.socket"


void send_message(union sigval sv)
{
    fputs("Pretending to send message...\n", stdout);
}


int init_socket()
{
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        fprintf(stderr, "serial: ERROR: Could not create socket (%s)\n",
            strerror(errno));
        return -1;
    }

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);
    if (bind(socket_fd, (struct sockaddr*)&sa, sizeof(struct sockaddr_un))
            == -1) {
        fprintf(stderr, "serial: ERROR: Could not bind (%s)\n",
            strerror(errno));
        return -1;
    }

    if (listen(socket_fd, 5) == -1) {
        fprintf(stderr, "serial: ERROR: Could not listen (%s)\n",
            strerror(errno));
        return -1;
    }

    return socket_fd;
}


int init_timer()
{
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_int = 0;
    sev.sigev_notify_function = send_message;
    sev.sigev_notify_attributes = NULL;
    timer_t timerid;
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        fprintf(stderr, "serial: ERROR: Could not create timer (%s)\n",
            strerror(errno));
        return -1;
    }

    struct itimerspec its;
    its.it_interval.tv_sec = 1;  // every second
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 1;  // arm timer
    its.it_value.tv_nsec = 0;
    timer_settime(timerid, 0, &its, NULL);

    return 0;
}


void die(int signum)
{
    assert(signum == SIGINT || signum == SIGQUIT || signum == SIGABRT);
    unlink(SOCKET_FILENAME);
    fputs("\nserial: Dying...\n", stderr);
    exit(2);
}


int main()
{
    if (signal(SIGABRT, die) == SIG_ERR) {
        fprintf(stderr,
            "serial: ERROR: Could not set up SIGABRT handler (%s)\n",
            strerror(errno));
        exit(1);
    }

    fputs("Initializing socket...\n", stdout);
    int socket_fd = init_socket();
    if (socket_fd == -1)
        abort();

    if (signal(SIGINT, die) == SIG_ERR
            || signal(SIGQUIT, die) == SIG_ERR) {
        fprintf(stderr,
            "serial: ERROR: Could not set up signal handler (%s)\n",
            strerror(errno));
        abort();
    }

    fputs("Initializing timer...\n", stdout);
    if (init_timer() == -1)
        abort();

    fd_set socket_fd_set;
    FD_ZERO(&socket_fd_set);
    FD_SET(socket_fd, &socket_fd_set);
    while (1) {
        fputs("Waiting for connections on socket...\n", stdout);
        select(socket_fd + 1, &socket_fd_set, NULL, NULL, NULL);
    }
}
