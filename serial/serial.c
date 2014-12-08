#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "common.h"


static pthread_mutex_t mutex;


void communicate(union sigval sv)
{
    int mutex_status = pthread_mutex_trylock(&mutex);
    if (mutex_status != 0) {
        if (mutex_status == EBUSY) {
            fputs("serial: ERROR: Thing took too long to repond\n", stderr);
            abort();
        }
        fprintf(stderr, "serial: ERROR: Could not lock mutex (%s)\n",
            strerror(errno));
        abort();
    }

    fputs("serial --> Arduino (fake) ...\n", stdout);
    fputs("serial <-- Arduino (fake) ...\n", stdout);

    fputs("serial --> thing ...\n", stdout);
    struct sensor_data sensor_data = {
        .a = 6000,
        .b = 12345,
        .c = 196,
        .d = 3.14159,
        .e = 1234567
    };
    ssize_t count = send(sv.sival_int, &sensor_data, sizeof(sensor_data),
        MSG_NOSIGNAL);
    if (count == -1) {
        if (errno == EPIPE) {
            fputs("serial: ERROR: Thing disappeared.\n", stderr);
            abort();
        }
        fprintf(stderr,
            "serial: ERROR: Could not communicate with thing (%s)\n",
            strerror(errno));
        abort();
    } else if ((size_t)count < sizeof(sensor_data)) {
        fprintf(stderr, "serial: ERROR: Sent only %zu of %zu bytes to thing\n",
            (size_t)count, sizeof(sensor_data));
        abort();
    }
    print_sensor_data(&sensor_data);

    fputs("serial <-- thing ...\n", stdout);
    struct thruster_data thruster_data;
    count = recv(sv.sival_int, &thruster_data, sizeof(thruster_data),
        0);
    if (count == -1) {
        fprintf(stderr, "serial: ERROR: recv() failed (%s)\n",
            strerror(errno));
        abort();
    } else if (count == 0) {
        fputs("serial: ERROR: Thing disappeared.\n", stderr);
        abort();
    } else if ((size_t)count < sizeof(thruster_data)) {
        fprintf(stderr, "serial: ERROR: Received only %zu of %zu bytes\n",
            (size_t)count, sizeof(thruster_data));
        abort();
    }
    print_thruster_data(&thruster_data);

    pthread_mutex_unlock(&mutex);
}


int init_socket()
{
    int listener = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listener == -1) {
        fprintf(stderr, "serial: ERROR: Could not create socket (%s)\n",
            strerror(errno));
        return -1;
    }

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "serial: ERROR: Could not bind socket (%s)\n",
            strerror(errno));
        return -1;
    }

    if (listen(listener, 1) == -1) {
        fprintf(stderr, "serial: ERROR: Could not listen on socket (%s)\n",
            strerror(errno));
        return -1;
    }

    fputs("Waiting for socket connection...\n", stdout);
    socklen_t socklen = sizeof(sa);
    int s = accept(listener, (struct sockaddr*)&sa, &socklen);
    if (s == -1) {
        fprintf(stderr,
            "serial: ERROR: Could not accept connection on socket (%s)\n",
            strerror(errno));
        return -1;
    }

    return s;
}


int init_timer(int s)
{
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_int = s,
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

    int s = init_socket();
    if (s == -1)
        abort();

    pthread_mutex_init(&mutex, NULL);

    if (init_timer(s) == -1)
        abort();

    pause();
}
