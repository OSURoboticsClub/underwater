#include "common.h"

#include <assert.h>
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


#define WORKER_COUNT 2


struct worker {
    int pid;
    char** argv;
};


struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};


static pthread_mutex_t arduino_mutex;
static struct state* state;


static void die()
{
    unlink(SOCKET_FILENAME);  // Ignore errors.
    warnx("Dying...");
}


static void communicate(union sigval sv)
{
    // Communicate with Arduino.

    int mutex_status = pthread_mutex_trylock(&arduino_mutex);
    if (mutex_status != 0) {
        if (mutex_status == EBUSY)
            errx(1, "Arduino is slow");
        errx(1, "Can't lock Arduino mutex");
    }

    if (pthread_mutex_lock(&state->thruster_data_mutex) != 0)
        errx(1, "Can't lock thruster data mutex");
    fputs("--> Arduino (fake)    ", stdout);
    print_thruster_data(&state->thruster_data);
    if (pthread_mutex_unlock(&state->thruster_data_mutex) != 0)
        errx(1, "Can't unlock thruster data mutex");

    if (pthread_mutex_lock(&state->sensor_data_mutex) == -1)
        errx(1, "Can't lock sensor data mutex");
    ++state->sensor_data.a;
    ++state->sensor_data.b;
    ++state->sensor_data.c;
    ++state->sensor_data.d;
    ++state->sensor_data.e;
    fputs("<-- Arduino (fake)    ", stdout);
    print_sensor_data(&state->sensor_data);
    if (pthread_mutex_unlock(&state->sensor_data_mutex) == -1)
        errx(1, "Can't unlock sensor data mutex");

    if (pthread_mutex_unlock(&arduino_mutex) != 0)
        errx(1, "Can't unlock Arduino mutex");

    // Notify workers.

    /*struct worker* workers = sv.sival_ptr;*/

    if (pthread_mutex_lock(&state->worker_mutexes[0]) == -1)
        errx(1, "Can't lock worker mutex");

    if (state->worker_misses[0] >= 4)
        errx(1, "Worker is too slow");
    else if (state->worker_misses[0] > 0)
        warnx("Worker is slow (%d notifications not acked)",
            state->worker_misses[0]);

    ++state->worker_misses[0];
    pthread_cond_signal(&state->worker_conds[0]);  // Always succeeds.

    if (pthread_mutex_unlock(&state->worker_mutexes[0]) == -1)
        errx(1, "Can't unlock worker mutex");

    if (pthread_mutex_lock(&state->sensor_data_mutex) == -1)
        errx(1, "Can't lock sensor data mutex");
    fputs("--> worker    ", stdout);
    print_sensor_data(&state->sensor_data);
    if (pthread_mutex_unlock(&state->sensor_data_mutex) == -1)
        errx(1, "Can't unlock sensor data mutex");

    putchar('\n');
}


static void init_signals()
{
    struct sigaction act = {
        .sa_handler = die,
        .sa_flags = 0
    };
    if (sigemptyset(&act.sa_mask) == -1)
        err(1, "Can't empty sa_mask");

    if (sigaction(SIGINT, &act, NULL) == -1)
        err(1, "Can't set up SIGINT handler");
    if (sigaction(SIGQUIT, &act, NULL) == -1)
        err(1, "Can't set up SIGQUIT handler");
}


static int init_state()
{
    int mem = shm_open("/robot", O_RDWR | O_CREAT, 0);
    if (mem == -1)
        err(1, "Can't open shared memory object");
    if (shm_unlink("/robot") == -1)
        err(1, "Can't unlink shared memory object");
    if (ftruncate(mem, sizeof(struct state)) == -1)
        err(1, "Can't set size of shared memory object");
    state = mmap(NULL, sizeof(struct state), PROT_READ | PROT_WRITE,
        MAP_SHARED, mem, 0);
    if (state == NULL)
        err(1, "Can't mmap() shared memory object");

    state->sensor_data.a = 6000;
    state->sensor_data.b = 12345;
    state->sensor_data.c = 196;
    state->sensor_data.d = 3.14159;
    state->sensor_data.e = 1234567;

    pthread_mutexattr_t ma;
    if (pthread_mutexattr_init(&ma) != 0)
        errx(1, "Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED) != 0)
        errx(1, "Can't set mutex to process-shared");
    pthread_mutex_init(&state->worker_mutexes[0], &ma);  // Always suceeds.
    if (pthread_mutexattr_destroy(&ma) != 0)
        errx(1, "Can't destroy mutex attributes object");

    pthread_condattr_t ca;
    if (pthread_condattr_init(&ca) != 0)
        errx(1, "Can't initialize condition variable attributes object");
    if (pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED) != 0)
        errx(1, "Can't set condition variable to process-shared");
    pthread_cond_init(&state->worker_conds[0], &ca);  // Always succeeds.
    if (pthread_condattr_destroy(&ca) != 0)
        errx(1, "Can't destory condition variable attributes object");

    state->worker_misses[0] = 0;

    return mem;
}


static void start_workers(struct worker workers[], int mem)
{
    int listener = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listener == -1)
        err(1, "Can't create socket");

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);  // Ignore errors.
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa)) == -1)
        err(1, "Can't bind socket");

    if (listen(listener, 1) == -1)
        err(1, "Can't listen on socket");

    for (int i = 0; i <= WORKER_COUNT - 1; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            if (execv(workers[i].argv[0], workers[i].argv) == -1)
                err(1, "Can't exec worker");
        } else if (pid == -1) {
            err(1, "Can't fork");
        }

        socklen_t socklen = sizeof(sa);
        int s = accept(listener, (struct sockaddr*)&sa, &socklen);
        if (s == -1)
            err(1, "Can't accept connection on listener socket");

        struct ucred cred;
        socklen_t cred_len = sizeof(cred);
        if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len)
                == -1)
            err(1, "Can't get credentials");
        workers[i].pid = cred.pid;

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
        memcpy(CMSG_DATA(cmh), &mem, sizeof(int));
        ssize_t count = sendmsg(s, &mh, 0);
        if (count == -1)
            err(1, "Can't send shared memory object");

        if (close(s) == -1)
            err(1, "Can't close worker socket");
    }

    if (close(listener) == -1)
        err(1, "Can't close listener socket");

    if (unlink(SOCKET_FILENAME) == -1)
        err(1, "Can't remove listener socket file");
}


static void init_timer(struct worker workers[])
{
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = workers,
        .sigev_notify_function = communicate,
        .sigev_notify_attributes = NULL
    };
    timer_t timerid;
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1)
        err(1, "Can't create timer");

    struct itimerspec its;
    its.it_interval.tv_sec = 1;  // every second
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;  // arm timer
    its.it_value.tv_nsec = 1;
    if (timer_settime(timerid, 0, &its, NULL) == -1)
        err(1, "Can't arm timer");
}


int main()
{
    fputs("Initializing...\n", stdout);

    if (atexit(die) != 0)
        err(1, "Can't set die() to be called at exit");

    init_signals();
    int mem = init_state();

    fputs("Starting workers...\n", stdout);

    static char* worker1_argv[] = {"./test", NULL};
    static char* worker2_argv[] = {"./test", NULL};
    static struct worker workers[WORKER_COUNT] = {
        {.argv = worker1_argv},
        {.argv = worker2_argv}
    };

    start_workers(workers, mem);

    fputs("Setting up timer...\n", stdout);

    pthread_mutex_init(&arduino_mutex, NULL);

    pthread_mutexattr_t ma;
    if (pthread_mutexattr_init(&ma) != 0)
        errx(1, "Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED) != 0)
        errx(1, "Can't set mutex to process-shared");
    pthread_mutex_init(&state->sensor_data_mutex, &ma);
    pthread_mutex_init(&state->thruster_data_mutex, &ma);
    if (pthread_mutexattr_destroy(&ma) != 0)
        errx(1, "Can't destroy mutex attributes object");

    init_timer(workers);

    fputs("Ready\n\n", stdout);

    pause();
}
