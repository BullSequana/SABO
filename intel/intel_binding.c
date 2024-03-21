/*
 * Copyright 2021-2024 Bull SAS
 */

#include "omp.h"
#include "sys.h"
#include "compiler.h"
#include "binding.h"
#include "intel_binding.h"
#include "log.h"

void sabo_intel_sys_bind_data_alloc(struct sys_bind_data *data)
{
#ifdef SABO_RTINTEL
    kmp_affinity_mask_t *mask;
    mask = xzalloc(sizeof(kmp_affinity_mask_t));
    kmp_create_affinity_mask(mask);
    data->data = (void *) mask;
#else /* ifdef SABO_RTINTEL */
    UNUSED(data);
    fatal_error("only available with intel compiler");
#endif /* ifdef SABO_RTINTEL */
}

void sabo_intel_sys_bind_data_free(struct sys_bind_data *data)
{
#ifdef SABO_RTINTEL
    kmp_affinity_mask_t *mask = (kmp_affinity_mask_t *) data->data;
    kmp_destroy_affinity_mask(mask);
    data->data = NULL;
#else /* ifdef SABO_RTINTEL */
    UNUSED(data);
    fatal_error("only available with intel compiler");
#endif /* ifdef SABO_RTINTEL */
}

void sabo_intel_set_thread_affinity(struct sys_bind_data *data)
{
#ifdef SABO_RTINTEL
    int rc;
    kmp_affinity_mask_t *mask;

    if (unlikely(NULL == data->data)) {
        sabo_sys_bind_data_alloc(data);
        mask = (kmp_affinity_mask_t *) data->data;
        if (data->cur_core_id != -1)
            kmp_set_affinity_mask_proc(data->cur_core_id, mask);
    } else {
        mask = (kmp_affinity_mask_t *) data->data;
        kmp_unset_affinity_mask_proc(data->cur_core_id, mask);
    }

#if 0
    if (data->cur_core_id == data->new_core_id)
        return; /* Nothing to do */
#endif

    mask = (kmp_affinity_mask_t *) data->data;

    /* Swap core id in kmp_maks */
    kmp_set_affinity_mask_proc(data->new_core_id, mask);

    debug(LOG_DEBUG_BINDING, "SABO MOVE pid %d tid %d OS proc from %d to %d",
        getpid(), sys_get_tid(), data->cur_core_id,
        data->new_core_id);

    /* Apply mask on thread */
    if ((rc = kmp_set_affinity(mask))) {
        error("Failed to move thread pid %d tid %d OS from %d to %d (rc %d)",
              getpid(), sys_get_tid(), data->cur_core_id,
              data->new_core_id, rc);
    }

    /* swap value in bind data */
    data->cur_core_id = data->new_core_id;
#else /* ifdef SABO_RTINTEL */
    UNUSED(data);
    fatal_error("only available with intel compiler");
#endif /* ifdef SABO_RTINTEL */
}

void sabo_intel_get_thread_affinity(struct sys_bind_data *data)
{
#ifdef SABO_RTINTEL
    int max_proc;
    kmp_affinity_mask_t *mask;

    if (unlikely(NULL == data->data)) {
        data->cur_core_id = -1;
        data->new_core_id = -1;
        sabo_sys_bind_data_alloc(data);
    }

    mask = (kmp_affinity_mask_t *) data->data;
    kmp_get_affinity(mask);

    max_proc = kmp_get_affinity_max_proc();

    /* Search lowest proc in mask */
    for (int i = 0; i < max_proc; i++) {
        int rc =  kmp_get_affinity_mask_proc(i, mask);

        if(!rc)
            continue;

        if (unlikely(-1 == data->cur_core_id)) {
            data->cur_core_id = i;
            continue;
        }

        /* remove other core_id */
        kmp_unset_affinity_mask_proc(i, mask);
    }
#else /* ifdef SABO_RTINTEL */
    UNUSED(data);
    fatal_error("only available with intel compiler");
#endif /* ifdef SABO_RTINTEL */
}
