#include "common.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


struct state* state;


int init_socket(int* mem)
{
    fputs("Connecting to socket...\n", stdout);

    int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        warn("Can't connect to socket");
        return -1;
    };

    fputs("Receiving shared memory object...\n", stdout);

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
    if (recvmsg(s, &mh, 0) == -1) {
        warn("Can't recvmsg() shared memory object");
        return -1;
    }
    struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
    *mem = *(int*)CMSG_DATA(cmh);

    return s;
}


int main()
{
    int mem;
    int s = init_socket(&mem);
    if (s == -1)
        return 1;

    state = mmap(NULL, sizeof(struct state),
        PROT_READ | PROT_WRITE, MAP_SHARED, mem, 0);
    if (state == NULL) {
        warn("Can't mmap() shared memory object");
        return 1;
    }

    state->thruster_data.ls = 20;
    state->thruster_data.rs = 20;
    state->thruster_data.fl = 800;
    state->thruster_data.fr = 300;
    state->thruster_data.bl = 800;
    state->thruster_data.br = 300;

    int i = 0;
    while (1) {
        fputs("worker <-- manager ...\n", stdout);

        if (pthread_mutex_lock(&state->worker_mutexes[0]) == -1) {
            warn("Can't lock worker mutex");
            return 1;
        }
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
            warn("Can't get current time");
            return 1;
        }
        now.tv_sec += 3;
        while (state->worker_misses[0] == 0) {
            if (pthread_cond_timedwait(&state->worker_conds[0],
                    &state->worker_mutexes[0], &now) == ETIMEDOUT
                    && getppid() == 1) {
                warn("Manager died");
                return 1;
            }
        }
        state->worker_misses[0] = 0;
        if (pthread_mutex_unlock(&state->worker_mutexes[0]) == -1) {
            warn("Can't unlock worker mutex");
            return 1;
        }

        fputs("    ", stdout);
        print_sensor_data(&state->sensor_data);

        if (i++ == 4) {
            fputs("Purposely spinning...\n", stdout);
            while (1) { __asm(""); }
        }

        ++state->thruster_data.ls;
        ++state->thruster_data.rs;
        ++state->thruster_data.fl;
        ++state->thruster_data.fr;
        ++state->thruster_data.bl;
        ++state->thruster_data.br;
    }
}
