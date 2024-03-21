/*
 * Copyright 2021-2024 Bull SAS
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "comm.h"
#include "log.h"
#include "sys.h"

#include "module_mpi.h"

#define SABO_COMM_UNDEF_VALUE -1

struct comm_module_funcs __sabo_module_funcs;
static int __sabo_comm_buffer_window = -1;
static double *__sabo_comm_recv_buffer = NULL;
static double *__sabo_comm_send_buffer = NULL;

int MPI_Init(int *argc, char ***argv);
int MPI_Init_thread(int *argc, char ***argv, int req, int *prov);
int MPI_Finalize(void);

static void comm_alloc_node_comm(void)
{
    __sabo_module_funcs.alloc_node_comm();
}

static void comm_free_node_comm(void)
{
     __sabo_module_funcs.free_node_comm();
}

int comm_is_initialized(void)
{
    if (!__sabo_module_funcs.is_initialized)
           return 0;

    return __sabo_module_funcs.is_initialized();
}

int comm_get_node_rank(void)
{
    static int __sabo_comm_node_rank = SABO_COMM_UNDEF_VALUE;

    if (likely(SABO_COMM_UNDEF_VALUE != __sabo_comm_node_rank))
        return __sabo_comm_node_rank;

    __sabo_comm_node_rank = __sabo_module_funcs.get_node_rank();

    return __sabo_comm_node_rank;
}

int comm_get_node_size(void)
{
    static int __sabo_comm_node_size = SABO_COMM_UNDEF_VALUE;

    if (likely(SABO_COMM_UNDEF_VALUE != __sabo_comm_node_size))
        return __sabo_comm_node_size;

    __sabo_comm_node_size = __sabo_module_funcs.get_node_size();

    return __sabo_comm_node_size;
}

int comm_get_world_rank(void)
{
    static int __sabo_comm_world_rank = SABO_COMM_UNDEF_VALUE;

    if (likely(SABO_COMM_UNDEF_VALUE != __sabo_comm_world_rank))
        return __sabo_comm_world_rank;

    __sabo_comm_world_rank = __sabo_module_funcs.get_world_rank();

    return __sabo_comm_world_rank;
}

int comm_get_world_size(void)
{
    static int __sabo_comm_world_size = SABO_COMM_UNDEF_VALUE;

    if (likely(SABO_COMM_UNDEF_VALUE != __sabo_comm_world_size))
        return __sabo_comm_world_size;

    __sabo_comm_world_size = __sabo_module_funcs.get_world_size();

    return __sabo_comm_world_size;
}

int comm_get_world_rank_from_node_rank(const int rank)
{
    assert(rank >= 0 && rank < __sabo_module_funcs.get_node_size());
    return __sabo_module_funcs.get_world_rank_from_node_rank(rank);
}

void comm_allgather(const double *sbuf, double *rbuf, const int count)
{
    __sabo_module_funcs.allgather(sbuf, rbuf, count);
}

double *comm_get_recv_buffer(const int window)
{
    int count;

    if (unlikely(-1 == __sabo_comm_buffer_window))
        __sabo_comm_buffer_window = window;

    if (unlikely(NULL != __sabo_comm_recv_buffer)) {
        assert(window == __sabo_comm_buffer_window);
        return __sabo_comm_recv_buffer;
    }

    count = comm_get_node_size() * window;
    __sabo_comm_recv_buffer = xzalloc((size_t) count * sizeof(double));

    return __sabo_comm_recv_buffer;
}

double *comm_get_send_buffer(const int window)
{
    if (unlikely(-1 == __sabo_comm_buffer_window))
        __sabo_comm_buffer_window = window;

    if (unlikely(NULL != __sabo_comm_send_buffer)) {
        assert(window == __sabo_comm_buffer_window);
        return __sabo_comm_send_buffer;
    }

    __sabo_comm_recv_buffer = xzalloc((size_t) window * sizeof(double));

    return __sabo_comm_recv_buffer;
}

void comm_init(void)
{
    /* World communicator */
    (void) comm_get_world_rank();
    (void) comm_get_world_size();

    /* Node communicator */
    comm_alloc_node_comm();
    (void) comm_get_node_rank();
    (void) comm_get_node_size();
}

void comm_fini(void)
{
    xfree(__sabo_comm_recv_buffer);
    __sabo_comm_recv_buffer = NULL;

    xfree(__sabo_comm_send_buffer);
    __sabo_comm_send_buffer = NULL;

    __sabo_comm_buffer_window = -1;

    comm_free_node_comm();
}

static void comm_reset_module_funcs(struct comm_module_funcs *funcs)
{
    memset(funcs, 0, sizeof(struct comm_module_funcs));
}

void comm_load_module_mpi(void)
{
    struct comm_module_funcs mpi_module_funcs;
    sabo_module_mpi_register_cb(&mpi_module_funcs);
    __sabo_module_funcs = mpi_module_funcs;
}

void comm_unload_module_mpi(void)
{
    comm_reset_module_funcs(&__sabo_module_funcs);
}

int MPI_Init(int *argc, char ***argv)
{
    int rc;

    comm_load_module_mpi();
    rc = __sabo_module_funcs.init(argc, argv);

    comm_init();

    return rc;
}

int MPI_Init_thread(int *argc, char ***argv, int req, int *prov)
{
    int rc;

    comm_load_module_mpi();
    rc = __sabo_module_funcs.init_thread(argc, argv, req, prov);

    comm_init();

    return rc;
}

int MPI_Finalize(void)
{
    int rc;
    struct comm_module_funcs mpi_module_funcs;

    sabo_module_mpi_register_cb(&mpi_module_funcs);

    comm_fini();

    rc = mpi_module_funcs.fini();

    return rc;
}
