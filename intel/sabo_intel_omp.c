/*
 * Copyright 2021-2024 Bull SAS
 */

#include "omp.h"
#include "sys.h"
#include "binding.h"
#include "sabo_internal.h"
#include "sabo_intel_omp.h"

static void sabo_intel_read_thread(core_process_t *process)
{
    const int tid = omp_get_thread_num();
    sabo_get_thread_affinity(&(process->binding[tid]));
}

void sabo_intel_omp_discover(core_process_t *process)
{
    /* Force a fork to read omp threads affinity */
    #pragma omp parallel
        sabo_intel_read_thread(process);    
}

static void sabo_intel_move_thread(core_process_t *process)
{
    const int tid = omp_get_thread_num();
    sabo_set_thread_affinity(&(process->binding[tid]));
}

void sabo_intel_omp_rebalance(core_process_t *process)
{
    /* Replace omp_num_threads old value */
    omp_set_num_threads(process->num_threads);

    /* Force a fork to rebind omp threads */
    #pragma omp parallel num_threads(process->num_threads)
        sabo_intel_move_thread(process);
}

double sabo_intel_omp_get_wtime(void)
{
    return omp_get_wtime();
}
