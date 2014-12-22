#include "worker.h"

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


struct robot init()
{
    struct robot robot;

    int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1)
        err(1, "Can't connect to socket");

    char control[CMSG_SPACE(sizeof(int)) * 2];
    struct msghdr mh = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = NULL,
        .msg_iovlen = 0,
        .msg_control = control,
        .msg_controllen = CMSG_SPACE(sizeof(int)) * 2,
        .msg_flags = 0
    };
    if (recvmsg(s, &mh, 0) == -1)
        err(1, "Can't recvmsg() shared memory object");
    struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
    int data[2];
    if (cmh == NULL)
        errx(1, "No cmsghdr object received");
    memcpy(data, CMSG_DATA(cmh), sizeof(data));

    robot.state = mmap(NULL, sizeof(struct state),
        PROT_READ | PROT_WRITE, MAP_SHARED, data[0], 0);
    if (robot.state == NULL)
        err(1, "Can't mmap() robot state");

    robot.ctl = mmap(NULL, sizeof(struct state),
        PROT_READ | PROT_WRITE, MAP_SHARED, data[1], 0);
    if (robot.state == NULL)
        err(1, "Can't mmap() robot state");

    if (close(s) == -1)
        err(1, "Can't close socket");

    return robot;
}


struct sensor_data wait_for_sensor_data(struct robot* robot)
{
    fputs("test: About to lock notification mutex\n", stdout);
    if (pthread_mutex_lock(&robot->ctl->n_mutex) == -1)
        errx(1, "Can't lock notification mutex");
    fputs("test: Locked notification mutex\n", stdout);

    while (!robot->ctl->n) {
        alarm(5);
        fputs("test: Waiting for notification...\n", stdout);
        pthread_cond_wait(&robot->ctl->n_cond, &robot->ctl->n_mutex);
        alarm(0);
    }
    robot->ctl->n = false;

    fputs("<-- manager    ", stdout);
    print_sensor_data(&robot->state->sensor_data);

    struct sensor_data sd = robot->state->sensor_data;

    if (pthread_mutex_unlock(&robot->ctl->n_mutex) == -1)
        errx(1, "Can't unlock notification mutex");

    return sd;
}


void set_thruster_data(struct robot* robot, struct thruster_data* td)
{
    if (pthread_mutex_lock(&robot->state->thruster_data_mutex) == -1)
        errx(1, "Can't lock thruster data mutex");

    memcpy(&robot->state->thruster_data, td, sizeof(struct thruster_data));

    fputs("--> manager    ", stdout);
    print_thruster_data(&robot->state->thruster_data);

    if (pthread_mutex_unlock(&robot->state->thruster_data_mutex) == -1)
        errx(1, "Can't unlock thruster data mutex");
}
