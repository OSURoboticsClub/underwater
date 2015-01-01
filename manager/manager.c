#include "common.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define WORKER_COUNT 2


struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};


enum worker_state {
    DEAD,
    CONNECTING,
    RUNNING
};


struct worker {
    pid_t pid;
    char** argv;

    pthread_mutex_t mutex;

    enum worker_state state;  // worker state
    int t;  // ticks since last ack

    int ctl_fd;
    struct worker_control* ctl;
};


pthread_mutex_t listener_mutex;
struct pollfd listener_poll;  // poll() argument


static struct state* state;
static int state_fd;


static pthread_mutexattr_t mutex_pshared;
static pthread_condattr_t cond_pshared;


static void die()
{
    unlink(SOCKET_FILENAME);  // Ignore errors.
    warnx("Dying...");
}


static void notify_workers(union sigval sv)
{
    int r;  // temporary return value holder

    // Reap any zombie workers.
    while (waitpid(-1, NULL, WNOHANG) > 0)
        fputs("Reaped worker\n", stdout);

    struct worker* workers = sv.sival_ptr;
    for (int i = 0; i <= WORKER_COUNT - 1; ++i) {
        struct worker* worker = &workers[i];

        r = pthread_mutex_trylock(&worker->mutex);
        if (r == EBUSY) {
            warnx("communicate() is slow");
            continue;
        } else if (r != 0) {
            errx(1, "Can't lock worker mutex");
        }

        if (worker->state == DEAD) {
            // Prevent other workers from being launched.
            r = pthread_mutex_trylock(&listener_mutex);
            if (r == EBUSY)
                goto worker_end;
            else if (r != 0)
                errx(1, "Can't lock listener mutex");

            // Launch the worker.
            worker->pid = fork();
            if (worker->pid == 0) {
                if (execv(workers[i].argv[0], workers[i].argv) == -1)
                    err(1, "Can't exec worker");
            } else if (worker->pid == -1) {
                err(1, "Can't fork");
            }

            worker->t = 0;
            worker->state = CONNECTING;

            // Initialize the notification system.
            pthread_mutex_init(&worker->ctl->n_mutex, &mutex_pshared);
            pthread_cond_init(&worker->ctl->n_cond, &cond_pshared);
            worker->ctl->n = false;
        } else if (worker->state == CONNECTING) {
            // Has worker connected yet?
            r = poll(&listener_poll, 1, 0);
            if (r == -1) {
                err(1, "Can't poll on listener");
            } else if (r == 0) {
                ++worker->t;
                warnx("Worker %d is slow to connect (t = %d)", i, worker->t);
            } else {
                struct sockaddr_un sa;
                sa.sun_family = AF_UNIX;
                strcpy(sa.sun_path, SOCKET_FILENAME);
                socklen_t socklen = sizeof(sa);
                int sock = accept(listener_poll.fd, (struct sockaddr*)&sa,
                    &socklen);
                // Let other workers launch.
                if (pthread_mutex_unlock(&listener_mutex) == -1)
                    errx(1, "Can't unlock listener mutex");
                if (sock == -1) {
                    warnx("Can't accept connection on listener socket");
                    worker->state = DEAD;
                    goto worker_end;
                }

                // Put shared memory file descriptors in a control message.
                int data[2] = {state_fd, worker->ctl_fd};
                char control[CMSG_SPACE(sizeof(int))];
                struct msghdr mh = {
                    .msg_namelen = 0,
                    .msg_iovlen = 0,
                    .msg_control = control,
                    .msg_controllen = sizeof(control),
                    .msg_flags = 0
                };
                struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
                if (cmh == NULL)
                    errx(1, "Can't get first cmsghdr object");
                cmh->cmsg_len = CMSG_LEN(sizeof(data));
                cmh->cmsg_level = SOL_SOCKET;
                cmh->cmsg_type = SCM_RIGHTS;
                memcpy(CMSG_DATA(cmh), &data, sizeof(data));

                // Send the control message to the worker.
                ssize_t count = sendmsg(sock, &mh, 0);
                if (count == -1) {
                    warn("Can't send shared memory object");
                    worker->state = DEAD;
                } else {
                    worker->t = 0;
                    worker->state = RUNNING;
                }

                if (close(sock) == -1) {
                    err(1, "Can't close worker socket");
                }
            }
        } else if (worker->state == RUNNING) {
            // Try to notify the worker.
            r = pthread_mutex_trylock(&worker->ctl->n_mutex);
            if (r == EBUSY) {
                ++worker->t;
                warnx("Worker %d is slow to ack (t = %d)", i, worker->t);
                goto worker_end;
            } else if (r != 0) {
                errx(1, "Can't lock worker notification mutex");
            } else if (worker->ctl->n) {
                ++worker->t;
                warnx("Worker %d is slow to ack (t = %d)", i, worker->t);
            } else {
                worker->t = 0;
                worker->ctl->n = true;
                fputs("Notifying worker...\n", stdout);
                pthread_cond_signal(&worker->ctl->n_cond);
            }

            if (pthread_mutex_unlock(&worker->ctl->n_mutex) == -1)
                errx(1, "Can't unlock worker notification mutex");
        }

worker_end:
        // Kill the worker if it's slow.
        if (worker->t >= 5) {
            if (worker->state == CONNECTING) {
                if (pthread_mutex_unlock(&listener_mutex) != 0)
                    errx(1, "Can't unlock listener mutex");
            }

            printf("Killing worker %d...\n", i);
            if (kill(worker->pid, SIGKILL) == -1)
                err(1, "Can't kill worker");

            if (pthread_mutex_destroy(&worker->ctl->n_mutex) != 0)
                errx(1, "Can't destroy notification mutex");
            if (pthread_cond_destroy(&worker->ctl->n_cond) != 0)
                errx(1, "Can't destroy notification condition variable");

            worker->state = DEAD;
        }

        if (pthread_mutex_unlock(&worker->mutex) != 0)
            err(1, "Can't unlock worker mutex");
    }
}


static void init_attrs()
{
    if (pthread_mutexattr_init(&mutex_pshared) != 0)
        errx(1, "Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&mutex_pshared, PTHREAD_PROCESS_SHARED)
            != 0)
        errx(1, "Can't set mutexattr to process-shared");

    if (pthread_condattr_init(&cond_pshared) != 0)
        errx(1, "Can't initialize condition variable attributes object");
    if (pthread_condattr_setpshared(&cond_pshared, PTHREAD_PROCESS_SHARED)
            != 0)
        errx(1, "Can't set condattr to process-shared");
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

    pthread_mutex_init(&state->sensor_data_mutex, &ma);
    pthread_mutex_init(&state->thruster_data_mutex, &ma);

    if (pthread_mutexattr_destroy(&ma) != 0)
        errx(1, "Can't destroy mutex attributes object");

    return mem;
}


static void init_socket()
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

    listener_poll.fd = listener;
    listener_poll.events = POLLIN;
}


static void init_workers(struct worker workers[])
{
    for (int i = 0; i <= WORKER_COUNT - 1; ++i) {
        struct worker* worker = &workers[i];

        pthread_mutex_init(&worker->mutex, NULL);
        worker->state = DEAD;
        worker->t = 0;

        worker->ctl_fd = shm_open("/robot", O_RDWR | O_CREAT, 0);
        if (worker->ctl_fd == -1)
            err(1, "Can't open shared memory object");
        if (shm_unlink("/robot") == -1)
            err(1, "Can't unlink shared memory object");
        if (ftruncate(worker->ctl_fd, sizeof(struct worker_control)) == -1)
            err(1, "Can't set size of shared memory object");
        worker->ctl = mmap(NULL, sizeof(struct worker_control),
            PROT_READ | PROT_WRITE, MAP_SHARED, worker->ctl_fd, 0);
        if (worker->ctl == NULL)
            err(1, "Can't mmap() shared memory object");
    }
}


static void init_timer(struct worker workers[])
{
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = workers,
        .sigev_notify_function = notify_workers,
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

    init_attrs();

    init_signals();
    state_fd = init_state();

    fputs("Starting workers...\n", stdout);

    static char* worker1_argv[] = {"./test", NULL};
    static char* worker2_argv[] = {"./test", NULL};
    static struct worker workers[WORKER_COUNT] = {
        {.argv = worker1_argv},
        {.argv = worker2_argv}
    };

    init_socket();
    init_workers(workers);

    fputs("Setting up timer...\n", stdout);

    init_timer(workers);

    fputs("Ready\n\n", stdout);

    pause();
}
