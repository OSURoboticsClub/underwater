#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "grab.h"


uint8_t clamp8(double x)
{
    if (x >= 256.0)
        return 255;
    if (x < 0)
        return 0;
    return (uint8_t)x;
}


int main(int argc, char** argv)
{
    if (argc != 5) {
        fputs("Usage:\n", stdout);
        fputs("  ./test device width height filename-prefix\n", stdout);
        return 2;
    };

    char* device = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    char* filename_prefix = argv[4];

    print_info(device, 0);

    struct grabber* g = create_grabber(argv[1], 0, height, width);
    if (g == NULL) {
        fputs("Oh no! Couldn't initialize grabber\n", stderr);
        return 1;
    };

    printf("Dimensions set to %ux%u\n", g->width, g->height);
    fputs("\n", stdout);

    fputs("Sleeping...\n", stdout);
    sleep(2);

    char filename[strlen(filename_prefix) + 3 + 1];
    strcpy(filename, filename_prefix);
    char* filename_suffix = filename + strlen(filename_prefix) + 1;
    filename_suffix[-1] = '-';
    for (int i = 0; i <= 20; ++i) {
        fputs("Grabbing frame...\n", stdout);
        int r = grab(g);
        if (r < 0) {
            fputs("Oh no! Couldn't grab frame.\n", stderr);
            break;
        };

        sprintf(filename_suffix, "%02d", i);
        printf("Writing to \"%s\"...\n", filename);

        FILE* img = fopen(filename, "wb");
        if (img == NULL) {
            fputs("Oh no! Couldn't open \"%s\" for writing.\n", stderr);
            break;
        };

        fprintf(img, "P6 %u %u 255\n", g->width, g->height);
        unsigned int pixels = 0;
        for (int j = 0; j <= g->buffer.length - 4; j += 4) {
            uint8_t y0 = g->frame_data[j];
            uint8_t cb = g->frame_data[j + 1];
            uint8_t y1 = g->frame_data[j + 2];
            uint8_t cr = g->frame_data[j + 3];
            uint8_t rgb[6] = {
                clamp8(y0 + (1.4065 * (cr - 128))),
                clamp8(y0 - (0.3455 * (cb - 128)) - (0.7169 * (cr - 128))),
                clamp8(y0 + (1.7790 * (cb - 128))),
                clamp8(y1 + (1.4065 * (cr - 128))),
                clamp8(y1 - (0.3455 * (cb - 128)) - (0.7169 * (cr - 128))),
                clamp8(y1 + (1.7790 * (cb - 128)))
            };
            fwrite(rgb, 6, 1, img);
            pixels += 2;
        };

        fclose(img);
        printf("Wrote %u pixels\n", pixels);
    };

    delete_grabber(g);

    return 0;
}
