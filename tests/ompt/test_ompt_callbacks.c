/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <omp.h>

#include "log.h"
#include "sabo_internal.h"
#include "sys.h"
#include "topo.h"

#if defined(SABO_RTINTEL)
static int doubles_are_equal(double val1, double val2)
{
    double diff = fabs(val1 - val2);
    return (diff < DBL_EPSILON) ? 1 : 0;
}

static int double_is_zero(double val)
{
    return doubles_are_equal(val, 0.0);
}

static int test_one_parallel_region_balanced(const int num_threads)
{
    int i, num_cores;

    print("%s: start", __func__);

    print("%s: warmup", __func__);
    /* warmup - force first parallel region */
    #pragma omp parallel num_threads(num_threads)
    {
        sleep(1);
    }

    num_cores = topo_get_num_cores();

    /* Reset ompt counters */
    sabo_core_reset_ompt_data();

    print("%s: measure", __func__);
    #pragma omp parallel num_threads(num_threads)
    {
        sleep(1);
    }

    print("%s: display threads time cycle(s)", __func__);
    for (i = 0; i < num_threads; i++ ) {
        ompt_threads_data_t *ompt_data;

        ompt_data = sabo_core_get_ompt_data();

        print("%s: thread #%d : %.3f time cycle(s)",
              __func__, i, ompt_data->elapsed[i]);
    }

    /* Sanity check for unused thread counters */
    for (i = num_threads; i < num_cores; i++) {
        ompt_threads_data_t *ompt_data;

        ompt_data = sabo_core_get_ompt_data();

        if (double_is_zero(ompt_data->elapsed[i]))
            continue;

        error("unexpected value %.3f (thread #%d)",
              ompt_data->elapsed[i], i);

        return -1;
    }

    sabo_core_reset_ompt_data();

    return 0;
}

static int test_one_parallel_region_unbalanced(const int num_threads)
{
    int i, num_cores;

    print("%s: start", __func__);

    print("%s: warmup", __func__);

    /* warmup - force first parallel region */
    #pragma omp parallel num_threads(num_threads)
    {
        sleep(1);
    }

    num_cores = topo_get_num_cores();

    /* Reset ompt counters */
    sabo_core_reset_ompt_data();

    print("%s: measure", __func__);
    #pragma omp parallel num_threads(num_threads)
    {
        const int tid = omp_get_thread_num();
        if (tid < num_threads / 2)
            sleep(1);
    }

    print("%s: display threads time cycle(s)", __func__);
    for (i = 0; i < num_threads; i++ ) {
        ompt_threads_data_t *ompt_data;

        ompt_data = sabo_core_get_ompt_data();

        print("%s: thread #%d : %.6f time cycle(s)",
              __func__, i, ompt_data->elapsed[i]);
    }

    /* Sanity check for unused thread counters */
    for (i = num_threads; i < num_cores; i++) {
        ompt_threads_data_t *ompt_data;

        ompt_data = sabo_core_get_ompt_data();

        if (double_is_zero(ompt_data->elapsed[i]))
            continue;

        error("unexpected value %.3f (thread #%d)",
              ompt_data->elapsed[i], i);

        return -1;
    }

    sabo_core_reset_ompt_data();

    return 0;
}

static int test_two_parallel_region_balanced(const int num_threads)
{
    int i, num_cores;

    print("%s: start", __func__);

    print("%s: warmup", __func__);

    /* warmup - force first parallel region */
    #pragma omp parallel num_threads(num_threads)
    {
        sleep(1);
    }

    num_cores = topo_get_num_cores();

    /* Reset ompt counters */
    sabo_core_reset_ompt_data();

    print("%s: measure", __func__);
    #pragma omp parallel num_threads(num_threads)
    {
        sleep(1);
    }

    #pragma omp parallel num_threads(num_threads)
    {
        sleep(1);
    }

    print("%s: display threads time cycle(s)", __func__);
    for (i = 0; i < num_threads; i++ ) {
        ompt_threads_data_t *ompt_data;

        ompt_data = sabo_core_get_ompt_data();

        print("%s: thread #%d : %.6f time cycle(s)",
              __func__, i, ompt_data->elapsed[i]);
    }

    /* Sanity check for unused thread counters */
    for (i = num_threads; i < num_cores; i++) {
        ompt_threads_data_t *ompt_data;

        ompt_data = sabo_core_get_ompt_data();

        if (double_is_zero(ompt_data->elapsed[i]))
            continue;

        error("unexpected value %.3f (thread #%d)",
              ompt_data->elapsed[i], i);

        return -1;
    }

    sabo_core_reset_ompt_data();

    return 0;
}
#endif /* #if defined(SABO_RTINTEL) */

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
#if defined(SABO_RTINTEL)
    if (0 > test_one_parallel_region_balanced(4))
        return EXIT_FAILURE;

    if (0 > test_one_parallel_region_unbalanced(4))
        return EXIT_FAILURE;

    if (0 > test_two_parallel_region_balanced(4))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
#else /* #if defined(SABO_RTINTEL) */
    print("Define SABO_RTINTEL to enable this tests");
    return EXIT_SUCCESS;
#endif /* #if defined(SABO_RTINTEL) */
}
