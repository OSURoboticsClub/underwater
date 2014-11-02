#include "grab.h"

#define _POSIX_C_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>


uint8_t clamp8(double x)
{
    if (x >= 256.0)
        return 255;
    if (x < 0)
        return 0;
    return (uint8_t)x;
}


int print_capabilities(int dev_fd)
{
    struct v4l2_capability c;
    int r = ioctl(dev_fd, VIDIOC_QUERYCAP, &c);
    if (r < 0) {
        fprintf(
            stderr, "Error: Couldn't check capabilities (%s)\n",
            strerror(errno));
        return -1;
    };

    fputs("Capabilities:\n", stdout);
    printf("  driver = %s\n", c.driver);
    printf("  card = %s\n", c.card);
    printf("  bus_info = %s\n", c.bus_info);
    printf("  version = %d\n", c.version);
    if (c.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        fputs("  CAP_VIDEO_CAPTURE\n", stdout);
    if (c.capabilities & V4L2_CAP_VIDEO_M2M)
        fputs("  CAP_VIDEO_M2M\n", stdout);
    if (c.capabilities & V4L2_CAP_VBI_CAPTURE)
        fputs("  CAP_VBI_CAPTURE\n", stdout);
    if (c.capabilities & V4L2_CAP_READWRITE)
        fputs("  CAP_READWRITE\n", stdout);
    if (c.capabilities & V4L2_CAP_ASYNCIO)
        fputs("  CAP_ASYNCIO\n", stdout);
    if (c.capabilities & V4L2_CAP_STREAMING)
        fputs("  CAP_STREAMING\n", stdout);
    return 0;
}


int print_input_info(int dev_fd)
{
    struct v4l2_input in;

    for (in.index = 0; ; ++in.index) {
        int r = ioctl(dev_fd, VIDIOC_ENUMINPUT, &in);
        if (r < 0) {
            if (errno == EINVAL)
                return 0;
            fprintf(
                stderr, "Error: Couldn't enumerate inputs (%s)\n",
                strerror(errno));
            return -1;
        };

        printf("Device %d:\n", in.index);
        printf("  name = %s\n", in.name);
        printf("  type = %d\n", in.type);
        printf("  std = %u\n", (unsigned int)in.std);
    };
}


int set_input(int dev_fd, int input_index)
{
    int r = ioctl(dev_fd, VIDIOC_S_INPUT, &input_index);
    if (r < 0) {
        fprintf(
            stderr, "Error: Couldn't set input (%s)\n", strerror(errno));
        return -1;
    };

    return 0;
}


int get_format(int dev_fd, uint32_t* type, uint32_t* sizeimage,
        uint32_t* width, uint32_t* height)
{
    struct v4l2_format f;
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int r = ioctl(dev_fd, VIDIOC_G_FMT, &f);
    if (r < 0) {
        fprintf(stderr, "Error: Couldn't get format (%s)\n", strerror(errno));
        return -1;
    };

    *type = f.type;
    *sizeimage = f.fmt.pix.sizeimage;
    *width = f.fmt.pix.width;
    *height = f.fmt.pix.height;
    return 0;
}


int print_format(int fd)
{
    struct v4l2_format f;
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int r = ioctl(fd, VIDIOC_G_FMT, &f);
    if (r < 0) {
        fprintf(stderr, "Error: Couldn't get format (%s)\n", strerror(errno));
        return -1;
    };

    fputs("Format:\n", stdout);
    printf("  type = %u\n", f.type);
    printf("  width = %u\n", f.fmt.pix.width);
    printf("  height = %u\n", f.fmt.pix.height);
    char pixelformat[5] = {
        f.fmt.pix.pixelformat & 0xFF,
        (f.fmt.pix.pixelformat & 0xFF00) >> 8,
        (f.fmt.pix.pixelformat & 0xFF0000) >> 16,
        (f.fmt.pix.pixelformat & 0xFF000000) >> 24,
        0
    };
    printf("  pixelformat = %u = %s\n", f.fmt.pix.pixelformat, pixelformat);
    printf("  sizeimage = %u\n", f.fmt.pix.sizeimage);
    return 0;
}


int read_image(int fd, int sizeimage)
{
    printf("About to read %d bytes\n", sizeimage);
    char* buffer = malloc(sizeimage * sizeof(char));
    ssize_t bytes_read = read(fd, buffer, sizeimage);
    if (bytes_read != sizeimage) {
        fprintf(stderr, "Error: Couldn't read image (%s)\n", strerror(errno));
        fprintf(stderr, "Only read %zd bytes\n", bytes_read);
        free(buffer);
        return -1;
    };

    int image_fd = open("out", O_WRONLY);
    write(image_fd, buffer, sizeimage);
    close(image_fd);
    fputs("Read some bytes\n", stdout);
    free(buffer);
    return 0;
}


int stream_images(int fd, const char* filename_prefix, uint32_t format_type,
        uint32_t width, uint32_t height)
{
    struct v4l2_requestbuffers rb;
    rb.count = 1;
    rb.type = format_type;
    rb.memory = V4L2_MEMORY_MMAP;
    rb.reserved[0] = 0;
    rb.reserved[1] = 0;
    int r = ioctl(fd, VIDIOC_REQBUFS, &rb);
    if (r < 0) {
        fputs("REQBUFS failed", stderr);
        return -1;
    };

    fputs("Buffers:\n", stdout);
    printf("  count = %d\n", rb.count);

    fputs("  Buffer 0:\n", stdout);
    struct v4l2_buffer b;
    b.type = format_type;
    b.index = 0;
    b.reserved = 0;
    b.reserved2 = 0;
    r = ioctl(fd, VIDIOC_QUERYBUF, &b);
    if (r < 0) {
        fputs("QUERYBUF failed\n", stderr);
        return -1;
    };

    uint8_t* mem = mmap(
        NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
        b.m.offset);
    printf("    map address = %p\n", mem);

    r = ioctl(fd, VIDIOC_STREAMON, &format_type);
    if (r < 0) {
        fputs("STREAMON failed\n", stderr);
        return -1;
    };

    fputs("Sleeping\n", stdout);
    sleep(2);

    char filename[strlen(filename_prefix) + 3 + 1];
    strcpy(filename, filename_prefix);
    char* filename_suffix = filename + strlen(filename_prefix) + 1;
    filename_suffix[-1] = '-';
    for (int i = 0; i <= 20; ++i) {
        r = ioctl(fd, VIDIOC_QBUF, &b);
        if (r < 0) {
            fputs("QBUF failed\n", stderr);
            return -1;
        };

        fputs("Waiting for frame\n", stdout);
        r = ioctl(fd, VIDIOC_DQBUF, &b);
        if (r < 0) {
            fputs("DQBUF failed\n", stderr);
            return -1;
        };
        fputs("Frame ready\n", stdout);

        printf("Opening %s\n", filename);
        sprintf(filename_suffix, "%02d", i);
        FILE* img = fopen(filename, "wb");
        fprintf(img, "P6 %u %u 255\n", width, height);
        unsigned int pixels = 0;
        for (uint32_t i = 0; i <= b.length - 4; i += 4) {
            uint8_t y0 = mem[i];
            uint8_t cb = mem[i + 1];
            uint8_t y1 = mem[i + 2];
            uint8_t cr = mem[i + 3];
            uint8_t rgb[6] = {
                clamp8(y0 + (1.4065 * (cr - 128))),
                clamp8(
                    y0 - (0.3455 * (cb - 128)) -
                    (0.7169 * (cr - 128))),
                clamp8(y0 + (1.7790 * (cb - 128))),
                clamp8(y1 + (1.4065 * (cr - 128))),
                clamp8(
                    y1 - (0.3455 * (cb - 128)) -
                    (0.7169 * (cr - 128))),
                clamp8(y1 + (1.7790 * (cb - 128)))
            };
            fwrite(rgb, 6, 1, img);
            pixels += 2;
        };
        printf("Wrote %u pixels\n", pixels);
        fclose(img);
        printf("Closing %s\n", filename);
    };

    r = ioctl(fd, VIDIOC_STREAMOFF, &format_type);
    if (r < 0)
        return -1;

    munmap(mem, b.length);
    return 0;
}


void print_info(const char* device)
{
    int dev_fd = open(device, O_RDWR);
    if (dev_fd < 0) {
        fprintf(stderr, "Couldn't open device (%s)\n", strerror(errno));
        return;
    };

    print_capabilities(dev_fd);
    print_input_info(dev_fd);
    set_input(dev_fd, 0);
    print_format(dev_fd);

    close(dev_fd);
}


struct grabber* create_grabber(
        const char* device, int input_index, int verbose)
{
    struct grabber* grabber = malloc(sizeof(struct grabber));
    int r;

    grabber->dev_fd = open(device, O_RDWR);
    if (grabber->dev_fd < 0) {
        fprintf(stderr, "Couldn't open device (%s)\n", strerror(errno));
        free(grabber);
        return NULL;
    };

    r = set_input(grabber->dev_fd, input_index);
    if (r < 0) {
        free(grabber);
        return NULL;
    };

    struct v4l2_format f;
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    r = ioctl(grabber->dev_fd, VIDIOC_G_FMT, &f);
    if (r < 0) {
        fprintf(stderr, "Error: Couldn't get format (%s)\n", strerror(errno));
        free(grabber);
        return NULL;
    };
    grabber->format_type = f.type;

    struct v4l2_requestbuffers rb;
    rb.count = 1;
    rb.type = f.type;
    rb.memory = V4L2_MEMORY_MMAP;
    rb.reserved[0] = 0;
    rb.reserved[1] = 0;
    r = ioctl(grabber->dev_fd, VIDIOC_REQBUFS, &rb);
    if (r < 0) {
        fprintf(
            stderr, "Buffer request was denied (%s)\n", strerror(errno));
        free(grabber);
        return NULL;
    };

    grabber->buffer.type = f.type;
    grabber->buffer.index = 0;
    grabber->buffer.reserved = 0;
    grabber->buffer.reserved2 = 0;
    r = ioctl(grabber->dev_fd, VIDIOC_QUERYBUF, &grabber->buffer);
    if (r < 0) {
        fprintf(
            stderr, "Error: Couldn't query buffer address (%s)\n",
            strerror(errno));
        free(grabber);
        return NULL;
    };

    grabber->frame_data = mmap(
        NULL, grabber->buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
        grabber->dev_fd, grabber->buffer.m.offset);
    if (grabber->frame_data == NULL) {
        fputs("Error: Couldn't mmap frame data\n", stderr);
        free(grabber);
        return NULL;
    };

    r = ioctl(grabber->dev_fd, VIDIOC_STREAMON, &grabber->format_type);
    if (r < 0) {
        fprintf(
            stderr, "Error: Couldn't start streaming (%s)\n", strerror(errno));
        munmap(grabber->frame_data, grabber->buffer.length);
        free(grabber);
        return NULL;
    };

    return grabber;
}


void delete_grabber(struct grabber* grabber)
{
    ioctl(grabber->dev_fd, VIDIOC_STREAMOFF, &grabber->format_type);
    munmap(grabber->frame_data, grabber->buffer.length);
    close(grabber->dev_fd);
    free(grabber);
}
