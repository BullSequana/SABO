/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>

#include "topo.h"
#include "sys.h"

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    topo_init();

    (void) topo_get_num_sockets();
    (void) topo_get_num_cores();
    (void) topo_get_num_cores_per_socket();

    topo_fini();

    printf("all done\n");
    return EXIT_SUCCESS;
}
