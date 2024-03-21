/*
 * Copyright 2021-2024 Bull SAS
 */


#include "sabo_omp.h"
#include "sabo_internal.h"
#include "sabo_intel_omp.h"

void sabo_omp_discover(core_process_t *process)
{
    sabo_intel_omp_discover(process);
}

void sabo_omp_rebalance(core_process_t *process)
{
    sabo_intel_omp_rebalance(process);
}

double sabo_omp_get_wtime(void)
{
    return sabo_intel_omp_get_wtime();
}
