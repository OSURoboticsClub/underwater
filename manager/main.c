#include "manager.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>


int main()
{
    char** argvv[2];
    char* argv[] = {"./test", NULL};
    argvv[0] = malloc(sizeof(argv));
    if (argvv[0] == NULL) {
        warnx("Can't allocate memory");
        return 1;
    }
    memcpy(argvv[0], argv, sizeof(argv));
    argvv[1] = malloc(sizeof(argv));
    if (argvv[1] == NULL) {
        warnx("Can't allocate memory");
        return 1;
    }
    memcpy(argvv[1], argv, sizeof(argv));

    init_manager(2, argvv);

    free(argvv[0]);
    free(argvv[1]);
}
