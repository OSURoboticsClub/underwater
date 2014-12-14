#include "common.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>


struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};


struct worker {
    int sock;
    int pid;
};


static pthread_mutex_t arduino_mutex;
struct state* state;


void communicate(union sigval sv)
{
    // Communicate with Arduino.

    int mutex_status = pthread_mutex_trylock(&arduino_mutex);
    if (mutex_status != 0) {
        if (mutex_status == EBUSY) {
            warnx("Arduino is slow");
            abort();
        }
        warnx("Can't lock Arduino mutex");
        abort();
    }

    fputs("    ", stdout);
    print_thruster_data(&state->thruster_data);

    fputs("manager --> Arduino (fake) ...\n", stdout);
    fputs("manager <-- Arduino (fake) ...\n", stdout);
    ++state->sensor_data.a;
    ++state->sensor_data.b;
    ++state->sensor_data.c;
    ++state->sensor_data.d;
    ++state->sensor_data.e;

    fputs("    ", stdout);
    print_sensor_data(&state->sensor_data);

    if (pthread_mutex_unlock(&arduino_mutex) != 0) {
        warnx("Can't unlock Arduino mutex");
        abort();
    }

    // Notify workers.

    /*struct worker* workers = sv.sival_ptr;*/

    fputs("manager --> worker ...\n", stdout);

    if (pthread_mutex_lock(&state->worker_mutexes[0]) == -1) {
        warnx("Can't lock worker mutex");
        abort();
    }
    if (state->worker_misses[0] >= 4) {
        warnx("Worker is too slow");
        abort();
    } else if (state->worker_misses[0] > 0) {
        warnx("Worker is slow (%d notifications not acked)",
            state->worker_misses[0]);
    }
    ++state->worker_misses[0];
    pthread_cond_signal(&state->worker_conds[0]);  // Never fails.
    if (pthread_mutex_unlock(&state->worker_mutexes[0]) == -1) {
        warnx("Can't release worker mutex");
        abort();
    }

    putchar('\n');
}


int init_socket(struct worker worker[], int mem)
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
    worker[0].sock = accept(listener, (struct sockaddr*)&sa, &socklen);
    if (worker[0].sock == -1) {
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

    struct ucred cred;
    socklen_t cred_len = sizeof(cred);
    if (getsockopt(worker[0].sock, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len)
            == -1) {
        warn("Can't get credentials");
        return -1;
    }
    worker[0].pid = cred.pid;

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
    ssize_t count = sendmsg(worker[0].sock, &mh, 0);
    if (count == -1) {
        warn("Can't sendmsg() shared memory object");
        return -1;
    };

    pthread_mutex_init(&state->worker_mutexes[0], NULL);  // Always suceeds.
    pthread_cond_init(&state->worker_conds[0], NULL);  // Always succeeds.

    return 0;
}


int init_timer(struct worker workers[])
{
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = workers,
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
            .sa_flags = 0
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

    struct worker workers[1];

    if (init_socket(workers, mem) == -1)
        abort();

    pthread_mutex_init(&arduino_mutex, NULL);

    if (init_timer(workers) == -1)
        abort();

    pause();
}
