#define _POSIX_C_SOURCE 200112L

#include <err.h>
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
            warnx("Thing took too long to respond");
            abort();
        }
        warnx("Can't lock mutex");
        abort();
    }

    fputs("manager --> Arduino (fake) ...\n", stdout);
    fputs("manager <-- Arduino (fake) ...\n", stdout);

    fputs("manager --> thing ...\n", stdout);
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
            warnx("Thing disappeared");
            abort();
        }
        warn("Can't communicate with thing");
        abort();
    } else if ((size_t)count < sizeof(sensor_data)) {
        warnx("Sent only %zu of %zu bytes to thing", (size_t)count,
            sizeof(sensor_data));
        abort();
    }
    print_sensor_data(&sensor_data);

    fputs("manager <-- thing ...\n", stdout);
    struct thruster_data thruster_data;
    count = recv(sv.sival_int, &thruster_data, sizeof(thruster_data),
        0);
    if (count == -1) {
        warn("recv() failed");
        abort();
    } else if (count == 0) {
        warnx("Thing disappeared");
        abort();
    } else if ((size_t)count < sizeof(thruster_data)) {
        warnx("Received only %zu of %zu bytes", (size_t)count,
            sizeof(thruster_data));
        abort();
    }
    print_thruster_data(&thruster_data);

    pthread_mutex_unlock(&mutex);
}


int init_socket()
{
    int listener = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listener == -1) {
        warn("Can't create socket");
        return -1;
    }

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        warn("Can't bind socket");
        return -1;
    }

    if (listen(listener, 1) == -1) {
        warn("Can't listen on socket");
        return -1;
    }

    fputs("Waiting for socket connection...\n", stdout);
    socklen_t socklen = sizeof(sa);
    int s = accept(listener, (struct sockaddr*)&sa, &socklen);
    if (s == -1) {
        warn("Can't accept connection on listener socket");
        return -1;
    }

    if (close(listener) == -1) {
        warn("Can't close listener socket");
        return -1;
    }

    if (unlink(SOCKET_FILENAME) == -1) {
        warn("Can't remove listener socket file");
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
        warn("Can't create timer");
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
    warnx("Dying...");
    exit(2);
}


int main()
{
    fputs("Initializing...\n", stdout);

    {
        struct sigaction act = {
            .sa_handler = die,
            .sa_flags = 0,
            .sa_restorer = NULL
        };
        if (sigemptyset(&act.sa_mask) == -1) {
            warn("Can't empty sa_mask");
            return 1;
        }

        if (sigaction(SIGABRT, &act, NULL) == -1) {
            warn("Can't set up SIGABRT handler");
            return 1;
        }

        if (sigaction(SIGINT, &act, NULL) == -1
                || sigaction(SIGQUIT, &act, NULL) == -1) {
            warn("Can't set up signal handlers");
            abort();
        }
    }

    int s = init_socket();
    if (s == -1)
        abort();

    pthread_mutex_init(&mutex, NULL);

    if (init_timer(s) == -1)
        abort();

    pause();
}
