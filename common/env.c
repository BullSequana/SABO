/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>

#include "env.h"
#include "log.h"
#include "sys.h"

#define ENV_DEFAULT_OMP_NUM_THREADS -1
#define ENV_DEFAULT_STEPBAL 1
#define ENV_DEFAULT_PERIODIC 0
#define ENV_DEFAULT_NUM_STEPS_EXCHANGED 1
#define ENV_DEFAULT_NO_REBALANCE 0


int env_get_implicit_balancing(void)
{
    const char *env;
    static int env_implicit_balancing = 0;
    if (NULL != (env = getenv("SABO_IMPLICIT_BALANCING")))
        env_implicit_balancing = atoi(env);

    debug(LOG_DEBUG_ENV, "env_implicit_balancing = %s",
          (env_implicit_balancing) ? "true" : "false");

    return env_implicit_balancing;
}

void env_get_log_debug(void)
{
    const char *env;
    log_debug = 0;
    if (NULL != (env = getenv("SABO_LOG_DEBUG")))
        log_debug = strtoul(env, NULL, 16);
}

int env_get_omp_num_threads(void)
{
    const char *env;
    static int env_omp_num_threads = -2; /* uninitialized value */

    if (likely(-2 != env_omp_num_threads)) /* already query */
        return env_omp_num_threads;

    env_omp_num_threads = ENV_DEFAULT_OMP_NUM_THREADS;
    if (NULL != (env = getenv("OMP_NUM_THREADS")))
        env_omp_num_threads = atoi(env);

    debug(LOG_DEBUG_ENV, "Detected %d omp_num_thread(s)",
          env_omp_num_threads);

    return env_omp_num_threads;
}

int env_get_stepbal(void)
{
    const char *env;
    static int env_stepbal = -2; /* uninitialized value */

    if (likely(-2 != env_stepbal)) /* already query */
        return env_stepbal;

    env_stepbal = ENV_DEFAULT_STEPBAL;
    if (NULL != (env = getenv("SABO_STEP_BALANCING")))
        env_stepbal = atoi(env);

    debug(LOG_DEBUG_ENV, "env_stepbal = %d", env_stepbal);

    return env_stepbal;
}

int env_get_periodic(void)
{
    const char *env;
    static int env_periodic = -2; /* uninitialized value */

    if (likely(-2 != env_periodic)) /* already query */
        return env_periodic;

    env_periodic = ENV_DEFAULT_PERIODIC;
    if (NULL != (env = getenv("SABO_PERIODIC")))
        env_periodic = !!atoi(env);

    debug(LOG_DEBUG_ENV, "env_periodic = %s",
          (env_periodic) ? "true" : "false");

    return env_periodic;
}

int env_get_num_steps_exchanged(void)
{
    const char *env;
    static int env_num_steps_exchanged = -2; /* uninitialized value */

    if (likely(-2 != env_num_steps_exchanged)) /* already query */
        return env_num_steps_exchanged;

    env_num_steps_exchanged = ENV_DEFAULT_NUM_STEPS_EXCHANGED;
    if (NULL != (env = getenv("SABO_NUM_STEPS_EXCHANGED")))
        env_num_steps_exchanged = atoi(env);

    debug(LOG_DEBUG_ENV, "env_num_steps_exchanged = %d",
          env_num_steps_exchanged);

    return env_num_steps_exchanged;
}

int env_get_no_rebalance(void)
{
    const char *env;
    static int env_no_rebalance = -2; /* uninitialized value */

    if (likely(-2 != env_no_rebalance)) /* already query */
        return env_no_rebalance;

    env_no_rebalance = ENV_DEFAULT_NO_REBALANCE;
    if (NULL != (env = getenv("SABO_NO_REBALANCE")))
        env_no_rebalance = !!atoi(env);

    debug(LOG_DEBUG_ENV, "env_no_rebalance = %s",
          (env_no_rebalance) ? "true" : "false");

    return env_no_rebalance;
}

int env_get_world_num_tasks(void)
{
    const char *env;
    static int env_world_num_tasks = -2; /* uninitialized value */

    if (likely(-2 != env_world_num_tasks))
        return env_world_num_tasks;

    env_world_num_tasks = -1;
    if (NULL != (env = getenv("SABO_WORLD_TASK_ID")))
        env_world_num_tasks = atoi(env);

    debug(LOG_DEBUG_ENV, "env_world_num_tasks = %d", env_world_num_tasks);

    return env_world_num_tasks;
}

int env_get_world_task_id(void)
{
    const char *env;
    static int env_world_task_id = -2; /* uninitialized value */

    if (likely(-2 != env_world_task_id))
        return env_world_task_id;

    env_world_task_id = -1;
    if (NULL != (env = getenv("SABO_WORLD_TASK_ID")))
        env_world_task_id = atoi(env);

    debug(LOG_DEBUG_ENV, "env_world_task_id = %d", env_world_task_id);

    return env_world_task_id;
}

int env_get_node_num_tasks(void)
{
    const char *env;
    static int env_node_num_tasks = -2; /* uninitialized value */

    if (likely(-2 != env_node_num_tasks))
        return env_node_num_tasks;

    env_node_num_tasks = -1;
    if (NULL != (env = getenv("SABO_NODE_NUM_TASKS")))
        env_node_num_tasks = atoi(env);

    debug(LOG_DEBUG_ENV, "env_node_num_tasks = %d",
          env_node_num_tasks);

    return env_node_num_tasks;
}

int env_get_node_task_id(void)
{
    const char *env;
    static int env_node_task_id = -2; /* uninitialized value */

    if (likely(-2 != env_node_task_id))
        return env_node_task_id;

    env_node_task_id = -1;
    if (NULL != (env = getenv("SABO_NODE_TASK_ID")))
        env_node_task_id = atoi(env);

    debug(LOG_DEBUG_ENV, "env_node_task_id = %d",
          env_node_task_id);

    return env_node_task_id;
}

void env_get_shared_node_filename(char *string, size_t size)
{
    const char *env;

    string[0] = '\0';
    if (NULL != (env = getenv("SABO_SHARED_FILENAME")))
        (void) snprintf(string, size, "%s", env);

    debug(LOG_DEBUG_ENV, "env_shared_node_filename = '%s'",
          string);
}

int env_get_hwloc_xml_file(char *string, size_t size)
{
    const char *env;
    int found = -1;

    string[0] = '\0';

    if (NULL != (env = getenv("SABO_HWLOC_FILENAME")))
        found = 0;

    if (!found)
        (void) snprintf(string, size, "%s", env);

    debug(LOG_DEBUG_ENV, "env_hwloc_filename = '%s'",
          string);

    return found;
}

void env_variables_init(void)
{
    int val;

    env_get_log_debug();

    val = env_get_omp_num_threads();
    if (0 >= val)
        error("sabo needs at least one omp thread (%d)", val);

    val = env_get_stepbal();
    if (0 >= val)
        error("invalid stepbal value (%d)", val);

    (void) env_get_no_rebalance();
    (void) env_get_periodic();
    (void) env_get_num_steps_exchanged();

    (void) env_get_node_task_id();
    (void) env_get_node_num_tasks();

    (void) env_get_world_task_id();
    (void) env_get_world_num_tasks();
}

void env_variables_fini(void)
{
    /* Nothing to do ! */
}
