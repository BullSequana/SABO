/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include "sabo_internal.h"
#include "sabo.h"
#include "topo.h"

int main(int argc, char *argv[])
{
    int num_cores;
    ompt_threads_data_t *data;
    int req = MPI_THREAD_MULTIPLE;
    int prov;

    sabo_core_init();

    sabo_omp_balanced();

    MPI_Init_thread(&argc, &argv, req, &prov);

    num_cores = topo_get_num_cores();

    data = sabo_core_get_ompt_data();
    data->start = 0.0;

    /* Populate ompt counters */
    for (int i = 0; i < num_cores / 2; i++)
        data->elapsed[i] = 2.0;

    for (int i = num_cores / 2; i < num_cores; i++)
        data->elapsed[i] = 1.0;

    sabo_omp_balanced();

    MPI_Finalize();

    sabo_omp_balanced();

    sabo_core_fini(0);

    printf("all done\n");
    return EXIT_SUCCESS;
}
