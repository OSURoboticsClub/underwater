#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "grab.h"


uint8_t clamp_d(double x)
{
    if (x >= 256.0)
        return 0xFF;
    if (x < 0.0)
        return 0x00;
    return (uint8_t)x;
}


uint8_t clamp_2(int16_t x)
{
    if (x > 0xFF)
        return 0xFF;
    if (x < 0x00)
        return 0x00;
    return (uint8_t)x;
}


uint8_t convolve_y(uint8_t* data, int width, int i)
{
    return clamp_2(
        -4*data[i] + data[i - 2] + data[i + 2] + data[i - width] +
        data[i + width]);
}


void to_rgb(uint8_t* rgb, int y, int cb, int cr)
{
    rgb[0] = clamp_d(1.164*(y - 16) + 1.596*(cr - 128));
    rgb[1] = clamp_d(1.164*(y - 16) - 0.813*(cr - 128) - 0.392*(cb - 128));
    rgb[2] = clamp_d(1.164*(y - 16) + 2.017*(cb - 128));
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

    struct grabber* g = create_grabber(device, 0, height, width);
    if (g == NULL) {
        fputs("Oh no! Couldn't initialize grabber\n", stderr);
        return 1;
    };

    printf("Dimensions set to %ux%u\n", g->width, g->height);
    fputs("\n", stdout);

    fputs("Sleeping...\n", stdout);
    sleep(2);

    char filename[strlen(filename_prefix) + 1 + 2 + 1 + 1+ 1];
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

        sprintf(filename_suffix, "%02d-r", i);
        printf("Writing to \"%s\"...\n", filename);

        FILE* img = fopen(filename, "wb");
        if (img == NULL) {
            fputs("Oh no! Couldn't open \"%s\" for writing.\n", stderr);
            break;
        };
        fprintf(img, "P6 %u %u 255\n", g->width, g->height);
        for (int j = 0; j <= g->buffer.length - 4; j += 4) {
            uint8_t y0 = g->frame_data[j];
            uint8_t cb = g->frame_data[j + 1];
            uint8_t y1 = g->frame_data[j + 2];
            uint8_t cr = g->frame_data[j + 3];
            uint8_t rgb[6];
            to_rgb(rgb, y0, cb, cr);
            to_rgb(rgb + 3, y1, cb, cr);
            fwrite(rgb, 6, 1, img);
        };
        fclose(img);

        filename_suffix[3] = 'e';
        printf("Writing to \"%s\"...\n", filename);

        int data_width = g->width * 2;
        img = fopen(filename, "wb");
        if (img == NULL) {
            fputs("Oh no! Couldn't open \"%s\" for writing.\n", stderr);
            break;
        };
        fprintf(img, "P5 %u %u 255\n", g->width - 4, g->height - 2);
        for (int j = data_width; j <= g->buffer.length - data_width - 4;
                j += 4) {
            if (j % data_width == 0 || j % data_width == data_width - 4) {
                continue;
            };
            uint8_t y0 = convolve_y(g->frame_data, data_width, j);
            uint8_t y1 = convolve_y(g->frame_data, data_width, j + 2);
            fwrite(&y0, 1, 1, img);
            fwrite(&y1, 1, 1, img);
        };
        fclose(img);
    };

    delete_grabber(g);

    return 0;
}
