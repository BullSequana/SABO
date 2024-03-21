/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"
#include "comm.h"

int main(int argc, char *argv[])
{
    int rc;
    int node_size;
    int req = MPI_THREAD_MULTIPLE;
    int prov;

    if (comm_is_initialized())
        goto ERROR;

    rc = MPI_Init_thread(&argc, &argv, req, &prov);
    if (MPI_SUCCESS != rc)
        goto ERROR;

    if (!comm_is_initialized())
        goto ERROR;

    comm_load_module_mpi();

    comm_init();

    if (NULL == comm_get_recv_buffer(2))
        goto ERROR;

    if (NULL == comm_get_send_buffer(2))
        goto ERROR;

    node_size = comm_get_node_size();

    for (int i = 0; i < node_size; i++)
        (void) comm_get_world_rank_from_node_rank(i);

    comm_fini();

    rc = MPI_Finalize();
    if (MPI_SUCCESS != rc)
        goto ERROR;

    comm_unload_module_mpi();

    if (comm_is_initialized())
        goto ERROR;

    printf("all done\n");
    return EXIT_SUCCESS;

ERROR:
    printf("failure\n");
    return EXIT_FAILURE;
}
