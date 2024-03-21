/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include "log.h"
#include "sys.h"
#include "sabo.h"
#include "sabo_ompt.h"
#include "sabo_internal.h"

#ifdef SABO_RTINTEL
#include <omp.h>

#include <omp-tools.h>

#ifdef SABO_USE_EZTRACE
#include "eztrace.h"
#endif /* #ifdef SABO_USE_EZTRACE */

/**
 * OMPT initialize and finalize
 */

#define register_callback_t(name, type)    \
    do{ \
        type f_##name = &on_##name; \
        if (ompt_set_callback(name, (ompt_callback_t)f_##name) == \
                ompt_set_never) \
        printf("0: Could not register callback '" #name "'\n");    \
    }while(0)

#define register_callback(name) register_callback_t(name, name##_t)

/**
 * OMPT callback
 */

static ompt_get_state_t  ompt_get_state;
static __thread bool __reenter__ = false;

static void
on_ompt_callback_sync_region(ompt_sync_region_t kind,
                 ompt_scope_endpoint_t endpoint,
                 ompt_data_t *parallel_data,
                 ompt_data_t *task_data,
                 const void *codeptr_ra)
{

    /* Silent unsued ompt callback parameters */
    UNUSED(endpoint);
    UNUSED(parallel_data);
    UNUSED(task_data);
    UNUSED(codeptr_ra);

    /* protection against reentrance */
    if (unlikely(__reenter__))
      return;
    __reenter__ = true;

    if (2 == kind || 9 == kind) {
        int tid;
        ompt_state_t thread_state;

        thread_state = (ompt_state_t) ompt_get_state(NULL);

        /* 257 -> hex 0x101 */
        if (ompt_state_overhead != thread_state)
          goto LEAVE;

#ifdef SABO_USE_EZTRACE
        eztrace_enter_event("OMP_SYNC_REGION", EZTRACE_BLUE);
#endif /* #ifdef SABO_USE_EZTRACE */

        /* skip master thread to avoid double counting */
        if (0 != (tid = omp_get_thread_num())) {
            ompt_threads_data_t *data;
            data = sabo_core_get_ompt_data();
            data->elapsed[tid] += omp_get_wtime() - data->start;
        }

#ifdef SABO_USE_EZTRACE
         eztrace_leave_event();
#endif /* #ifdef SABO_USE_EZTRACE */
    }
LEAVE:
  __reenter__ = false;
}

static void
on_ompt_callback_parallel_begin(ompt_data_t *encountering_task_data,
                const ompt_frame_t *encountering_task_frame,
                ompt_data_t *parallel_data,
                unsigned int requested_parallelism,
                int flags, const void *codeptr_ra)
{
    ompt_threads_data_t *data;

    /* Silent unsued ompt callback parameters */
    UNUSED(encountering_task_data);
    UNUSED(encountering_task_frame);
    UNUSED(parallel_data);
    UNUSED(requested_parallelism);
    UNUSED(flags);
    UNUSED(codeptr_ra);

    /* protection against reentrance */
    if (unlikely(__reenter__))
      return;
    __reenter__ = true;

#ifdef SABO_USE_EZTRACE
    eztrace_enter_event("OMP_ZONE_BEGIN", EZTRACE_GREEN);
#endif /* #ifdef SABO_USE_EZTRACE */

    /* callback only call by master thread */
    assert(0 == omp_get_thread_num());

    data = sabo_core_get_ompt_data();
    data->start = omp_get_wtime();
    data->num_calls++;

#ifdef SABO_USE_EZTRACE
    eztrace_leave_event();
#endif /* #ifdef SABO_USE_EZTRACE */
    __reenter__ = false;
}

static void
on_ompt_callback_parallel_end(ompt_data_t *parallel_data,
                  ompt_data_t *task_data,
                  int flags, const void *codeptr_ra)
{
    ompt_threads_data_t *data;

    /* Silent unsued ompt callback parameters */
    UNUSED(parallel_data);
    UNUSED(task_data);
    UNUSED(flags);
    UNUSED(codeptr_ra);

    /* protection against reentrance */
    if (unlikely(__reenter__))
      return;
    __reenter__ = true;

#ifdef SABO_USE_EZTRACE
    eztrace_enter_event("OMP_ZONE_END", EZTRACE_RED);
#endif /* #ifdef SABO_USE_EZTRACE */

    /* callback only call by master thread */
    assert(0 == omp_get_thread_num());

    data = sabo_core_get_ompt_data();
    data->elapsed[0] = omp_get_wtime() - data->start;

    if (enabled_implicit_balancing())
      sabo_omp_balanced();

#ifdef SABO_USE_EZTRACE
    eztrace_leave_event();
#endif /* #ifdef SABO_USE_EZTRACE */
    __reenter__ = false;
}

static int
ompt_initialize(ompt_function_lookup_t lookup,
        int initial_device_num,
        ompt_data_t* data)
{
    ompt_set_callback_t ompt_set_callback;

    /* Silent unsued ompt callback parameters */
    UNUSED(data);
    UNUSED(initial_device_num);

    ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
    ompt_get_state = (ompt_get_state_t) lookup("ompt_get_state");

    sabo_core_init(); /* internal initialization */

    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_sync_region);

    return 1;
}

static void
ompt_finalize(ompt_data_t* data)
{
    sabo_core_fini((double) data->value);
}

ompt_start_tool_result_t* ompt_start_tool(
        unsigned int omp_version,
        const char *runtime_version)
{
    static double time = 0;

    /* Silent unsued ompt callback parameters */
    UNUSED(omp_version);
    UNUSED(runtime_version);

    time = omp_get_wtime();

    static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,
        &ompt_finalize,    {.ptr = &time}};

    return &ompt_start_tool_result;
}

#endif /* #ifdef SABO_RTINTEL */
