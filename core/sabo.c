/*
 * Copyright 2021-2024 Bull SAS
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

/* sabo common */
#include "decision_tree.h"
#include "env.h"
#include "list.h"
#include "log.h"
#include "sys.h"
#include "topo.h"

/* sabo core */
#include "comm.h"
#include "sabo.h"
#include "sabo_internal.h"
#include "sabo_omp.h"
#include "binding.h"

#define SABO_CORE_PRINT_THRESHOLD ((double) 1/100000)

#define SABO_REBALANCING_THRESHOLD ((double) 0.1)

struct core_socket_data {
    int num_processes;
    int num_free_cores;
    core_process_t **processes;
};
typedef struct core_socket_data core_socket_data_t;

struct core_ctx {
    int num_sockets;
    int num_cores_per_socket;
    int node_rank;
    int node_comm_size;
    int num_cores;
    int init_once;

    int implicit_balancing;
    int window;
    int step;

    double cumulate_elapsed;
    double mpi_elapsed;

    core_process_t *myprocess;
    core_process_t *processes;

    core_socket_data_t *data;
};

static struct core_ctx *__sabo_core_ctx = NULL;

static void __attribute__((constructor)) sabo_constructor_init(void)
{
#ifndef SABO_USE_MPI
    comm_init();
#endif /* ifndef SABO_USE_MPI */
}

static void __attribute__((destructor)) sabo_destructor_init(void)
{
    /* Actually unused */
}

static void sabo_core_init_process(core_process_t *process)
{
    void *ptr;
    const int window = __sabo_core_ctx->window;
    const int num_cores = __sabo_core_ctx->num_cores;

    /* MPI comm ranks */
    process->node_rank = -1;
    process->world_rank = -1;

    /* Next value */
    process->num_threads = -1;
    process->socket_id = -1;

    /* Prev value */
    process->prev_num_threads = -1;
    process->prev_socket_id = -1;

    ptr = xzalloc(sizeof(double) * (size_t) num_cores);
    process->ompt.elapsed = (double *) ptr;

    process->counters.delta = xzalloc(sizeof(double) * (size_t) window);
    process->counters.num_threads = xzalloc(sizeof(int) * (size_t) window);
    process->counters.elapsed = xzalloc(sizeof(double) * (size_t) window);
}

static void sabo_core_fini_process(core_process_t *process)
{
    xfree(process->ompt.elapsed);
    process->ompt.elapsed = NULL;

    xfree(process->counters.delta);
    process->counters.delta = NULL;

    xfree(process->counters.num_threads);
    process->counters.num_threads = NULL;

    xfree(process->counters.elapsed);
    process->counters.elapsed = NULL;
}

static void core_discover_placement(core_process_t *process)
{
#if 0
    sabo_intel_omp_discover(process);

    /* convert core_id to socket_id */
    socket_id = topo_get_socket_id_from_core_id(core_id);

    /* save start informations */
    process->socket_id = socket_id;
    process->num_threads = num_threads;
#else
    process->socket_id = -1;
    process->num_threads = env_get_omp_num_threads();
#endif
}

/* Core processes allocations */
static void core_init_context(void)
{
    void *ptr;
    core_process_t *tmp;
    core_process_t *myprocess;

    if (likely(__sabo_core_ctx->init_once))
        return;

    const int num_cores = __sabo_core_ctx->num_cores;
    const int num_sockets = __sabo_core_ctx->num_sockets;
    const int num_cores_per_socket = __sabo_core_ctx->num_cores_per_socket;

    const int node_rank = comm_get_node_rank();
    const int node_comm_size = comm_get_node_size();

    __sabo_core_ctx->node_rank = node_rank;
    __sabo_core_ctx->node_comm_size = node_comm_size;

    ptr = xzalloc(sizeof(core_process_t) * (size_t) node_comm_size);
    __sabo_core_ctx->processes = (core_process_t *) ptr;

    for (int i = 0; i < node_comm_size; i++) {
        core_process_t *process = &(__sabo_core_ctx->processes[i]);
        const int wrank = comm_get_world_rank_from_node_rank(i);

        sabo_core_init_process(process);

        /* MPI comm ranks */
        process->node_rank = i;
        process->world_rank = wrank;

        /* num_threads */
        process->num_threads = env_get_omp_num_threads();
    }

    /* copy myprocess into processes array */
    tmp = __sabo_core_ctx->myprocess;
    myprocess = &(__sabo_core_ctx->processes[node_rank]);
    for (int i = 0; i < __sabo_core_ctx->window; i++) {
        myprocess->ompt.elapsed[i] = tmp->ompt.elapsed[i];
        myprocess->counters.elapsed[i] = tmp->counters.elapsed[i];
    }
    sabo_core_fini_process(tmp);
    xfree(tmp);
    __sabo_core_ctx->myprocess = myprocess;

    myprocess->binding = xzalloc(sizeof(struct sys_bind_data) * (size_t) num_cores);

    for (int i = 0; i < num_cores; i++) {
         myprocess->binding[i].cur_core_id = -1;
    }

    ptr = xzalloc(sizeof(core_socket_data_t) * (size_t) num_sockets);
    __sabo_core_ctx->data = (core_socket_data_t *) ptr;

    /* Pre-allocate core processes */
    for (int i = 0; i < num_sockets; i++) {
        ptr =  xzalloc(sizeof(core_process_t *) * (size_t) node_comm_size);
        __sabo_core_ctx->data[i].processes = (core_process_t **) ptr;
    }

    /* Initialize decision tree */
    decision_tree_init(num_sockets, num_cores_per_socket, node_comm_size);

    core_discover_placement(myprocess);

    __sabo_core_ctx->init_once = 1;
}

static void core_fini_context(void)
{
    if (unlikely((NULL == __sabo_core_ctx)))
        return;

    /* clean sockets */
    if (NULL != __sabo_core_ctx->data) {
        for (int i = 0; i < __sabo_core_ctx->num_sockets; i++)
            xfree(__sabo_core_ctx->data[i].processes);
        xfree(__sabo_core_ctx->data);
    }

    /* Clean processes */
    if (NULL != __sabo_core_ctx->processes) {
        for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++)
               sabo_core_fini_process(&(__sabo_core_ctx->processes[i]));
        xfree(__sabo_core_ctx->processes);
        __sabo_core_ctx->myprocess = NULL;
    }

    xfree(__sabo_core_ctx->myprocess);

    xfree(__sabo_core_ctx);
    __sabo_core_ctx = NULL;
}

static void core_prepare_processes(void)
{
    for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        core_process_t *process = &(__sabo_core_ctx->processes[i]);
        process->prev_num_threads = process->num_threads;
        process->num_threads = process->counters.num_threads[0];
    }
}

static int core_cmp_processes_by_rank(void const *ptr1, void const *ptr2)
{
    core_process_t const *p1 = * (core_process_t * const *) ptr1;
    core_process_t const *p2 = * (core_process_t * const *) ptr2;
    return p1->node_rank - p2->node_rank;
}

static void core_sort_processes_by_rank(core_socket_data_t *data)
{
    qsort(data->processes, (size_t) data->num_processes,
          sizeof(core_process_t *), core_cmp_processes_by_rank);
}

static int core_cmp_processes_by_num_threads(void const *ptr1, void const *ptr2)
{
    int diff;

    core_process_t const *p1 = * (core_process_t * const *) ptr1;
    core_process_t const *p2 = * (core_process_t * const *) ptr2;

    if (0 != (diff = (p1->num_threads - p2->num_threads)))
        return diff;

    return p1->node_rank - p2->node_rank;
}

static void core_sort_processes_by_num_threads(core_socket_data_t *data)
{
    qsort(data->processes, (size_t) data->num_processes,
          sizeof(core_process_t *), core_cmp_processes_by_num_threads);
}

static void core_dispatch_processes(void)
{
    /* Reset socket data */
    for (int i = 0; i < __sabo_core_ctx->num_sockets; i++) {
        core_socket_data_t *data = &(__sabo_core_ctx->data[i]);
        data->num_free_cores = __sabo_core_ctx->num_cores_per_socket;
        data->num_processes = 0;
    }

    /* Add process in right socket processes list */
    for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        core_socket_data_t *data;
        core_process_t *process = &(__sabo_core_ctx->processes[i]);

        assert(process->socket_id >= 0);
        assert(process->socket_id < __sabo_core_ctx->num_sockets);

        data = &(__sabo_core_ctx->data[process->socket_id]);
        data->num_free_cores -= process->num_threads;
        data->processes[data->num_processes] = process;
        data->num_processes++;
    }

    /* Sort socket processes array */
    for (int i = 0; i < __sabo_core_ctx->num_sockets; i++)
        core_sort_processes_by_num_threads(&(__sabo_core_ctx->data[i]));
}

static int core_skip_compute(const int step)
{
    int compute;

    const int stepbal = env_get_stepbal();
    const int periodic = env_get_periodic();

    if (periodic) {
        compute = (((step + 1) % stepbal) == 0 ) ? 1 : 0;
    } else {
        compute = (step == stepbal) ? 1 : 0;
    }

    if (!compute)
        return 1;

    debug(LOG_DEBUG_CORE, "step = %d window = %d", step + 1,
          __sabo_core_ctx->window);

    /* call for balancing in early steps withous accumulating
     * enough step times knowledge */
    if (__sabo_core_ctx->window > step + 1)
        return 1;

    return 0;
}

static int sabo_exchange_process_step_data(void)
{
    int idx;
    double start;
    double *recv_buffer;
    core_process_t *process;

    const int window = __sabo_core_ctx->window;

    process = __sabo_core_ctx->myprocess;
    recv_buffer = comm_get_recv_buffer(window);

    start = sabo_omp_get_wtime();
    comm_allgather(process->counters.elapsed,
               recv_buffer, window);
    __sabo_core_ctx->mpi_elapsed = sabo_omp_get_wtime() - start;

    /* Dispatch receive data into dedicated process */
    idx = 0;
    for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        process = &(__sabo_core_ctx->processes[i]);
        for (int j = 0; j < window; j++) {
            process->counters.elapsed[j] = recv_buffer[idx++];
        }
    }

    comm_allgather((double*) process->counters.num_threads,
               recv_buffer, window);
    idx = 0;
    for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        process = &(__sabo_core_ctx->processes[i]);
        for (int j = 0; j < window; j++) {
            process->counters.num_threads[j] = (int) recv_buffer[idx++];
        }
    }
    return 0;
}

#if 0
static double sabo_compute_maxtemps_on_last_step(void)
{
    double maxtemps = 0.0;

    for(int i = 0; i < comm_get_node_comm_size(); i++ ) {
        const int idx = (i + 1) * num_steps_exchanged - 1;
        if (maxtemps < process_counters->recvbuf[idx])
            maxtemps = process_counters->recvbuf[idx];
    }

    return maxtemps;
}
#endif

static double sabo_compute_step_sum(const int step)
{
    double sum = (double) 0;

    /* Sum all threads time cycle of a step on node */
    for(int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        const core_process_t *process = &(__sabo_core_ctx->processes[i]);
        sum += process->counters.elapsed[step];
    }

    return sum;
}

static core_process_t *sabo_search_step_max_delta(const int idx)
{
    core_process_t *mprocess = &(__sabo_core_ctx->processes[0]);

    for (int i = 1 ; i < __sabo_core_ctx->node_comm_size; i++) {
        core_process_t *process;

        const double maxval = mprocess->counters.delta[idx];
        const int max_num_threads = mprocess->counters.num_threads[idx];

        process = &(__sabo_core_ctx->processes[i]);

        /* Already got max values */
        if (process->counters.delta[idx] < maxval)
            continue;

        /* Found a new max value */
        if (process->counters.delta[idx] > maxval) {
            mprocess = process;
            continue;
        }

        /* Value are equal, get the proces with the less num_threads */
        if (process->counters.num_threads[idx] > max_num_threads)
            continue;

        mprocess = process;
    }

    return mprocess;
}

static int core_update_step_process_counters(core_process_t *process,
                         const int step, const double sum)
{
    int num_threads;
    double tmp;
    double delta;

    /* get the number of cores required by this process */
    tmp = process->counters.elapsed[step];
    tmp = (tmp / sum) * ((double) __sabo_core_ctx->num_cores);
    num_threads = (int) floor(tmp);

    /* keep the remainder of tmp that could not be allocated */
    delta = tmp - (double) num_threads;

    /* Process need at least 1 thread */
    if (unlikely(0 == num_threads)) {
        num_threads++;
        delta = (double) 0;
    }

    /* Process is assigned on an uniq socket */
    if (unlikely(__sabo_core_ctx->num_cores_per_socket < num_threads)) {
        num_threads = __sabo_core_ctx->num_cores_per_socket;
        delta = (double) -1;
    }

    ndebug(LOG_DEBUG_CORE, "process #%d step #%d tmp: %.3f "
           "num_threads: %02d delta: %.3f (elapsed: %.2f sum: %.3f)",
           process->world_rank, step, tmp, num_threads,
           process->counters.delta[step],
           process->counters.elapsed[step], sum);

    process->counters.num_threads[step] = num_threads;
    process->counters.delta[step] = delta;

    return process->counters.num_threads[step];
}

static int core_update_step_processes_counters(const int step)
{
    int num_cores = 0;
    const double sum = sabo_compute_step_sum(step);

    for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        core_process_t *process = &(__sabo_core_ctx->processes[i]);
        num_cores += core_update_step_process_counters(process, step, sum);
    }

    return num_cores;
}

static void core_dispatch_unused_cores(const int remaining, const int step)
{
    for (int i = 0; i < remaining; i++) {
        int num_threads;
        core_process_t *process;

        process    = sabo_search_step_max_delta(step);

        /* give him one more thread */
        num_threads = process->counters.num_threads[step] + 1;
        assert(num_threads <= __sabo_core_ctx->num_cores_per_socket);

        process->counters.delta[step] = (double) 0;

        if (unlikely(__sabo_core_ctx->num_cores_per_socket < num_threads))
            process->counters.delta[step] = (double) -1;

        process->counters.num_threads[step] = num_threads;
    }
}

/* From recvbuff where all process omp time of last num_steps_exchanged
 * are gathered lets compute what would be their required nb cores of
 * each last num_steps_exchanged steps */
static void core_compute_step_num_threads(void)
{
    for (int i = 0; i < __sabo_core_ctx->window; i++) {
        int remaining;

        /* compute remaining cores */
        remaining = __sabo_core_ctx->num_cores;
        remaining -= core_update_step_processes_counters(i);

        /* allocated cores are necessarily <= total num_cores */
        ndebug(LOG_DEBUG_CORE, "remaining %d num_core: %d allocate: %d",
               remaining, __sabo_core_ctx->num_cores,
               __sabo_core_ctx->num_cores - remaining);

        /* need to dispatch the remaining cores to processes,
           let's do it by max remainders, after all they wanted more */
        core_dispatch_unused_cores(remaining, i);
    }
}

static int core_compute_step_process_avg(core_process_t *process)
{
    int num_threads;
    double avg;
    double delta;

    const int window = __sabo_core_ctx->window;

    avg = (double) 0;
    for (int i = 0; i < window; i++)
        avg += (double) process->counters.num_threads[i];

    avg /= (double) window;
    num_threads = (int) floor(avg);
    delta = avg - (double) num_threads;

    if (unlikely(__sabo_core_ctx->num_cores_per_socket == num_threads))
        delta = (double) -1;

    /* update_process_step_data computing propreties */
    assert(num_threads <= __sabo_core_ctx->num_cores_per_socket);
    assert(0 < num_threads);

    ndebug(LOG_DEBUG_CORE, "wrank #%3d nrank #%3d avg: %f num_threads: %d "
           "delta: %f", process->world_rank, process->node_rank,
           avg, num_threads, delta);

    /* Overwritting first entry */
    process->counters.num_threads[0] = num_threads;
    process->counters.delta[0] = delta;

    return process->counters.num_threads[0];
}

static void core_compute_average_step_num_threads(void)
{
    int allocate;
    int remaining;

    allocate = 0;
    for (int i = 0; i < __sabo_core_ctx->node_comm_size; i++) {
        core_process_t *process;
        process    = &(__sabo_core_ctx->processes[i]);
        allocate += core_compute_step_process_avg(process);
    }

    /* compute remaining cores */
    remaining  = __sabo_core_ctx->num_cores - allocate;

    /* allocated cores are necessarily <= total num_cores */
    ndebug(LOG_DEBUG_CORE, "remaining %d num_core: %d allocate: %d",
           remaining, __sabo_core_ctx->num_cores, allocate);

    /* need to dispatch the remaining cores to processes,
       let's do it by max remainders, after all they wanted more */
    core_dispatch_unused_cores(remaining, 0);
}

static int core_estimate_speedup(void) {
    int proc_num_threads = 0;
    int total_num_threads = 0;
    double total_time = (double) 0;
    double proc_time = (double) 0;
    double gain_proc;
    double gain_tot;

    for (int proc_idx = 0 ; proc_idx < __sabo_core_ctx->node_comm_size ; proc_idx++) {
        core_process_t *process = &(__sabo_core_ctx->processes[proc_idx]);

        total_num_threads += process->num_threads;
        if (proc_idx == __sabo_core_ctx->node_rank) {
            proc_num_threads += process->num_threads;
        }

        for (int step_idx = 0 ; step_idx < __sabo_core_ctx->window ; step_idx++) {
            total_time += process->counters.elapsed[step_idx];

            if (proc_idx == __sabo_core_ctx->node_rank) {
                proc_time += process->counters.elapsed[step_idx];
            }
        }
    }

    gain_proc = proc_time / (double) proc_num_threads;
    gain_tot = total_time / (double) total_num_threads;

    debug(LOG_DEBUG_CORE, "process #%d step #%d threads: %d / %d "
          "(time: %.3f / %.3f), gain: %.3f / %.3f",
          __sabo_core_ctx->node_rank, __sabo_core_ctx->step,
          proc_num_threads, total_num_threads,
          proc_time, total_time,
          gain_proc, gain_tot);

    if (fabs(gain_tot - gain_proc) < gain_tot * SABO_REBALANCING_THRESHOLD) {
        debug(LOG_DEBUG_CORE, "cancel rebalancing");
        return 1;
    }

    return 0;
}

static void core_compute_new_threads_distribution(void)
{
    core_compute_step_num_threads();
    core_compute_average_step_num_threads();
}

#if 0 /* Unused */
static double core_found_last_step_max_elapsed(void)
{
    double max_elapsed = 0.0;

    for (int i = 0; i < data->num_processes; i++) {
        int step;
        core_process_t *process;

        process = data->processes[i];
        step = process->counters.step;

        if (process->counters.elapsed[step] <= max_elapsed)
            continue;

        max_elapsed = process->counters.elapsed[step];
    }

    return max_elapsed;
}

static int core_compute_list_stop_value(core_socket_data_t *data)
{
    int stop;

    max_elapsed = core_found_last_step_max_elapsed();

    /* Compute stop value for each process */
    for (int i = 0; i < data->num_processes; i++) {
        double val;
        core_process_t *process = data->processes[i];

        /* Compute value */
        val = process->counters.elapsed[step];
        val *= process->prev_num_threads;
        val /= process->num_threads;

        if (val >= elapsed)
            return 1;
    }

    return 0;
}
#endif /* unused */

static void core_adjust_list_num_threads(core_socket_data_t *data)
{
    while(data->num_free_cores < 0) { /* Too many cores assigned */
        for (int i = 0; i < data->num_processes; i++) {
            core_process_t *process = data->processes[i];

            if (unlikely(1 == process->num_threads))
                break;

            process->num_threads--;
            data->num_free_cores++;
        }
    }

    while (data->num_free_cores > 0) { /* Too few cores assigned */
        for (int i = 0; i < data->num_processes; i++) {
            core_process_t *process = data->processes[i];

            process->num_threads++;
            data->num_free_cores--;
        }
    }
}

static void core_adjust_num_threads(void)
{
    for (int i = 0; i < __sabo_core_ctx->num_sockets; i++) {
        core_socket_data_t *data = &(__sabo_core_ctx->data[i]);
        core_adjust_list_num_threads(data);
    }
}

static int core_compute_first_core_id(const core_socket_data_t *data,
                      const int node_rank)
{
    int start = 0;

    /* Compute my first core _id on socket */
    for (int i = 0; i < data->num_processes; i++) {
        if (data->processes[i]->node_rank == node_rank)
            break;
        start += data->processes[i]->num_threads;
    }

    return start;
}

static void core_apply_new_placement(core_process_t *process)
{
    int start;

    const int socket_id = process->socket_id;

    /* Sort processes by rank to preserve deterministic placement */
    core_sort_processes_by_rank(&(__sabo_core_ctx->data[socket_id]));

    /* Compute process first core */
    start = core_compute_first_core_id(&(__sabo_core_ctx->data[socket_id]),
                       process->node_rank);

    debug(LOG_DEBUG_CORE, "process wrank #%3d nrank %3d/%3d on socket "
          "#%2d got %3d thread(s) prev %3d thread(s)", process->world_rank,
          process->node_rank, __sabo_core_ctx->data[socket_id].num_processes,
          process->socket_id, process->num_threads, process->prev_num_threads);

    if (process->num_threads == process->prev_num_threads &&
        process->binding[0].cur_core_id == start &&
        process->socket_id == process->prev_socket_id) {
        debug(LOG_DEBUG_CORE, "Nothing to do");
        return; /* nothing to do */
    }

    /* Iterate on socket core */
    for (int i = 0; i < process->num_threads; i++) {
        int cpu = start + i;
        cpu = topo_get_socket_core_id(process->socket_id, cpu);
        process->binding[i].new_core_id = cpu;
    }

    if (likely(!env_get_no_rebalance()))
        sabo_omp_rebalance(process);
}

static void core_gather_ompt_counters(core_process_t *process)
{
    int step;
    double sum = (double) 0;

    step = __sabo_core_ctx->step % __sabo_core_ctx->window;

    for (int i = 0; i < __sabo_core_ctx->num_cores; i++)
        sum += process->ompt.elapsed[i];

    process->counters.elapsed[step] = sum;
}

ompt_threads_data_t *sabo_core_get_ompt_data(void)
{
    return &(__sabo_core_ctx->myprocess->ompt);
}

void sabo_core_reset_ompt_data(void)
{
    ompt_threads_data_t *ompt_data = sabo_core_get_ompt_data();

    for (int i = 0; i < __sabo_core_ctx->num_cores; i++)
        ompt_data->elapsed[i] = (double) 0;
}

int enabled_implicit_balancing(void)
{
    return __sabo_core_ctx->implicit_balancing;
}
/**
 * OMP balanced
 **/
void __sabo_omp_balanced__(void)
{
    double elapsed;

    const double start = sabo_omp_get_wtime();

    /* Sum all threads time cycle get by OMPT
     * keep track of current step time in tab of all step times */
    core_gather_ompt_counters(__sabo_core_ctx->myprocess);

    /* No comm interface available */
    if (unlikely(!comm_is_initialized()))
        goto LEAVE;

    if(core_skip_compute(__sabo_core_ctx->step))
        goto LEAVE;

    /*  First sabo_omp_balanced with comm interface */
    core_init_context();

    sabo_exchange_process_step_data();

    if (core_estimate_speedup())
        goto LEAVE;

    /* recompute num_threads based on ompt counters */
    core_compute_new_threads_distribution();

    /* Build process list with requested num threads */
    core_prepare_processes();

    /* Compute best placement with branch & cut algorithme */
    decision_tree_compute_placement(__sabo_core_ctx->processes);

    /* Dispatch processes into socket processes list */
    core_dispatch_processes();

    /* Adapt processes num_threads to match num_cores_per_socket */
    core_adjust_num_threads();

    /* Set omp_num_threads and rebind omp threads */
    core_apply_new_placement(__sabo_core_ctx->myprocess);

LEAVE:
    __sabo_core_ctx->step++;
    sabo_core_reset_ompt_data();

    elapsed = (sabo_omp_get_wtime() - start);
    __sabo_core_ctx->cumulate_elapsed += elapsed;

#ifndef NDEBUG
    if (SABO_CORE_PRINT_THRESHOLD > elapsed)
        return;

    debug(LOG_DEBUG_PERF, "Compute %s in %.3f usec(s) cumulate %.6f second(s)",
          __func__, elapsed * 1000000, __sabo_core_ctx->cumulate_elapsed);
#endif /* #ifndef NDEBUG */
}

int sabo_core_init(void)
{
    env_variables_init();
    topo_init();

    const int num_sockets = topo_get_num_sockets();
    const int num_cores_per_socket = topo_get_num_cores_per_socket();

    __sabo_core_ctx = xzalloc(sizeof(struct core_ctx));

    __sabo_core_ctx->window = env_get_num_steps_exchanged();
    __sabo_core_ctx->num_sockets = num_sockets;
    __sabo_core_ctx->num_cores_per_socket = num_cores_per_socket;
    __sabo_core_ctx->num_cores = num_sockets * num_cores_per_socket;
    __sabo_core_ctx->implicit_balancing = env_get_implicit_balancing();

    /* Allocate one process to collect ompt data */
    __sabo_core_ctx->myprocess = xzalloc(sizeof(core_process_t));
    sabo_core_init_process(__sabo_core_ctx->myprocess);

    return 0;
}

void sabo_core_fini(double start_time)
{
    UNUSED(start_time);

    env_variables_fini();
    topo_fini();
    decision_tree_fini();

    if (NULL == __sabo_core_ctx)
        return;

    debug(LOG_DEBUG_PERF, "%s cumulate time %.2f second(s) "
          "(mpi: %.6f algo: %.6f)", "sabo_omp_balanced",
          __sabo_core_ctx->cumulate_elapsed, __sabo_core_ctx->mpi_elapsed,
          __sabo_core_ctx->cumulate_elapsed - __sabo_core_ctx->mpi_elapsed);

    core_fini_context();
    __sabo_core_ctx = NULL;

}
