/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_tree_h
#define include_tree_h

#include "list.h"
#include "sabo_internal.h"

void decision_tree_init(const int num_sockets, const int num_cores_per_socket,
            const int num_processes);
void decision_tree_fini(void);

void decision_tree_compute_placement(struct core_process *processes);

#endif /* #ifndef include_tree_h */
