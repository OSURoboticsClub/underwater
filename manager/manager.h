#pragma once


#include <stdbool.h>

void init_manager(char* name, int worker_count, char*** argvv,
        bool do_heartbeat[]);
