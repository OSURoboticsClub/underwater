#include <stdio.h>
#include <stdlib.h>

#include "grab.h"


int main(int argc, char** argv)
{
    if (argc != 3) {
        fputs("Usage:\n", stdout);
        fputs("  ./print-info device input\n", stdout);
        return 2;
    };

    print_info(argv[1], atoi(argv[2]));
    return 0;
}
