/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef __include_intel_binding_h
#define __include_intel_binding_h

#include "binding.h"

void sabo_intel_sys_bind_data_alloc(struct sys_bind_data *data);
void sabo_intel_sys_bind_data_free(struct sys_bind_data *data);
void sabo_intel_set_thread_affinity(struct sys_bind_data *data);
void sabo_intel_get_thread_affinity(struct sys_bind_data *data);

#endif /* ifndef __include_intel_binding_h */
