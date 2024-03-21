/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "sys.h"
#include "decision_tree.h"

static void test_article_example(void)
{
    core_process_t *process, *processes;

    const int num_sockets = 2;
    const int num_cores_per_socket = 24;
    const int num_processes = 4;

    processes = xzalloc(sizeof(core_process_t) * (size_t) num_processes);

    /*  build processes list in desorder */

    /* Process 1 */
    process = &(processes[1]);
    process->node_rank = 1;
    process->prev_socket_id = 0;
    process->prev_num_threads = 12;
    process->socket_id = -1;
    process->num_threads = 9;

    /* Process 0 */
    process = &(processes[0]);
    process->node_rank = 0;
    process->prev_socket_id = 0;
    process->prev_num_threads = 12;
    process->socket_id = -1;
    process->num_threads = 14;

    /* Process 2 */
    process = &(processes[2]);
    process->node_rank = 2;
    process->prev_socket_id = 1;
    process->prev_num_threads = 12;
    process->socket_id = -1;
    process->num_threads = 13;

    /* Process 3 */
    process = &(processes[3]);
    process->node_rank = 3;
    process->prev_socket_id = 1;
    process->prev_num_threads = 12;
    process->socket_id = -1;
    process->num_threads = 12;

    decision_tree_init(num_sockets, num_cores_per_socket, num_processes);

    decision_tree_compute_placement(processes);

    decision_tree_fini();

    xfree(processes);
}

static void test_article_milan64_4ppn_1node(void)
{
    core_process_t *processes;

    const int num_sockets = 2;
    const int num_cores_per_socket = 64;
    const int num_processes = 4;

    processes = xzalloc(sizeof(core_process_t) * (size_t) num_processes);

    for (int i = 0; i < num_processes; i++) {
        core_process_t *process = &(processes[i]);
        process->node_rank = i;
        process->prev_socket_id = -1;
        process->prev_num_threads = 32;
        process->socket_id = -1;
        process->num_threads = 32;
    }

    decision_tree_init(num_sockets, num_cores_per_socket, num_processes);

    decision_tree_compute_placement(processes);

    decision_tree_fini();

    xfree(processes);
}

int main(int argc, char *argv[])
{
    /* Silent unsued main arguments */
    UNUSED(argc);
    UNUSED(argv);

    test_article_example();

    test_article_milan64_4ppn_1node();

    printf("all done\n");
    return EXIT_SUCCESS;
}
