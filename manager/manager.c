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

#define error(str) do { warn(str); die(); } while (0)
#define errorx(str) do { warnx(str); die(); } while (0)


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
    struct worker_ctl* ctl;
};


struct worker_group {
    int count;
    struct worker* workers;
};


pthread_mutex_t listener_mutex;
struct pollfd listener_poll;  // poll() argument


static struct state* state;
static int state_fd;


static pthread_mutex_t timer_mutex;
static timer_t timerid;
static pthread_cond_t quit_cond;
static bool should_quit;


static pthread_mutexattr_t mutex_pshared;
static pthread_condattr_t cond_pshared;


static void die()
{
    unlink(SOCKET_FILENAME);  // Ignore errors.
    warnx("About to die");

    if (pthread_mutex_lock(&timer_mutex) != 0) {
        warnx("Can't lock timer mutex");
        return;
    }

    printf("timerid = %p\n", timerid);
    if (timer_delete(timerid) == -1) {
        warn("Can't delete timer");
        goto die_end;
    }
    should_quit = true;
    if (pthread_cond_signal(&quit_cond) != 0)
        warnx("Can't signal on quit condition variable");

die_end:
    if (pthread_mutex_unlock(&timer_mutex) != 0)
        warnx("Can't unlock timer mutex");

    pthread_exit(0);
}


static void notify_workers(union sigval sv)
{
    int r;  // temporary return value holder

    // Reap any zombie workers.
    while (waitpid(-1, NULL, WNOHANG) > 0)
        fputs("Reaped worker\n", stdout);

    struct worker_group* group = sv.sival_ptr;
    for (int i = 0; i <= group->count - 1; ++i) {
        struct worker* worker = &group->workers[i];

        r = pthread_mutex_trylock(&worker->mutex);
        if (r == EBUSY) {
            warnx("communicate() is slow");
            continue;
        } else if (r != 0) {
            errorx("Can't lock worker mutex");
        }

        if (worker->state == DEAD) {
            // Prevent other workers from being launched.
            r = pthread_mutex_trylock(&listener_mutex);
            if (r == EBUSY)
                goto worker_end;
            else if (r != 0)
                errorx("Can't lock listener mutex");

            // Launch the worker.
            worker->pid = fork();
            if (worker->pid == 0) {
                if (execv(worker->argv[0], worker->argv) == -1)
                    error("Can't exec worker");
            } else if (worker->pid == -1) {
                error("Can't fork");
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
                error("Can't poll on listener");
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
                    errorx("Can't unlock listener mutex");
                if (sock == -1) {
                    warnx("Can't accept connection on listener socket");
                    worker->state = DEAD;
                    goto worker_end;
                }

                // Put shared memory file descriptors in a control message.
                int data[2] = {state_fd, worker->ctl_fd};
                char control[CMSG_SPACE(sizeof(data))];
                struct msghdr mh = {
                    .msg_namelen = 0,
                    .msg_iovlen = 0,
                    .msg_control = control,
                    .msg_controllen = sizeof(control),
                    .msg_flags = 0
                };
                struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
                if (cmh == NULL)
                    errorx("Can't get first cmsghdr object");
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
                    error("Can't close worker socket");
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
                errorx("Can't lock worker notification mutex");
            } else if (worker->ctl->n) {
                ++worker->t;
                warnx("Worker %d is slow to ack (t = %d)", i, worker->t);
            } else {
                worker->t = 0;
                worker->ctl->n = true;
                printf("Notifying worker %d...\n", i);
                pthread_cond_signal(&worker->ctl->n_cond);
            }

            if (pthread_mutex_unlock(&worker->ctl->n_mutex) == -1)
                errorx("Can't unlock worker notification mutex");
        }

worker_end:
        // Kill the worker if it's slow.
        if (worker->t >= 5) {
            if (worker->state == CONNECTING) {
                if (pthread_mutex_unlock(&listener_mutex) != 0)
                    errorx("Can't unlock listener mutex");
            }

            printf("Killing worker %d...\n", i);
            if (kill(worker->pid, SIGKILL) == -1)
                error("Can't kill worker");

            if (pthread_mutex_destroy(&worker->ctl->n_mutex) != 0)
                errorx("Can't destroy notification mutex");
            if (pthread_cond_destroy(&worker->ctl->n_cond) != 0)
                errorx("Can't destroy notification condition variable");

            worker->state = DEAD;
        }

        if (pthread_mutex_unlock(&worker->mutex) != 0)
            error("Can't unlock worker mutex");
    }
}


static void init_attrs()
{
    if (pthread_mutexattr_init(&mutex_pshared) != 0)
        errorx("Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&mutex_pshared, PTHREAD_PROCESS_SHARED)
            != 0)
        errorx("Can't set mutexattr to process-shared");

    if (pthread_condattr_init(&cond_pshared) != 0)
        errorx("Can't initialize condition variable attributes object");
    if (pthread_condattr_setpshared(&cond_pshared, PTHREAD_PROCESS_SHARED)
            != 0)
        errorx("Can't set condattr to process-shared");
}


/*
 *static void init_signals()
 *{
 *    struct sigaction act = {
 *        .sa_handler = die,
 *        .sa_flags = 0
 *    };
 *    if (sigemptyset(&act.sa_mask) == -1)
 *        error("Can't empty sa_mask");
 *
 *    if (sigaction(SIGINT, &act, NULL) == -1)
 *        error("Can't set up SIGINT handler");
 *    if (sigaction(SIGQUIT, &act, NULL) == -1)
 *        error("Can't set up SIGQUIT handler");
 *}
 */


static int init_state()
{
    int mem = shm_open("/robot", O_RDWR | O_CREAT, 0);
    if (mem == -1)
        error("Can't open shared memory object");
    if (shm_unlink("/robot") == -1)
        error("Can't unlink shared memory object");
    if (ftruncate(mem, sizeof(struct state)) == -1)
        error("Can't set size of shared memory object");
    state = mmap(NULL, sizeof(struct state), PROT_READ | PROT_WRITE,
        MAP_SHARED, mem, 0);
    if (state == NULL)
        error("Can't mmap() shared memory object");

    state->sensor_data.a = 6000;
    state->sensor_data.b = 12345;
    state->sensor_data.c = 196;
    state->sensor_data.d = 3.14159;
    state->sensor_data.e = 1234567;

    pthread_mutexattr_t ma;
    if (pthread_mutexattr_init(&ma) != 0)
        errorx("Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED) != 0)
        errorx("Can't set mutex to process-shared");

    pthread_mutex_init(&state->sensor_data_mutex, &ma);
    pthread_mutex_init(&state->thruster_data_mutex, &ma);

    if (pthread_mutexattr_destroy(&ma) != 0)
        errorx("Can't destroy mutex attributes object");

    return mem;
}


static void init_socket()
{
    int listener = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listener == -1)
        error("Can't create socket");

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);  // Ignore errors.
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa)) == -1)
        error("Can't bind socket");

    if (listen(listener, 1) == -1)
        error("Can't listen on socket");

    listener_poll.fd = listener;
    listener_poll.events = POLLIN;
}


static void init_workers(struct worker_group* group)
{
    for (int i = 0; i <= group->count - 1; ++i) {
        struct worker* worker = &group->workers[i];

        pthread_mutex_init(&worker->mutex, NULL);
        worker->state = DEAD;
        worker->t = 0;

        worker->ctl_fd = shm_open("/robot", O_RDWR | O_CREAT, 0);
        if (worker->ctl_fd == -1)
            error("Can't open shared memory object");
        if (shm_unlink("/robot") == -1)
            error("Can't unlink shared memory object");
        if (ftruncate(worker->ctl_fd, sizeof(struct worker_ctl)) == -1)
            error("Can't set size of shared memory object");
        worker->ctl = mmap(NULL, sizeof(struct worker_ctl),
            PROT_READ | PROT_WRITE, MAP_SHARED, worker->ctl_fd, 0);
        if (worker->ctl == NULL)
            error("Can't mmap() shared memory object");
    }
}


static void init_timer(struct worker_group* group)
{
    if (pthread_mutex_init(&timer_mutex, NULL) != 0) {
        warnx("Can't initialize timer mutex");
        exit(1);
    }

    if (pthread_cond_init(&quit_cond, NULL) != 0) {
        warnx("Can't initialize quit condition variable");
        exit(1);
    }

    if (pthread_mutex_lock(&timer_mutex) != 0) {
        warnx("Can't lock timer mutex");
        exit(1);
    }

    pthread_attr_t pa;
    if (pthread_attr_init(&pa) != 0)
        errorx("Can't initialize pthread_attr");
    if (pthread_attr_setdetachstate(&pa, PTHREAD_CREATE_DETACHED) != 0)
        errorx("Can't configure pthread_attr");
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = group,
        .sigev_notify_function = notify_workers,
        .sigev_notify_attributes = &pa,
    };

    // Create timer.
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1)
        error("Can't create timer");
    printf("timerid = %p\n", timerid);

    if (pthread_attr_destroy(&pa) != 0)
        errorx("Can't destroy pthread_attr");

    struct itimerspec its;
    its.it_interval.tv_sec = 1;  // every second
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;  // arm timer
    its.it_value.tv_nsec = 1;

    // Start timer.
    if (timer_settime(timerid, 0, &its, NULL) == -1)
        error("Can't arm timer");

    should_quit = false;

    fputs("Ready\n\n", stdout);

    while (!should_quit)
        pthread_cond_wait(&quit_cond, &timer_mutex);

    warnx("Dying...");

    pthread_mutex_unlock(&timer_mutex);  // Ignore errors.
}


void init_manager(int worker_count, char*** argvv)
{
    fputs("Initializing...\n", stdout);

    init_attrs();

    /*init_signals();*/
    state_fd = init_state();

    fputs("Starting workers...\n", stdout);

    static struct worker_group group;
    group.count = worker_count;
    group.workers = malloc(sizeof(struct worker) * worker_count);
    for (int i = 0; i <= worker_count - 1; ++i) {
        group.workers[i].argv = argvv[i];
    }

    init_socket();
    init_workers(&group);

    fputs("Setting up timer...\n", stdout);

    init_timer(&group);
}
