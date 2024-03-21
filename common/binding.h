/*
 * Copyright 2021- 2024 Bull SAS
 */

#ifndef __include_binding_h
#define __include_binding_h

struct sys_bind_data {
    int cur_core_id;
    int new_core_id;
    void *data;
};

void sabo_get_thread_affinity(struct sys_bind_data *data);
void sabo_set_thread_affinity(struct sys_bind_data *data);
void sabo_sys_bind_data_alloc(struct sys_bind_data *data);
void sabo_sys_bind_data_free(struct sys_bind_data *data);

#endif /* #ifndef __include_binding_h */
