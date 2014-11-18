#ifndef TEST_H
#define TEST_H

#include <linux/videodev2.h>
#include <stdint.h>

struct grabber {
    // FOR INTERNAL USE.
    int dev_fd;
    struct v4l2_buffer buffer;
    uint32_t format_type;

    // The color format (see the V4L2 docs for details).
    uint32_t pixelformat;

    // The raw frame grabbed from the device.
    uint8_t* raw_frame;

    // The processed frame in a row-major array of 8-bit R, G, and B values in
    // that order.
    uint8_t* frame;

    // The width and height of `frame`.
    uint32_t width;
    uint32_t height;
};

// Print info about V4L2 device with filename `device`. Also print info about
// the input indexed by `input_index`.
void print_info(const char* device, int input_index);

// Create a `grabber` and get it ready for grabbing frames.
struct grabber* create_grabber(
    const char* device, int input_index, int width, int height);

// Grab a frame using `grabber`; put it in `grabber->raw_frame`.
// Return value: 0 if success; -1 if failure.
int grab(struct grabber* grabber);

// Map `grabber->raw_frame` into RGB and put the result in `grabber->frame`.
// Return value: 0 if success; -1 if failure.
int process(struct grabber* grabber);

// Delete `grabber`, including its raw and processed frame buffers.
void delete_grabber(struct grabber* grabber);

#endif  // TEST_H
