#include "error.h"
#include "manager.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


#define error_e(msg) error_e_helper(argv[0], msg, errno)


int main(int argc, char* argv[])
{
    char** progs[2];
    char* prog0[] = {"./printer", NULL};
    progs[0] = malloc(sizeof(prog0));
    if (progs[0] == NULL)
        error_e("Can't allocate memory");
    memcpy(progs[0], prog0, sizeof(prog0));

    char* prog1[] = {"./keyboard", NULL};
    progs[1] = malloc(sizeof(prog1));
    if (progs[1] == NULL)
        error_e("Can't allocate memory");
    memcpy(progs[1], prog1, sizeof(prog1));

    bool do_heartbeat[2] = {true, false};

    init_manager(argv[0], 2, progs, do_heartbeat);

    free(progs[0]);
    free(progs[1]);
}
