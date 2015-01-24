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
    char* prog[] = {"./test", NULL};
    progs[0] = malloc(sizeof(prog));
    if (progs[0] == NULL)
        error_e("Can't allocate memory");
    memcpy(progs[0], prog, sizeof(prog));
    progs[1] = malloc(sizeof(prog));
    if (progs[1] == NULL)
        error_e("Can't allocate memory");
    memcpy(progs[1], prog, sizeof(prog));

    bool do_heartbeat[2] = {true, false};

    init_manager(argv[0], 2, progs, do_heartbeat);

    free(progs[0]);
    free(progs[1]);
}
