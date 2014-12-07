//#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


int main()
{
    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "socket");

    if (connect(server, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "test: ERROR: Could not connect to socket (%s)\n",
            strerror(errno));
        return 1;
    };

    //sleep(1);

    char controlbuf[1000];
    struct msghdr mh = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = NULL,
        .msg_iovlen = 0,
        .msg_control = controlbuf,
        .msg_controllen = sizeof(controlbuf),
        .msg_flags = 0
    };
    ssize_t received = recvmsg(server, &mh, 0);
    if (received == -1) {
        fprintf(stderr, "test: ERROR: recvmsg() failed (%s)\n",
            strerror(errno));
        return 1;
    }
    printf("%d bytes of ancillary data received\n", (int)received);
    printf("Length of control message buffer is %d\n", (int)mh.msg_controllen);
    assert(mh.msg_controllen == sizeof(controlbuf));

    struct cmsghdr* cmh = CMSG_FIRSTHDR(&mh);
    int a = *(int*)CMSG_DATA(cmh);
    close(a);

    char buf;
    while (1) {
        ssize_t received = recv(server, &buf, 1, 0);
        if (received == -1) {
            fprintf(stderr, "test: ERROR: recv() failed (%s)\n",
                strerror(errno));
            return 1;
        }
        if (received != 1) {
            fputs("Server disappeared.\n", stdout);
            return 0;
        }
        putchar(buf);
        putchar('\n');
    }
}
