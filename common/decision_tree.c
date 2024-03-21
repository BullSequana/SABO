/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include "decision_tree.h"
#include "list.h"
#include "log.h"
#include "sabo_internal.h"
#include "sabo_omp.h"
#include "sys.h"
#include "topo.h"

#define DECISION_NODE_THRESHOLD 64

struct tree_process {
    /* Speed-up core_processes update */
    struct core_process *core_process;

    /* Tree process data */
    int num_threads;
    int socket_id;
    int prev_socket_id;
    int rank;
};

struct tree_socket_data {
    int num_free_cores;
    int total_num_threads;
};

struct tree_node {
    struct list_node list_node;

    /* node info */
    int min;
    int norme;
    int num_socket_changes;
    int placed_num_processes;

    /* debug info */
    int depth;
    int pad0;

    /* Actual state on node */
    struct tree_process *processes;

    /* Socket data */
    struct tree_socket_data *data;

    /* Best candidate */
    struct tree_node *best;

    /* Node father */
    struct tree_node *father;
};

struct tree_ctx {
    int num_sockets;
    int num_cores_per_socket;
    int num_processes;
    int num_free_nodes;

    struct listm_head free_nodes;
};

static struct tree_ctx *__sabo_tree_ctx = NULL;

static void tree_init_node(struct tree_node *node, const int num_sockets,
               const int num_processes)
{
    void *ptr;

    ptr = xzalloc(sizeof(struct tree_socket_data) * (size_t) num_sockets);
    node->data = (struct tree_socket_data *) ptr;

    /* Allocate unassigned processes array */
    ptr = xzalloc(sizeof(struct tree_process) * (size_t) num_processes);
    node->processes = (struct tree_process *) ptr;
}

static void tree_fini_node(struct tree_node *node)
{
    xfree(node->processes);
    xfree(node->data);
}

static struct tree_node *tree_alloc_node(void)
{
    void *ptr;
    struct tree_node *node;

    ptr = listm_get_first(&(__sabo_tree_ctx->free_nodes));
    node = (struct tree_node *) ptr;

    if(likely(NULL != node)) {
        listm_remove(&(__sabo_tree_ctx->free_nodes), (void *) node);
        assert(NULL != node->processes);
        __sabo_tree_ctx->num_free_nodes--;
    } else { /* Allocate new node */
        node = xzalloc(sizeof(struct tree_node));
        tree_init_node(node, __sabo_tree_ctx->num_sockets,
                   __sabo_tree_ctx->num_processes);
    }

    return node;
}

static void tree_free_node(struct tree_node *node)
{
    if (unlikely(NULL == node)) /* mimic free behaviours */
        return;

    /* Add first to minimize default page on tree_alloc_node */
    if(likely(DECISION_NODE_THRESHOLD > __sabo_tree_ctx->num_free_nodes)) {
        listm_add_first(&(__sabo_tree_ctx->free_nodes), (void *) node);
        __sabo_tree_ctx->num_free_nodes++;
        return;
    }

    tree_fini_node(node);
    xfree(node);
}

void tree_init_nodes_list(const int num_sockets, const int num_processes);

void tree_init_nodes_list(const int num_sockets, const int num_processes)
{
    const size_t offset = LIST_OFFSET(struct tree_node, list_node);

    listm_head_init(&(__sabo_tree_ctx->free_nodes), offset);

    __sabo_tree_ctx->num_free_nodes = 0;

    for (int i = 0; i < DECISION_NODE_THRESHOLD; i++) {
        struct tree_node *node = xzalloc(sizeof(struct tree_node));
        tree_init_node(node, num_sockets, num_processes);
        tree_free_node(node);    /* Add to free node pool    */
    }
}

static void tree_fini_nodes_list(void)
{
    void *ptr;

    if (NULL == __sabo_tree_ctx)
        return;

    while (NULL != (ptr = listm_get_first(&(__sabo_tree_ctx->free_nodes)))) {
        listm_remove(&(__sabo_tree_ctx->free_nodes), ptr);
        tree_fini_node((struct tree_node *) ptr);
        xfree(ptr);
    }
}

/* Sorted processes by num_threads */
static int tree_cmp_processes_by_num_threads(void const *ptr1, void const *ptr2)
{
    struct tree_process const *p1 = (struct tree_process const *) ptr1;
    struct tree_process const *p2 = (struct tree_process const *) ptr2;
    return p1->num_threads - p2->num_threads;
}

static void tree_sort_processes_by_num_threads(struct tree_node *node)
{
    qsort(node->processes, (size_t) __sabo_tree_ctx->num_processes,
          sizeof(struct tree_process), tree_cmp_processes_by_num_threads);
}

static struct tree_node *tree_alloc_root_node(struct core_process *processes)
{
    struct tree_node *root;

    root = tree_alloc_node();

    root->min = INT_MIN;
    root->norme = INT_MIN;
    root->father = NULL;
    root->best = NULL;
    root->num_socket_changes = 0;
    root->placed_num_processes = 0;

    root->depth = 0;

    for (int i = 0; i < __sabo_tree_ctx->num_sockets; i++)
           root->data[i].num_free_cores = __sabo_tree_ctx->num_cores_per_socket;

    /* Copy core_process datas */
    for (int i = 0; i < __sabo_tree_ctx->num_processes; i++) {
        struct tree_process *tree_process = &(root->processes[i]);

        /* Copy data */
        tree_process->num_threads = processes[i].num_threads;
        tree_process->prev_socket_id = processes[i].prev_socket_id;
        tree_process->socket_id = -1;    /* undefined value */
        tree_process->rank =  processes[i].node_rank;
        tree_process->core_process = &(processes[i]);
    }

    /* Sort process by requested num threads */
    tree_sort_processes_by_num_threads(root);

    return root;
}

static struct tree_node *tree_dup_node(struct tree_node *dup_node,
                       struct tree_node *node)
{
    dup_node->min = node->min;
    dup_node->norme = node->norme;
    dup_node->num_socket_changes = node->num_socket_changes;
    dup_node->placed_num_processes = node->placed_num_processes;

    dup_node->depth = node->depth + 1;

    /* Copy processes data */
    for (int i = 0; i < __sabo_tree_ctx->num_processes; i++)
        dup_node->processes[i] = node->processes[i];

    /* Copy socket data */
    for (int i = 0; i < __sabo_tree_ctx->num_sockets; i++)
        dup_node->data[i] = node->data[i];

    dup_node->best = NULL;
    dup_node->father = node;

    return dup_node;
}

static void tree_update_node(struct tree_node *node, const int socket_id)
{
    struct tree_process *process;

    /* Get next unassigned processes */
    process = &(node->processes[node->placed_num_processes]);

    /* Update ndoe counters */
    node->data[socket_id].num_free_cores -= process->num_threads;
    process->socket_id = socket_id;
    node->placed_num_processes++;

    debug(LOG_DEBUG_DECISION_TREE, "Place process #%d on socket #%d",
           process->rank, socket_id);

    if (process->prev_socket_id != socket_id)
        node->num_socket_changes++;

    if (0 < node->data[socket_id].num_free_cores)
        return;

    if (INT_MIN == node->norme) { /* never update */
        node->norme = node->data[socket_id].num_free_cores;
    } else {
        node->norme += node->data[socket_id].num_free_cores;
    }
}

#ifndef NDEBUG
static void tree_dump_placement(struct tree_node *node)
{
    debug(LOG_DEBUG_DECISION_TREE, "%s (node: %p norme: %d)",
          __func__, (void *) node, node->norme);

    for (int i = 0; i < __sabo_tree_ctx->num_processes; i++) {
        debug(LOG_DEBUG_DECISION_TREE, "process #%d socket #%d",
              node->processes[i].rank,
              node->processes[i].socket_id);
    }

}
#endif /*# ifndef NDEBUG */

static void tree_build_tree_recursive(struct tree_node *node)
{
    struct tree_node *child_node = tree_alloc_node();

    for (int i = 0; i < __sabo_tree_ctx->num_sockets; i++) {
        int remaining;
        struct tree_node *tmp;

        if (unlikely(NULL == child_node))
               child_node = tree_alloc_node();

        if (0 > node->data[i].num_free_cores)
            continue;

        /* Create child node */
        child_node = tree_dup_node(child_node, node);

        /* Update information with process placement */
        tree_update_node(child_node, i);

        /* Cut tree decision exploration */
        if (child_node->norme != INT_MIN &&
            child_node->norme < node->min) {
            continue; /* skip */
        }

        /* Compute remaining processes */
        remaining = __sabo_tree_ctx->num_processes;
        remaining -= child_node->placed_num_processes;

        /* Skip leaf recusive call */
        if (0 != remaining)
            tree_build_tree_recursive(child_node);

        if (0 != remaining) /* cancel exploration */
            continue;

        /* Valid solution */
        child_node->min = child_node->norme;

        /* Free non candidate node */
        if (child_node->min < node->min)
            continue;

        /* Keep candidate with the fewest thread migration */
        if (node->best &&
            child_node->num_socket_changes >= node->best->num_socket_changes)
            continue;

#ifndef NDEBUG
        tree_dump_placement(child_node);
#endif /*# ifndef NDEBUG */

        debug(LOG_DEBUG_DECISION_TREE, "depth: %d replace %p by %p",
              node->depth, (void *) node->best, (void *) child_node);

        /* Found a new candidate */
        tmp = node->best;

        node->best = child_node;
        node->min = child_node->min;
        node->num_socket_changes = child_node->num_socket_changes;

        child_node = tmp;
    }

    tree_free_node(child_node);

    if (NULL == node->father)
        return; /* Nothing to do */

    /* Free non candidate node */
    if (node->father->min >= node->min) {
        tree_free_node(node->best);
        return;
    }

    debug(LOG_DEBUG_DECISION_TREE, "depth: %d replace %p by %p",
          node->father->depth, (void *) node->father->best,
          (void *) node->best);

    /* Found a new candidate */
    tree_free_node(node->father->best);
    node->father->min = node->min;
    node->father->best = node->best;
}

static void tree_update_core_processes_list(struct tree_node *node)
{
    assert(node->placed_num_processes == __sabo_tree_ctx->num_processes);

    /* Swap prev_socket_id with socket id for core processes */
    for (int i = 0; i < node->placed_num_processes; i++) {
        struct tree_process *tree_process = &(node->processes[i]);
        const int socket_id = tree_process->core_process->socket_id;

        assert(tree_process->socket_id >= 0);
        tree_process->core_process->socket_id = tree_process->socket_id;
        tree_process->core_process->prev_socket_id = socket_id;
    }
}

void decision_tree_compute_placement(struct core_process *processes)
{
    struct tree_node *root;

#ifndef NDEBUG
    const double start = sabo_omp_get_wtime();
#endif /* #ifndef NDEBUG */

    /* Allocate and initialize root node */
    root = tree_alloc_root_node(processes);

    /* Find best candidate */
    tree_build_tree_recursive(root);
    if (unlikely(NULL == root->best)) {
        error("num_socket: %d", __sabo_tree_ctx->num_sockets);
        error("num_cores_per_socket: %d", __sabo_tree_ctx->num_cores_per_socket);
        error("num_processes: %d", __sabo_tree_ctx->num_processes);
        for (int i = 0; i < __sabo_tree_ctx->num_processes; i++) {
            error("process #%d num_threads: %d",
                  processes[i].node_rank, processes[i].num_threads);
        }
        fatal_error("Failed to compute solution");
    }

#ifndef NDEBUG
    tree_dump_placement(root->best);
#endif /* #ifndef NDEBUG */

    /* Store compute processes list */
    tree_update_core_processes_list(root->best);

    /* Free root node */
    tree_free_node(root->best);
    tree_free_node(root);

#ifndef NDEBUG
    debug(LOG_DEBUG_PERF, "Compute %s in %.3f usec(s)",
          __func__, (sabo_omp_get_wtime() - start) * 1000000);
#endif /* #ifndef NDEBUG */
}

void decision_tree_init(const int num_sockets, const int num_cores_per_socket,
            const int num_processes)
{
    if (NULL != __sabo_tree_ctx)
        return;

    __sabo_tree_ctx = xzalloc(sizeof(struct tree_ctx));

    __sabo_tree_ctx->num_cores_per_socket = num_cores_per_socket;
    __sabo_tree_ctx->num_sockets = num_sockets;
    __sabo_tree_ctx->num_processes = num_processes;

    tree_init_nodes_list(num_sockets, num_processes);
}

void decision_tree_fini(void)
{
    tree_fini_nodes_list();

    xfree(__sabo_tree_ctx);
    __sabo_tree_ctx = NULL;
}
