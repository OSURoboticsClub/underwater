#include <stdio.h>

#include "grab.h"


int main(int argc, char** argv)
{
    if (argc != 3) {
        fputs("Usage:\n", stdout);
        fputs("  ./test device filename-prefix\n", stdout);
        return 2;
    };

    print_info(argv[1]);

    struct grabber* g = create_grabber(argv[1], 0, 1);
    if (g == NULL) {
        fputs("Couldn't initialize grabber\n", stderr);
        return 1;
    };

    delete_grabber(g);

    return 0;
}
