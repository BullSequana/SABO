/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef  __include_topo_h__
#define  __include_topo_h__

#include "hwloc.h"

void topo_init(void);
void topo_fini(void);

int topo_get_num_sockets(void);
int topo_get_num_cores(void);
int topo_get_num_cores_per_socket(void);

int topo_get_socket_core_id(int socket_id, int local_core_id);
int topo_get_socket_id_from_core_id(const int core_id);

int topo_get_thread_cpubind(void);
hwloc_topology_t topo_get_hwloc_topology(void);

#endif /* #ifndef  __include_topo_h__ */
