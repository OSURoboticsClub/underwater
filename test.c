#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "grab.h"


uint8_t convolve_y(uint8_t* data, int width, int i)
{
    return clamp_2(
        -4*data[i] + data[i - 2] + data[i + 2] + data[i - width] +
        data[i + width]);
}


int main(int argc, char** argv)
{
    char* filename_prefix;
    switch (argc) {
        case 4:
            filename_prefix = NULL;
            break;
        case 5:
            filename_prefix = argv[4];
            break;
        default:
            fputs("Usage:\n", stdout);
            fputs("  ./test device width height [filename-prefix]\n", stdout);
            return 2;
    };

    char* device = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);

    struct grabber* g = create_grabber(device, 0, height, width);
    if (g == NULL) {
        fputs("Oh no! Couldn't initialize grabber\n", stderr);
        return 1;
    };

    printf("Dimensions set to %ux%u\n", g->width, g->height);
    fputs("\n", stdout);

    /*
     *fputs("Sleeping...\n", stdout);
     *sleep(2);
     */

    char* filename;
    char* filename_suffix;
    if (filename_prefix != NULL) {
        filename = malloc(
            sizeof(char) * (strlen(filename_prefix) + 1 + 2 + 1 + 1 + 1));
        strcpy(filename, filename_prefix);
        filename_suffix = filename + strlen(filename_prefix) + 1;
        filename_suffix[-1] = '-';
    };

    for (int i = 1; i <= 20; ++i) {
        fputs("Grabbing frame...\n", stdout);
        int r = grab(g);
        if (r < 0) {
            fputs("Oh no! Couldn't grab frame.\n", stderr);
            break;
        };
        r = process(g);
        if (r < 0) {
            fputs("Oh no! Couldn't color-process frame.\n", stderr);
            break;
        };

        if (filename_prefix != NULL) {
            sprintf(filename_suffix, "%02d-r", i);
            printf("Writing to \"%s\"...\n", filename);

            FILE* img = fopen(filename, "wb");
            if (img == NULL) {
                fputs("Oh no! Couldn't open \"%s\" for writing.\n", stderr);
                break;
            };
            fprintf(img, "P6 %u %u 255\n", g->width, g->height);
            fwrite(g->frame, g->width * g->height * 3, 1, img);
            fclose(img);
        } else {
            printf("Grabbed frame #%d\n", i);
        };
    };

    delete_grabber(g);

    return 0;
}
