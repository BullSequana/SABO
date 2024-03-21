/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef __include_sabo_intel_omp_h
#define __include_sabo_intel_omp_h

#include "sabo_internal.h"

void sabo_intel_omp_discover(core_process_t *process);
void sabo_intel_omp_rebalance(core_process_t *process);
double sabo_intel_omp_get_wtime(void);

#endif /* #ifndef __include_sabo_intel_omp_h */
