#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>


#define SOCKET_FILENAME "socket"
#define MAX_CLIENTS 8


int clients[MAX_CLIENTS];
int num_clients = 0;


void send_message(union sigval sv)
{
    fputs("Pretending to send message...\n", stdout);
    for (int i = 0; i <= num_clients - 1; ++i) {
        if (send(clients[i], ".", 1, MSG_NOSIGNAL) != 1) {
            fprintf(stderr, "serial: ERROR: Could not notify client %d (%s)\n",
                i, strerror(errno));
            abort();
        }
    }
}


int init_socket()
{
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener == -1) {
        fprintf(stderr, "serial: ERROR: Could not create socket (%s)\n",
            strerror(errno));
        return -1;
    }

    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, SOCKET_FILENAME);
    unlink(SOCKET_FILENAME);
    if (bind(listener, (struct sockaddr*)&sa, sizeof(sa))
            == -1) {
        fprintf(stderr, "serial: ERROR: Could not bind socket (%s)\n",
            strerror(errno));
        return -1;
    }

    if (listen(listener, MAX_CLIENTS) == -1) {
        fprintf(stderr, "serial: ERROR: Could not listen on socket (%s)\n",
            strerror(errno));
        return -1;
    }

    return listener;
}


int init_timer()
{
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_int = 0,
        .sigev_notify_function = send_message,
        .sigev_notify_attributes = NULL
    };
    timer_t timerid;
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        fprintf(stderr, "serial: ERROR: Could not create timer (%s)\n",
            strerror(errno));
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
    assert(signum == SIGINT || signum == SIGQUIT || signum == SIGABRT);
    unlink(SOCKET_FILENAME);  // Ignore errors.
    fputs("serial: Dying...\n", stderr);
    exit(2);
}


int main()
{
    if (signal(SIGABRT, die) == SIG_ERR) {
        fprintf(stderr,
            "serial: ERROR: Could not set up SIGABRT handler (%s)\n",
            strerror(errno));
        exit(1);
    }

    fputs("Initializing...\n", stdout);

    int listener = init_socket();
    if (listener == -1)
        abort();

    if (signal(SIGINT, die) == SIG_ERR || signal(SIGQUIT, die) == SIG_ERR) {
        fprintf(stderr,
            "serial: ERROR: Could not set up signal handler (%s)\n",
            strerror(errno));
        abort();
    }

    if (init_timer() == -1)
        abort();

    fd_set listener_set;
    FD_ZERO(&listener_set);
    FD_SET(listener, &listener_set);
    while (1) {
        fputs("Waiting for connections on socket...\n", stdout);
        if (select(listener + 1, &listener_set, NULL, NULL, NULL) == -1) {
            fprintf(stderr, "serial: ERROR: select() failed (%s)\n",
                strerror(errno));
            abort();
        }
        int client = accept(listener, NULL, 0);
        if (client == -1) {
            fprintf(stderr, "serial: ERROR: accept() failed (%s)\n",
                strerror(errno));
            abort();
        }
        printf("Client fd = %d\n", client);
        clients[num_clients++] = client;
    }
}
