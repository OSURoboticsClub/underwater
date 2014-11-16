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
    uint8_t* raw_frame;
    uint8_t* frame;
    uint32_t width;
    uint32_t height;
    uint32_t pixelformat;
};

void print_info(const char* device, int input_index);

struct grabber* create_grabber(
    const char* device, int input_index, int width, int height);

int grab(struct grabber* grabber);

int process(struct grabber* grabber);

void delete_grabber(struct grabber* grabber);

#endif // TEST_H
