/*
 * Copyright 2021-2024 Bull SAS
 */


#ifndef include_core_internal_h
#define include_core_internal_h

#include "sys.h"
#include "binding.h"
#include "sabo_ompt.h"

struct core_counters {
    double *delta;
    double *elapsed;
    int *num_threads;
};

struct core_process {
    /* Process MPI ranks */
    int node_rank;
    int world_rank;

    /* New placement infos */
    int socket_id;
    int num_threads;

    /* Previous placement infos */
    int prev_socket_id;
    int prev_num_threads;

    /* Algorithme computed values */
    struct core_counters counters;

    struct sys_bind_data *binding;

    /* ompt thread counters */
    ompt_threads_data_t ompt;
};
typedef struct core_process core_process_t;

int sabo_core_init(void);
void sabo_core_fini(double start_time);

ompt_threads_data_t *sabo_core_get_ompt_data(void);
void sabo_core_reset_ompt_data(void);
int enabled_implicit_balancing(void);

#endif /* #ifndef include_core_internal_h */
