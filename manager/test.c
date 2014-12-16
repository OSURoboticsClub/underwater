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


int init_socket()
{
    int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1)
        err(1, "Can't connect to socket");

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
    if (recvmsg(s, &mh, 0) == -1)
        err(1, "Can't recvmsg() shared memory object");
    struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
    if (cmh == NULL)
        errx(1, "No cmsghdr object received");
    int mem;
    memcpy(&mem, CMSG_DATA(cmh), sizeof(int));

    close(s);

    return mem;
}


int main()
{
    fputs("Connecting...\n", stdout);

    int mem = init_socket();
    if (mem == -1)
        return 1;

    state = mmap(NULL, sizeof(struct state),
        PROT_READ | PROT_WRITE, MAP_SHARED, mem, 0);
    if (state == NULL) {
        warn("Can't mmap() shared memory object");
        return 1;
    }

    fputs("Ready\n\n", stdout);

    while (1) {
        if (pthread_mutex_lock(&state->worker_mutexes[0]) == -1) {
            warnx("Can't lock worker mutex");
            return 1;
        }
        while (state->worker_misses[0] == 0) {
            alarm(5);
            pthread_cond_wait(&state->worker_conds[0],
                &state->worker_mutexes[0]);
            alarm(0);
        }
        state->worker_misses[0] = 0;
        if (pthread_mutex_unlock(&state->worker_mutexes[0]) == -1) {
            warnx("Can't unlock worker mutex");
            return 1;
        }
        fputs("<-- manager    ", stdout);
        print_sensor_data(&state->sensor_data);

        ++state->thruster_data.ls;
        ++state->thruster_data.rs;
        ++state->thruster_data.fl;
        ++state->thruster_data.fr;
        ++state->thruster_data.bl;
        ++state->thruster_data.br;
        fputs("--> manager    ", stdout);
        print_thruster_data(&state->thruster_data);
        fputs("\n", stdout);
    }
}
