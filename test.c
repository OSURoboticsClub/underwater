#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


int open_device(const char* filename)
{
    int fd = open(filename, O_RDWR);
    if (fd >= 0)
        return fd;

    if (errno == ENOENT)
        fputs("ENOENT (File does not exist)\n", stderr);
    else if (errno == EACCES)
        fputs("EACCES (Can't access device)\n", stderr);
    else if (errno == EBUSY)
        fputs("EBUSY (Device is busy)\n", stderr);
    else if (errno == ENXIO)
        fputs("ENXIO\n", stderr);
    else if (errno == ENOMEM)
        fputs("ENOMEM\n", stderr);
    else if (errno == EMFILE)
        fputs("EMFILE\n", stderr);
    else if (errno == ENFILE)
        fputs("ENFILE\n", stderr);
    else
        fputs("Unknown error opening device\n", stderr);

    return -1;
}


int check_capabilities(int fd)
{
    struct v4l2_capability c;
    int r = ioctl(fd, VIDIOC_QUERYCAP, &c);
    if (r >= 0) {
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
    };

    fputs("Unknown error checking capabilities\n", stderr);
    return -1;
}


int print_device_info(int fd)
{
    struct v4l2_input in;
    in.index = 0;
    int r = ioctl(fd, VIDIOC_ENUMINPUT, &in);
    if (r >= 0) {
        fputs("Device info:\n", stdout);
        printf("  index = %d\n", in.index);
        printf("  name = %s\n", in.name);
        printf("  type = %d\n", in.type);
        printf("  std = %u\n", (unsigned int)in.std);
        return 0;
    };

    if (errno == EINVAL)
        fputs("EINVAL (Index out of bounds)\n", stderr);
    else
        fputs("Unknown error enumerating output\n", stderr);

    return -1;
}


int set_input(int fd)
{
    int index = 0;
    int r = ioctl(fd, VIDIOC_S_INPUT, &index);
    if (r >= 0) {
        fputs("Input set to 0\n", stdout);
        return 0;
    };

    fputs("Couldn't set input to 0\n", stderr);
    return -1;
}


int get_format(int fd)
{
    struct v4l2_format f;
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int r = ioctl(fd, VIDIOC_G_FMT, &f);
    if (r >= 0) {
        printf("type = %u\n", f.type);
        printf("width = %u\n", f.fmt.pix.width);
        printf("height = %u\n", f.fmt.pix.height);
        printf("pixelformat = %u\n", f.fmt.pix.pixelformat);
        printf("sizeimage = %u\n", f.fmt.pix.sizeimage);
        return f.fmt.pix.sizeimage;
    };

    fputs("Couldn't get format\n", stdout);
    return -1;
}


int read_image(int fd, int sizeimage)
{
    printf("About to read %d bytes\n", sizeimage);
    char* buffer = malloc(sizeimage * sizeof(char));
    ssize_t bytes_read = read(fd, buffer, sizeimage);
    if (bytes_read == sizeimage) {
        int image_fd = open("out", O_WRONLY);
        write(image_fd, buffer, sizeimage);
        close(image_fd);
        fputs("Read some bytes\n", stdout);
        free(buffer);
        return 0;
    };

    fprintf(stderr, "strerror says: %s\n", strerror(errno));
    fprintf(stderr, "Only read %zd bytes\n", bytes_read);
    free(buffer);
    return -1;
}


int main(int argc, char** argv)
{
    int fd = open_device(argv[1]);
    if (fd < 0)
        return 1;

    if (check_capabilities(fd) < 0)
        return 1;

    if (print_device_info(fd) < 0)
        return 1;

    if (set_input(fd) < 0)
        return 1;

    int sizeimage = get_format(fd);
    if (sizeimage < 0)
        return 1;

    if (read_image(fd, sizeimage) < 0)
        return 1;

    return 0;
}
