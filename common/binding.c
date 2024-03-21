/*
 * Copyright 2021-2024 Bull SAS
 */

#include "binding.h"
#include "intel_binding.h"
#include "hwloc_binding.h"

void sabo_get_thread_affinity(struct sys_bind_data *data)
{
    sabo_hwloc_get_thread_affinity(data);
}

void sabo_set_thread_affinity(struct sys_bind_data *data)
{
    sabo_hwloc_set_thread_affinity(data);
}

void sabo_sys_bind_data_alloc(struct sys_bind_data *data)
{
    sabo_hwloc_sys_bind_data_alloc(data);
}

void sabo_sys_bind_data_free(struct sys_bind_data *data)
{
    sabo_hwloc_sys_bind_data_free(data);
}
