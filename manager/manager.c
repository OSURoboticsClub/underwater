#define _POSIX_C_SOURCE 200112L

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "common.h"


static pthread_mutex_t mutex;


struct state* state;


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
    ++state->sensor_data.a;
    ++state->sensor_data.b;
    ++state->sensor_data.c;
    ++state->sensor_data.d;
    ++state->sensor_data.e;
    char ready = 1;
    ssize_t count = send(sv.sival_int, &ready, sizeof(ready),
        MSG_NOSIGNAL);
    if (count == -1) {
        if (errno == EPIPE) {
            warnx("Thing disappeared");
            abort();
        }
        warn("Can't communicate with thing");
        abort();
    } else if ((size_t)count < sizeof(ready)) {
        warnx("Sent only %zu of %zu bytes to thing", (size_t)count,
            sizeof(ready));
        abort();
    }
    fputs("    ", stdout);
    print_sensor_data(&state->sensor_data);

    fputs("manager <-- thing ...\n", stdout);
    count = recv(sv.sival_int, &ready, sizeof(ready), 0);
    if (count == -1) {
        warn("recv() failed");
        abort();
    } else if (count == 0) {
        warnx("Thing disappeared");
        abort();
    } else if ((size_t)count < sizeof(ready)) {
        warnx("Received only %zu of %zu bytes", (size_t)count,
            sizeof(ready));
        abort();
    } else if (ready != 1) {
        warnx("ready isn't 1");
        abort();
    }

    fputs("    ", stdout);
    print_thruster_data(&state->thruster_data);

    putchar('\n');

    pthread_mutex_unlock(&mutex);
}


int init_socket(int mem)
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

    char control[CMSG_SPACE(sizeof(int)) * 1];  // one cmsg with one int
    struct msghdr mh = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = NULL,
        .msg_iovlen = 0,
        .msg_control = control,
        .msg_controllen = CMSG_SPACE(sizeof(int)) * 1,
        .msg_flags = 0
    };
    struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
    cmh->cmsg_len = CMSG_LEN(sizeof(int));
    cmh->cmsg_level = SOL_SOCKET;
    cmh->cmsg_type = SCM_RIGHTS;
    *CMSG_DATA(cmh) = mem;
    ssize_t count = sendmsg(s, &mh, 0);
    if (count == -1) {
        warn("Can't sendmsg() shared memory object");
        return -1;
    };

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

    int mem = shm_open("/robot", O_RDWR | O_CREAT, 0);
    if (mem == -1) {
        warn("Can't open shared memory object");
        abort();
    }
    if (shm_unlink("/robot") == -1) {
        warn("Can't unlink shared memory object");
        abort();
    }
    if (ftruncate(mem, sizeof(struct state)) == -1) {
        warn("Can't set size of shared memory object");
        abort();
    }
    state = mmap(NULL, sizeof(struct state),
        PROT_READ | PROT_WRITE, MAP_SHARED, mem, 0);
    if (state == NULL) {
        warn("Can't mmap() shared memory object");
        abort();
    }

    state->sensor_data.a = 6000;
    state->sensor_data.b = 12345;
    state->sensor_data.c = 196;
    state->sensor_data.d = 3.14159;
    state->sensor_data.e = 1234567;

    int s = init_socket(mem);
    if (s == -1)
        abort();

    pthread_mutex_init(&mutex, NULL);

    if (init_timer(s) == -1)
        abort();

    pause();
}
