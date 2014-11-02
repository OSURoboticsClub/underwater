#ifndef TEST_H
#define TEST_H

#include <linux/videodev2.h>
#include <stdint.h>

enum {
    READWRITE,
    STREAMING
};

struct grabber {
    int dev_fd;
    struct v4l2_buffer buffer;
    uint32_t format_type;
    uint8_t* frame_data;
    uint32_t width;
    uint32_t height;
};

void print_info(const char* device);

struct grabber* create_grabber(
    const char* device, int input_index, int verbose);

int grab(struct grabber* grabber);

void delete_grabber(struct grabber* grabber);

#endif // TEST_H
