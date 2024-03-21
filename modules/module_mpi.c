/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>

#ifdef SABO_USE_MPI
#include "mpi.h"
#endif /* #ifdef SABO_USE_MPI */

#include "log.h"
#include "comm.h"
#include "module_mpi.h"
#include "sys.h"

#ifdef SABO_USE_MPI
#define SABO_MPI_CHECK(y,x)                        \
    do {                                \
        int __ret = (x);                    \
        const char *__name = (y);                \
        if (unlikely(MPI_SUCCESS != __ret)) {            \
            char err_string[MPI_MAX_ERROR_STRING];        \
            int err_string_len = 0;                \
            MPI_Error_string(__ret, err_string,        \
                     &err_string_len);        \
            fatal_error("MPI call '%s' returned "        \
                    "error code %d (%s)",        \
                    __name, __ret, err_string);        \
        }                            \
    } while (0)

static MPI_Comm sabo_mpi_module_node_comm = MPI_COMM_NULL;
#endif /* #ifdef SABO_USE_MPI */

static int sabo_mpi_module_initialized = 0;
static int *sabo_mpi_module_translate_node_rank = NULL;

static int mpi_get_world_rank(void)
{
#ifdef SABO_USE_MPI
    int rc;
    int rank;

    rc = PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    SABO_MPI_CHECK("PMPI_Comm_rank", rc);

    return rank;
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int mpi_get_world_size(void)
{
#ifdef SABO_USE_MPI
    int rc;
    int size;

    rc = PMPI_Comm_size(MPI_COMM_WORLD, &size);
    SABO_MPI_CHECK("PMPI_Comm_size", rc);

    return size;
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int mpi_get_node_rank(void)
{
#ifdef SABO_USE_MPI
    int rc;
    int rank;

    assert(MPI_COMM_NULL != sabo_mpi_module_node_comm);

    rc = PMPI_Comm_rank(sabo_mpi_module_node_comm, &rank);
    SABO_MPI_CHECK("PMPI_Comm_rank", rc);

    return rank;
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int mpi_get_node_size(void)
{
#ifdef SABO_USE_MPI
    int rc;
    int size;

    assert(MPI_COMM_NULL != sabo_mpi_module_node_comm);

    rc = PMPI_Comm_size(sabo_mpi_module_node_comm, &size);
    SABO_MPI_CHECK("PMPI_Comm_size", rc);

    return size;
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int mpi_get_world_rank_from_node_rank(const int rank)
{
#ifdef SABO_USE_MPI
    assert(MPI_COMM_NULL != sabo_mpi_module_node_comm);
    assert(NULL != sabo_mpi_module_translate_node_rank);

    return sabo_mpi_module_translate_node_rank[rank];
#else /* #ifdef SABO_USE_MPI */
    UNUSED(rank);
    UNUSED(sabo_mpi_module_translate_node_rank);
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static void mpi_alloc_node_comm(void)
{
#ifdef SABO_USE_MPI
    int rc;
    int node_grp_size;
    int *node_ranks;
    int *world_ranks;
    MPI_Group node_grp;
    MPI_Group world_grp;

    if (likely(MPI_COMM_NULL != sabo_mpi_module_node_comm))
        return;

    rc = PMPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED,
                  mpi_get_world_rank(),
                  MPI_INFO_NULL, &sabo_mpi_module_node_comm);
    SABO_MPI_CHECK("PMPI_Comm_split_type", rc);

    assert(MPI_COMM_NULL != sabo_mpi_module_node_comm);

    rc = PMPI_Comm_group(sabo_mpi_module_node_comm, &node_grp);
    SABO_MPI_CHECK("PMPI_Comm_group", rc);

    PMPI_Comm_group(MPI_COMM_WORLD, &world_grp);
    SABO_MPI_CHECK("PMPI_Comm_group", rc);

    node_grp_size = mpi_get_node_size();
    node_ranks = xzalloc(sizeof(int) * (size_t) node_grp_size);
    world_ranks = xzalloc(sizeof(int) * (size_t) node_grp_size);

    for (int i = 0; i < node_grp_size; i++)
        node_ranks[i] = i;

    rc = PMPI_Group_translate_ranks(node_grp, node_grp_size,
                    node_ranks, world_grp, world_ranks);
    SABO_MPI_CHECK("PMPI_Group_translate_ranks", rc);

    sabo_mpi_module_translate_node_rank = world_ranks;

    rc = PMPI_Group_free(&node_grp);
    SABO_MPI_CHECK("PMPI_Group_free", rc);

    rc = PMPI_Group_free(&world_grp);
    SABO_MPI_CHECK("PMPI_Group_free", rc);

    xfree(node_ranks);
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static void mpi_free_node_comm(void)
{
#ifdef SABO_USE_MPI
    int rc;

    if (unlikely(MPI_COMM_NULL == sabo_mpi_module_node_comm))
        return;

    rc = PMPI_Comm_free(&sabo_mpi_module_node_comm);
    SABO_MPI_CHECK("PMPI_Comm_free", rc);
    sabo_mpi_module_node_comm = MPI_COMM_NULL;

    xfree(sabo_mpi_module_translate_node_rank);
    sabo_mpi_module_translate_node_rank = NULL;
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int sabo_module_mpi_pmpi_init(int *argc, char ***argv)
{
#ifdef SABO_USE_MPI
    UNUSED(argc);
    UNUSED(argv);
    fatal_error("MPI_Init should not be called when using SABO. Please use MPI_Init_thread instead.");
#endif
}

static int sabo_module_mpi_pmpi_init_thread(int *argc, char ***argv, int req, int *prov)
{
#ifdef SABO_USE_MPI
    int rc;
    static int (*__sabo_pmpi_init_thread_func)(int *, char ***, int, int*) = NULL;

    if (unlikely(NULL == __sabo_pmpi_init_thread_func)) {
        void *func;
        char *error;

        dlerror();    /* Clear any existing error */

        func = dlsym(RTLD_NEXT, "PMPI_Init_thread");

        if (unlikely(NULL != (error = dlerror()))) {
            fatal_error("Failed dlsym PMPI_Init_thread (%s)",
                    error);
        }

        * (void **) (&__sabo_pmpi_init_thread_func) = func;
    }

    rc = __sabo_pmpi_init_thread_func(argc, argv, req, prov);
    SABO_MPI_CHECK("PMPI_Init_thread", rc);

    __atomic_store_n(&sabo_mpi_module_initialized,
             1, __ATOMIC_RELAXED);

    return rc;
#else /* #ifdef SABO_USE_MPI */
    UNUSED(argc);
    UNUSED(argv);
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int sabo_module_mpi_pmpi_finalize(void)
{
#ifdef SABO_USE_MPI
    int rc;
    static int (*__sabo_pmpi_fini_func)(void) = NULL;

    if (unlikely(NULL == __sabo_pmpi_fini_func)) {
        void *func;
        char *error;

        dlerror();    /* Clear any existing error */

        func = dlsym(RTLD_NEXT, "PMPI_Finalize");

        if (unlikely(NULL != (error = dlerror()))) {
            fatal_error("Failed dlsym PMPI_Finalize (%s)",
                    error);
        }

        * (void **) (&__sabo_pmpi_fini_func) = func;
    }

    __atomic_store_n(&sabo_mpi_module_initialized,
             0, __ATOMIC_RELAXED);

    rc = __sabo_pmpi_fini_func();
    SABO_MPI_CHECK("PMPI_Finalize", rc);
    return rc;
#else /* #ifdef SABO_USE_MPI */
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static void mpi_allgather(const void *sbuf, void *rbuf, const int count)
{
#ifdef SABO_USE_MPI
    int rc;

    assert(MPI_COMM_NULL != sabo_mpi_module_node_comm);

    rc = PMPI_Allgather(sbuf, count, MPI_DOUBLE, rbuf, count,
                MPI_DOUBLE, sabo_mpi_module_node_comm);
    SABO_MPI_CHECK("PMPI_Allgather", rc);
#else /* #ifdef SABO_USE_MPI */
    UNUSED(sbuf);
    UNUSED(rbuf);
    UNUSED(count);
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

static int mpi_is_initialized(void)
{
#ifdef SABO_USE_MPI
    return __atomic_load_n(&sabo_mpi_module_initialized,
                   __ATOMIC_RELAXED);
#else /* #ifdef SABO_USE_MPI */
    UNUSED(sabo_mpi_module_initialized);
    fatal_error("Please recompile sabo with CFLAGS -DSABO_USE_MPI");
#endif /* #ifdef SABO_USE_MPI */
}

void sabo_module_mpi_register_cb(struct comm_module_funcs *funcs)
{
    funcs->is_initialized = mpi_is_initialized;

    funcs->init = sabo_module_mpi_pmpi_init;
    funcs->init_thread = sabo_module_mpi_pmpi_init_thread;
    funcs->fini = sabo_module_mpi_pmpi_finalize;

    funcs->get_world_rank = mpi_get_world_rank;
    funcs->get_world_size = mpi_get_world_size;
    funcs->get_world_rank_from_node_rank = mpi_get_world_rank_from_node_rank;

    funcs->get_node_rank = mpi_get_node_rank;
    funcs->get_node_size = mpi_get_node_size;

    funcs->alloc_node_comm = mpi_alloc_node_comm;
    funcs->free_node_comm = mpi_free_node_comm;

    funcs->allgather = mpi_allgather;
}
