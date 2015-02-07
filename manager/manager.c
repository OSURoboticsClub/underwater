#include "common.h"
#include "error.h"

#include <assert.h>
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


#define warning(msg) warning_helper(prog_name, msg)
#define error(msg) error_helper(prog_name, msg)
#define warning_e(msg) warning_e_helper(prog_name, msg, errno)
#define error_e(msg) error_e_helper(prog_name, msg, errno)


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

    bool do_heartbeat;

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


static char* prog_name;


static pthread_mutex_t listener_mutex;
static struct pollfd listener_poll;  // poll() argument


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
    warning("About to die");

    if (pthread_mutex_lock(&timer_mutex) != 0) {
        warning("Can't lock timer mutex");
        return;
    }

    should_quit = true;
    if (pthread_cond_signal(&quit_cond) != 0)
        warning("Can't signal on quit condition variable");

    if (pthread_mutex_unlock(&timer_mutex) != 0)
        warning("Can't unlock timer mutex");
}


static void thread_error(char* str)
{
    warning(str);
    die();
    pthread_exit(NULL);
}


static void thread_error_e(char* str)
{
    warning_e(str);
    die();
    pthread_exit(NULL);
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
            warning("communicate() is slow");
            continue;
        } else if (r != 0) {
            thread_error("Can't lock worker mutex");
        }

        if (worker->state == DEAD) {
            // Prevent other workers from being launched.
            r = pthread_mutex_trylock(&listener_mutex);
            if (r == EBUSY)
                goto worker_end;
            else if (r != 0)
                thread_error("Can't lock listener mutex");

            // Launch the worker.
            worker->pid = fork();
            if (worker->pid == 0) {
                if (execv(worker->argv[0], worker->argv) == -1)
                    // We can't call thread_error here, since the main thread
                    // is in the parent process.
                    warning_e("Can't exec worker");
            } else if (worker->pid == -1) {
                thread_error_e("Can't fork");
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
                thread_error_e("Can't poll on listener");
            } else if (r == 0) {
                ++worker->t;
                printf("Worker %d is slow to connect (t = %d)\n", i,
                    worker->t);
            } else {
                struct sockaddr_un sa;
                sa.sun_family = AF_UNIX;
                strcpy(sa.sun_path, SOCKET_FILENAME);
                socklen_t socklen = sizeof(sa);
                int sock = accept(listener_poll.fd, (struct sockaddr*)&sa,
                    &socklen);
                // Let other workers launch.
                if (pthread_mutex_unlock(&listener_mutex) == -1)
                    thread_error("Can't unlock listener mutex");
                if (sock == -1) {
                    warning("Can't accept connection on listener socket");
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
                    thread_error("Can't get first cmsghdr object");
                cmh->cmsg_len = CMSG_LEN(sizeof(data));
                cmh->cmsg_level = SOL_SOCKET;
                cmh->cmsg_type = SCM_RIGHTS;
                memcpy(CMSG_DATA(cmh), &data, sizeof(data));

                // Send the control message to the worker.
                ssize_t count = sendmsg(sock, &mh, 0);
                if (count == -1) {
                    warning_e("Can't send shared memory object");
                    worker->state = DEAD;
                } else {
                    worker->t = 0;
                    worker->state = RUNNING;
                }

                if (close(sock) == -1) {
                    thread_error_e("Can't close worker socket");
                }
            }
        } else if (worker->state == RUNNING) {
            // Try to notify the worker.
            r = pthread_mutex_trylock(&worker->ctl->n_mutex);
            if (r == EBUSY) {
                ++worker->t;
                printf("Worker %d is slow to ack (t = %d)\n", i,
                    worker->t);
                goto worker_end;
            } else if (r != 0) {
                thread_error("Can't lock worker notification mutex");
            } else if (worker->do_heartbeat && worker->ctl->n) {
                ++worker->t;
                printf("Worker %d is slow to ack (t = %d)\n", i,
                    worker->t);
            } else {
                worker->t = 0;
                worker->ctl->n = true;
                printf("Notifying worker %d...\n", i);
                pthread_cond_signal(&worker->ctl->n_cond);
            }

            if (pthread_mutex_unlock(&worker->ctl->n_mutex) == -1)
                thread_error("Can't unlock worker notification mutex");
        }

worker_end:
        // Kill the worker if it's slow.
        if (worker->t >= 5) {
            if (worker->state == CONNECTING) {
                if (pthread_mutex_unlock(&listener_mutex) != 0)
                    thread_error("Can't unlock listener mutex");
            }

            printf("Killing worker %d...\n", i);
            if (kill(worker->pid, SIGKILL) == -1)
                thread_error_e("Can't kill worker");

            if (pthread_mutex_destroy(&worker->ctl->n_mutex) != 0)
                thread_error("Can't destroy notification mutex");
            if (pthread_cond_destroy(&worker->ctl->n_cond) != 0)
                thread_error("Can't destroy notification condition variable");

            worker->state = DEAD;
        }

        if (pthread_mutex_unlock(&worker->mutex) != 0)
            thread_error("Can't unlock worker mutex");
    }
}


static void init_attrs()
{
    if (pthread_mutexattr_init(&mutex_pshared) != 0)
        error("Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&mutex_pshared, PTHREAD_PROCESS_SHARED)
            != 0)
        error("Can't set mutexattr to process-shared");

    if (pthread_condattr_init(&cond_pshared) != 0)
        error("Can't initialize condition variable attributes object");
    if (pthread_condattr_setpshared(&cond_pshared, PTHREAD_PROCESS_SHARED)
            != 0)
        error("Can't set condattr to process-shared");
}


static void init_signals()
{
    struct sigaction act = {
        .sa_handler = die,
        .sa_flags = 0
    };
    if (sigemptyset(&act.sa_mask) == -1)
        error("Can't empty sa_mask");

    if (sigaction(SIGINT, &act, NULL) == -1)
        error("Can't set up SIGINT handler");
    if (sigaction(SIGQUIT, &act, NULL) == -1)
        error("Can't set up SIGQUIT handler");
}


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
        error("Can't initialize mutex attributes object");
    if (pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED) != 0)
        error("Can't set mutex to process-shared");

    pthread_mutex_init(&state->sensor_data_mutex, &ma);
    pthread_mutex_init(&state->thruster_data_mutex, &ma);

    if (pthread_mutexattr_destroy(&ma) != 0)
        error("Can't destroy mutex attributes object");

    return mem;
}


static void init_socket()
{
    int listener = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listener == -1)
        error_e("Can't create socket");

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);  // Ignore errors.
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa)) == -1)
        error_e("Can't bind socket");

    if (listen(listener, 1) == -1)
        error_e("Can't listen on socket");

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
            error_e("Can't open shared memory object");
        if (shm_unlink("/robot") == -1)
            error_e("Can't unlink shared memory object");
        if (ftruncate(worker->ctl_fd, sizeof(struct worker_ctl)) == -1)
            error_e("Can't set size of shared memory object");
        worker->ctl = mmap(NULL, sizeof(struct worker_ctl),
            PROT_READ | PROT_WRITE, MAP_SHARED, worker->ctl_fd, 0);
        if (worker->ctl == NULL)
            error_e("Can't mmap() shared memory object");
    }
}


static void init_timer(struct worker_group* group)
{
    if (pthread_mutex_init(&timer_mutex, NULL) != 0)
        error("Can't initialize timer mutex");

    if (pthread_cond_init(&quit_cond, NULL) != 0)
        error("Can't initialize quit condition variable");

    if (pthread_mutex_lock(&timer_mutex) != 0)
        error("Can't lock timer mutex");

    should_quit = false;

    pthread_attr_t pa;
    if (pthread_attr_init(&pa) != 0)
        error("Can't initialize pthread_attr");
    if (pthread_attr_setdetachstate(&pa, PTHREAD_CREATE_DETACHED) != 0)
        error("Can't configure pthread_attr");

    // When timer expires, call notify_worker(group) in a new thread.
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = group,
        .sigev_notify_function = notify_workers,
        .sigev_notify_attributes = &pa,
    };

    // Create timer.
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1)
        error_e("Can't create timer");

    if (pthread_attr_destroy(&pa) != 0)
        error_e("Can't destroy pthread_attr");

    struct itimerspec its;
    its.it_interval.tv_sec = 1;  // every second
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;  // arm timer
    its.it_value.tv_nsec = 1;

    // Start timer.
    if (timer_settime(timerid, 0, &its, NULL) == -1)
        error_e("Can't arm timer");

    fputs("Ready\n\n", stdout);

    // Wait until a thread tells us to quit.
    while (!should_quit) {
        pthread_cond_wait(&quit_cond, &timer_mutex);
    }

    warning("Dying...");

    if (timer_delete(timerid) == -1) {
        warning_e("Can't delete timer");
    }

    pthread_mutex_unlock(&timer_mutex);  // Ignore errors.

    unlink(SOCKET_FILENAME);  // Ignore errors.
}


void init_manager(char* name, int worker_count, char*** argvv,
        bool do_heartbeat[])
{
    fputs("Initializing...\n", stdout);

    prog_name = name;

    init_attrs();

    init_signals();
    state_fd = init_state();

    fputs("Starting workers...\n", stdout);

    static struct worker_group group;
    group.count = worker_count;
    group.workers = malloc(sizeof(struct worker) * worker_count);
    for (int i = 0; i <= worker_count - 1; ++i) {
        group.workers[i].argv = argvv[i];
        group.workers[i].do_heartbeat = do_heartbeat[i];
    }

    init_socket();
    init_workers(&group);

    fputs("Setting up timer...\n", stdout);

    init_timer(&group);
}
