#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "data.h"


#define SOCKET_FILENAME "socket"


int thing;  // Python program that does things


void communicate(union sigval sv)
{
    fputs("Pretending to send message to Arduino...\n", stdout);
    fputs("Pretending to receive message from Arduino...\n", stdout);

    struct data data = {
        .a = 6000,
        .b = 12345,
        .c = 196,
        .d = 3.14159,
        .e = 1234567
    };
    ssize_t sent = send(thing, &data, sizeof(data), MSG_NOSIGNAL);
    if (sent == -1) {
        if (errno == EPIPE) {
            fputs("serial: ERROR: Thing disappeared.\n", stdout);
            abort();
        } else {
            fprintf(stderr,
                "serial: ERROR: Could not communicate with thing (%s)\n",
                strerror(errno));
            abort();
        }
    } else if ((size_t)sent < sizeof(data)) {
        fprintf(stderr, "serial: ERROR: Sent only %zu of %zu bytes to thing\n",
            (size_t)sent, sizeof(data));
        abort();
    }
}


int init_socket()
{
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener == -1) {
        fprintf(stderr, "serial: ERROR: Could not create socket (%s)\n",
            strerror(errno));
        return -1;
    }

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa))
            == -1) {
        fprintf(stderr, "serial: ERROR: Could not bind socket (%s)\n",
            strerror(errno));
        return -1;
    }

    if (listen(listener, 1) == -1) {
        fprintf(stderr, "serial: ERROR: Could not listen on socket (%s)\n",
            strerror(errno));
        return -1;
    }

    return listener;
}


int init_timer()
{
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_int = 0,
        .sigev_notify_function = communicate,
        .sigev_notify_attributes = NULL
    };
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
    unlink(SOCKET_FILENAME);  // Ignore errors.
    fputs("serial: Dying...\n", stderr);
    exit(2);
}


int main()
{
    fputs("Initializing...\n", stdout);

    if (signal(SIGABRT, die) == SIG_ERR) {
        fprintf(stderr,
            "serial: ERROR: Could not set up SIGABRT handler (%s)\n",
            strerror(errno));
        exit(1);
    }

    if (signal(SIGINT, die) == SIG_ERR || signal(SIGQUIT, die) == SIG_ERR) {
        fprintf(stderr,
            "serial: ERROR: Could not set up signal handlers (%s)\n",
            strerror(errno));
        abort();
    }

    int listener = init_socket();
    if (listener == -1)
        abort();

    fd_set listener_set;
    FD_ZERO(&listener_set);
    FD_SET(listener, &listener_set);

    fputs("Waiting for connection on socket...\n", stdout);
    if (select(listener + 1, &listener_set, NULL, NULL, NULL) == -1) {
        fprintf(stderr, "serial: ERROR: select() failed (%s)\n",
            strerror(errno));
        abort();
    }
    thing = accept(listener, NULL, 0);
    if (thing == -1) {
        fprintf(stderr, "serial: ERROR: accept() failed (%s)\n",
            strerror(errno));
        abort();
    }
    fputs("Accepted connection from thing\n", stdout);

    if (init_timer() == -1)
        abort();

    pause();
}
