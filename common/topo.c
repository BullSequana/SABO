/*
 * Copyright 2021-2024 Bull SAS
 */

#include <limits.h>
#include <stdio.h>
#include <hwloc.h>

#include "sys.h"
#include "log.h"
#include "topo.h"
#include "env.h"

static hwloc_topology_t sabo_topology = NULL;

static int **sabo_topo_core_id_by_socket = NULL;

hwloc_topology_t topo_get_hwloc_topology(void)
{
    return sabo_topology;
}

int topo_get_socket_id_from_core_id(const int core_id)
{
    hwloc_obj_t core;
    hwloc_obj_t socket;

    if (unlikely(NULL == sabo_topology))
        fatal_error("No hwloc topology init");

    core = hwloc_get_obj_by_type(sabo_topology, HWLOC_OBJ_CORE,
                     (unsigned int) core_id);
    assert(NULL != core);

    socket = hwloc_get_next_obj_covering_cpuset_by_type(sabo_topology,
                                core->cpuset,
                                HWLOC_OBJ_PACKAGE,
                                NULL);
    assert(NULL != socket);

    return (int) socket->logical_index;
}

int topo_get_num_sockets(void)
{
    static int num_sockets = -2; /* uninitialized value */

    if (unlikely(NULL == sabo_topology))
        fatal_error("No hwloc topology init");

    if (-2 != num_sockets) /* already query */
        return num_sockets;

    num_sockets = hwloc_get_nbobjs_by_type(sabo_topology, HWLOC_OBJ_PACKAGE);
    debug(LOG_DEBUG_TOPO, "Detected %d socket(s)", num_sockets);

    return num_sockets;
}

int topo_get_socket_core_id(int socket_id, int local_core_id)
{
    int num_sockets;
    hwloc_cpuset_t cpuset;

    assert(socket_id >= 0);
    assert(socket_id < topo_get_num_sockets());

    assert(local_core_id >= 0);

    if (likely(NULL != sabo_topo_core_id_by_socket)) {
        assert(local_core_id < topo_get_num_cores_per_socket());
        return sabo_topo_core_id_by_socket[socket_id][local_core_id];
    }

    num_sockets = topo_get_num_sockets();

    cpuset = hwloc_bitmap_alloc();

    sabo_topo_core_id_by_socket = xzalloc(sizeof(int *) * (size_t) num_sockets);

    for (int i = 0; i < num_sockets; i++) {
        hwloc_obj_t socket;

        const int count = topo_get_num_cores_per_socket();

        socket = hwloc_get_obj_by_type(sabo_topology, HWLOC_OBJ_PACKAGE,
                           (unsigned int) i);
        assert(NULL != socket);

        sabo_topo_core_id_by_socket[i] = xzalloc(sizeof(int) * (size_t) count);

        for (int j = 0; j < count; j++) {
            int core_id;
            hwloc_obj_t core;

            core = hwloc_get_obj_inside_cpuset_by_type(sabo_topology,
                                   socket->cpuset,
                                   HWLOC_OBJ_CORE,
                                   (unsigned int) j);
            assert(NULL != core);

            /* Get a copy of its cpuset that we may modify. */
            hwloc_bitmap_copy(cpuset, core->cpuset);

            /* Get only one logical processor (in case the core is
             * SMT/hyper-threaded). */
            hwloc_bitmap_singlify(cpuset);

            /* Extract core id from cpuset */
            core_id = hwloc_bitmap_first(cpuset);

            sabo_topo_core_id_by_socket[i][j] = core_id;
        }

    }

    hwloc_bitmap_free(cpuset);
    return sabo_topo_core_id_by_socket[socket_id][local_core_id];
}

int topo_get_num_cores(void)
{
    static int num_cores = -2; /* uninitialized value */

    if (unlikely(NULL == sabo_topology))
        fatal_error("No hwloc topology init");

    if (-2 != num_cores) /* already query */
        return num_cores;

    num_cores = hwloc_get_nbobjs_by_type(sabo_topology, HWLOC_OBJ_CORE);
    debug(LOG_DEBUG_TOPO, "Detected %d core(s)", num_cores);

    return num_cores;
}

int topo_get_num_cores_per_socket(void)
{
    static int num_cores_per_socket = -2; /* uninitialized value */
    const int num_sockets = topo_get_num_sockets();

    if (-2 != num_cores_per_socket) /* already query */
        return num_cores_per_socket;

    for (int i = 0; i < num_sockets; i++) {
        int num_cores;
        hwloc_obj_t socket;

        /* Get socket node #i */
        socket = hwloc_get_obj_by_type(sabo_topology, HWLOC_OBJ_PACKAGE,
                         (unsigned int) i);
        assert(NULL != socket);

        num_cores = hwloc_get_nbobjs_inside_cpuset_by_type(sabo_topology,
                                   socket->cpuset,
                                   HWLOC_OBJ_CORE);
        debug(LOG_DEBUG_TOPO, "Detected %d num core(s) on socket #%d",
              num_cores, i);

        if (-2 == num_cores_per_socket) {
            num_cores_per_socket = num_cores;
            continue;
        }

        if (num_cores_per_socket != num_cores) {
            error("Different num cores per socket detected");
            num_cores_per_socket = -1;
            break;
        }
    }

    debug(LOG_DEBUG_TOPO, "Detected %d num core(s) per socket",
          num_cores_per_socket);

    return num_cores_per_socket;
}

int topo_get_thread_cpubind(void)
{
    int rc;
    int core_id;
    int num_cores;
    hwloc_cpuset_t cpuset;

    cpuset = hwloc_bitmap_alloc();
    rc = hwloc_get_cpubind(sabo_topology, cpuset,
                   HWLOC_CPUBIND_THREAD);
    if (unlikely(0 != rc))
        fatal_error("Unexpected error on hwloc_get_cpubind");

    num_cores = hwloc_get_nbobjs_inside_cpuset_by_type(sabo_topology,
                               cpuset,
                               HWLOC_OBJ_CORE);
    /* No real binding */
    if (num_cores > 1)
        return -1;

    hwloc_bitmap_singlify(cpuset);
    core_id = hwloc_bitmap_first(cpuset);

    hwloc_bitmap_free(cpuset);

    return core_id;
}

void topo_init(void)
{
    int num_cores;
    int num_cores_per_socket;
    int num_sockets;
    char filename[PATH_MAX];

    hwloc_topology_init(&sabo_topology);

    if (!env_get_hwloc_xml_file(filename, PATH_MAX))
        hwloc_topology_set_xml(sabo_topology, filename);

    hwloc_topology_load(sabo_topology);

    num_cores = topo_get_num_cores();
    if (0 > num_cores)
        fatal_error("Unexpected num cores (%d)", num_cores);

    num_sockets = topo_get_num_sockets();
    if (1 > num_sockets)
        error("sabo need at least one socket (%d)", num_sockets);

    num_cores_per_socket = topo_get_num_cores_per_socket();
    if (2 > num_cores_per_socket)
        error("sabo need at least two cores per socket (%d)",
              num_cores_per_socket);

    /* initialiaze array in sequential part */
    topo_get_socket_core_id(0,0);
}

void topo_fini(void)
{
    if (unlikely(NULL == sabo_topology))
        fatal_error("No hwloc topology init");

    hwloc_topology_destroy(sabo_topology);
    sabo_topology = NULL;
}
