/*
 * Copyright 2021-2024 Bull SAS
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "sys.h"

static int read_integer(const char *string, char **end_string,
            const int allow_zero)
{
    long value;

    value = strtol(string, end_string, 10);

    if (unlikely(0 == value && !allow_zero))
        goto ERROR;

    if (unlikely(0 > value || *end_string == string)) {
        goto ERROR;

    }

    return (int) value;

ERROR:
    error("invalid integer value: %s", string);
    return -1;
}

static int read_slurm_ppn_repeat_count(const char *full_str_ptr,
                       const char *cur_str_ptr,
                       char **end_str_ptr)
{
    int count;

    if (unlikely('(' != *cur_str_ptr))
        return 0;

    (*end_str_ptr)++;    /* skip '(' character */
    cur_str_ptr = *end_str_ptr;

    if (unlikely('x' != *cur_str_ptr)) {
        error("invalid repeat specification at offset %d: %s",
              (int) (cur_str_ptr - full_str_ptr), full_str_ptr);
        return -1;
    }

    (*end_str_ptr)++;    /* skip 'x' character */
    cur_str_ptr = *end_str_ptr;

    count = read_integer(cur_str_ptr, end_str_ptr, 0);

    if (unlikely(0 == count)) {
        error("invalid repeat count at offset %d: %s",
              (int) (cur_str_ptr - full_str_ptr), full_str_ptr);
        return -1;
    }

    cur_str_ptr = *end_str_ptr;

    if (unlikely(')' != *cur_str_ptr)) {
        error("unexpected character at offset %d: %s",
              (int) (cur_str_ptr - full_str_ptr), full_str_ptr);
        return -1;
    }

    (*end_str_ptr)++;    /* skip ')' character */

    return count;
}

static int read_slurm_tasks_ppn_parse(const char *full_str_ptr,
                      const int task_per_node_size,
                      int *task_per_node)
{
    int index = 0;
    const char *cur_str_ptr = full_str_ptr;

    while ('\0' != *cur_str_ptr) {
        int count;
        int repeat;
        char *end_str_ptr = NULL;

        count = read_integer(cur_str_ptr, &end_str_ptr, 0);

        if (unlikely(0 >= count)) {
            error("invalid integer value at offset %d: %s",
                  (int) (cur_str_ptr - full_str_ptr), full_str_ptr);
            return -1;
        }

        cur_str_ptr = end_str_ptr;

        repeat = read_slurm_ppn_repeat_count(full_str_ptr, cur_str_ptr,
                            &end_str_ptr);
        if (unlikely(0 > repeat))
            return -1;

        repeat = (!repeat) ? 1 : repeat;

        for (int i = 0; i < repeat; i++) {
            assert(index < task_per_node_size);
            task_per_node[index++] = count;
        }

        cur_str_ptr = end_str_ptr;

        if ('\0' == *cur_str_ptr)
            break;

        if (unlikely(',' != *cur_str_ptr)) {
            error("unexpected character at offset %d: %s",
                  (int) (cur_str_ptr - full_str_ptr), full_str_ptr);
            return -1;
        }

        cur_str_ptr++; /* skip ',' character */
    }

    if (unlikely(index != task_per_node_size)) {
        error("unexpected error");
        return -1;
    }

    return 0;
}

static int read_slurm_integer_env_variable(const char *name, const int allow_zero)
{
    char *env;
    char *end;
    int value;

    if (unlikely(NULL == (env = getenv(name)))) {
        error("Can't read '%s' env variable", name);
        return -1;
    }

    debug(LOG_DEBUG_TOOLS, "read '%s' : %s", name, env);

    value = read_integer(env, &end, allow_zero);

    if (unlikely(0 > value || (0 == value && !allow_zero))) {
        error("invalid '%s' integer %s", name, env);
        return -1;
    }

    return value;
}

static int read_slurm_job_nodes_env_variable(void)
{
    const char *name = "SLURM_JOB_NUM_NODES";
    return read_slurm_integer_env_variable(name, 0);
}

static int read_slurm_job_nodeid_env_variable(void)
{
    const char *name = "SLURM_NODEID";
    return read_slurm_integer_env_variable(name, 1);
}

static int read_slurm_node_localid_env_variable(void)
{
    const char *name = "SLURM_LOCALID";
    return read_slurm_integer_env_variable(name, 1);
}

static int read_slurm_get_node_tasks_per_node_env_variable(void)
{
    char *env;
    int rc;
    int num_nodes;
    int node_id;
    int local_id;
    int *array;
    const char *name = "SLURM_TASKS_PER_NODE";

    local_id = read_slurm_node_localid_env_variable();
    if (unlikely(0 > local_id))
           return -1;

    num_nodes = read_slurm_job_nodes_env_variable();
    if (unlikely(0 >= num_nodes))
        return -1;

    node_id = read_slurm_job_nodeid_env_variable();
    if (unlikely(0 > num_nodes))
         return -1;

    assert(node_id < num_nodes);

    if (unlikely(NULL == (env = getenv(name)))) {
        error("Can't read '%s' env variable", name);
        return -1;
    }

    debug(LOG_DEBUG_TOOLS, "read '%s' : %s", name, env);

    array = xzalloc(sizeof(int) * (size_t) num_nodes);

    rc = read_slurm_tasks_ppn_parse(env, num_nodes, array);
    if (likely(0 > rc))
        goto ERROR;

    rc = array[node_id];
    debug(LOG_DEBUG_TOOLS, "found %d on node #%d", rc, node_id);

ERROR:
    xfree(array);

    return rc;
}

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
#ifndef NDEBUG
    log_debug |= LOG_DEBUG_TOOLS;
#endif /* NDEBUG */

    return read_slurm_get_node_tasks_per_node_env_variable();
}
