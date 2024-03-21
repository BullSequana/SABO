/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef __include_hwloc_binding_h
#define __include_hwloc_binding_h

#include "binding.h"

void sabo_hwloc_sys_bind_data_alloc(struct sys_bind_data *data);
void sabo_hwloc_sys_bind_data_free(struct sys_bind_data *data);
void sabo_hwloc_set_thread_affinity(struct sys_bind_data *data);
void sabo_hwloc_get_thread_affinity(struct sys_bind_data *data);

#endif /* ifndef __include_hwloc_binding_h */
