/*
 * Copyright 2022-2024 Bull SAS
 */

#include <stdio.h>
#include <sys.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <omp.h>

#include "sabo.h"

#define MAX_ITER 10

int main(int argc, char *argv[])
{
    int rank;
    int size;
    int num_iters;
    int req = MPI_THREAD_MULTIPLE;
    int prov;
    double start;
    int num_thr;
    int wait;

    if (argc != 2) {
        num_iters = 1;
    }
    else {
        num_iters = atoi(argv[1]);
    }

    MPI_Init_thread(&argc, &argv, req, &prov);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    fprintf(stdout, "/ num_iters %d -> rank %d / pid %d\n", num_iters, rank, getpid());
    fflush(stdout);

    wait = rank + 1;
    for (int iter = 0 ; iter < MAX_ITER ; iter++) {
        start = omp_get_wtime();

        #pragma omp parallel for
        for (int i = 0; i < num_iters ; i++) {
            UNUSED(i);
            num_thr = omp_get_num_threads();
            sleep(wait);
        }
        fprintf(stdout, "iter %d -> rank %d / pid %d, omp thr %d, expected %.3f s measured %.3f s\n", 
            iter, rank, getpid(), num_thr, 
            (double) wait * (num_iters / num_thr + (num_iters % num_thr ? 1 : 0)),
            omp_get_wtime() - start);
        fflush(stdout);

        MPI_Barrier(MPI_COMM_WORLD);
        fprintf(stdout, "iter %d -> rank %d / pid %d, resynchronized\n", iter, rank, getpid());
        fflush(stdout);

        #if 0
        sabo_omp_balanced();
        #endif
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}
