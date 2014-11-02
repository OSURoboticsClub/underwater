#include <stdio.h>
#include <unistd.h>

#include "grab.h"


int main(int argc, char** argv)
{
    if (argc != 3) {
        fputs("Usage:\n", stdout);
        fputs("  ./test device filename-prefix\n", stdout);
        return 2;
    };

    print_info(argv[1], 0);

    struct grabber* g = create_grabber(argv[1], 0, 1);
    if (g == NULL) {
        fputs("Couldn't initialize grabber\n", stderr);
        return 1;
    };
    fputs("\n", stdout);

    fputs("Sleeping...\n", stdout);
    sleep(2);

    fputs("Grabbing frame...\n", stdout);
    int r = grab(g);
    if (r < 0)
        fputs("Oh no! Couldn't grab frame.\n", stdout);

    delete_grabber(g);

    return 0;
}
