#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>


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


int print_device_info(int fd)
{
    int number;
    int r = ioctl(fd, VIDIOC_G_OUTPUT, &number);
    if (r >= 0) {
        printf("number = %d\n", number);
        return 0;
    };

    if (r == EINVAL)
        fputs("No or too many outputs\n", stderr);
    else
        fputs("Unknown error finding outputs\n", stderr);

    return -1;
}


int check_capabilities(int fd)
{
    struct v4l2_capability c;
    int r = ioctl(fd, VIDIOC_QUERYCAP, &c);
    if (r >= 0) {
        printf("driver = %s\n", c.driver);
        printf("card = %s\n", c.card);
        printf("bus_info = %s\n", c.bus_info);
        printf("version = %d\n", c.version);
        return 0;
    };

    fputs("Unknown error checking capabilities\n", stderr);
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

    return 0;
}
