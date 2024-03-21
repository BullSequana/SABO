/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_env_h
#define include_env_h

#include <stddef.h>

void env_variables_init(void);
void env_variables_fini(void);

int env_get_world_num_tasks(void);
int env_get_world_task_id(void);

int env_get_node_num_tasks(void);
int env_get_node_task_id(void);

void env_get_shared_node_filename(char *string, size_t size);
int env_get_hwloc_xml_file(char *string, size_t size);

int env_get_no_rebalance(void);
int env_get_stepbal(void);
int env_get_periodic(void);
int env_get_num_steps_exchanged(void);
int env_get_omp_num_threads(void);
void env_get_log_debug(void);
int env_get_implicit_balancing(void);

#endif /* #ifndef include_env_h */
