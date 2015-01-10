#pragma once


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void warning_helper(char* prog_name, char* msg)
{
    fprintf(stderr, "%s: %s\n", prog_name, msg);
}


static void error_helper(char* prog_name, char* msg)
{
    fprintf(stderr, "%s: %s\n", prog_name, msg);
    exit(1);
}


static void warning_e_helper(char* prog_name, char* msg, int errnum)
{
    fprintf(stderr, "%s: %s (%s)\n", prog_name, msg, strerror(errnum));
}


static void error_e_helper(char* prog_name, char* msg, int errnum)
{
    fprintf(stderr, "%s: %s (%s)\n", prog_name, msg, strerror(errnum));
    exit(1);
}
