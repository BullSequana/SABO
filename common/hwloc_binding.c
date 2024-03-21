/*
 * Copyright 2021-2024 Bull SAS
 */

#include "hwloc.h"

#include "sys.h"
#include "compiler.h"
#include "binding.h"
#include "log.h"
#include "topo.h"
#include "hwloc_binding.h"

void sabo_hwloc_sys_bind_data_alloc(struct sys_bind_data *data)
{
    data->data = (void *) hwloc_bitmap_alloc();
}

void sabo_hwloc_sys_bind_data_free(struct sys_bind_data *data)
{
    if (NULL != data->data)
        hwloc_bitmap_free((hwloc_bitmap_t) data->data);
}

void sabo_hwloc_set_thread_affinity(struct sys_bind_data *data)
{
    int ret;
    pthread_t myself;
    hwloc_topology_t topo;
    hwloc_obj_t core;
    hwloc_cpuset_t cpuset;

#ifndef NDEBUG
    char hostname[256];
    char string[256];
    char string2[256];
#endif /* #ifndef NDEBUG */

    if (unlikely(NULL == (topo = topo_get_hwloc_topology()))) {
        error("Can't get hwloc topology");
        return;
    }

    if (unlikely(NULL == data->data)) {
        sabo_hwloc_get_thread_affinity(data);
    }

    myself = pthread_self();

    core = hwloc_get_obj_by_type(topo, HWLOC_OBJ_CORE, data->new_core_id);
    if (NULL == core) {
        error("Can't find core #%d in hwloc topology", data->new_core_id);
        return;
    }

#ifndef NDEBUG
    gethostname(hostname, 256);
    hwloc_bitmap_list_snprintf(string, 256, (hwloc_bitmap_t) data->data);
    hwloc_bitmap_list_snprintf(string2, 256, core->cpuset);
    /* Get affinity from hwloc */
    debug(LOG_DEBUG_BINDING, "SABO move host: %s pid %d tid %d OS proc "
          "from %s to %s (cur: %d next: %d)", hostname, getpid(),
          sys_get_tid(), string, string2, data->cur_core_id,
          data->new_core_id);
#endif /* #ifndef NDEBUG */

    cpuset = hwloc_bitmap_dup(core->cpuset);
    hwloc_bitmap_singlify(cpuset); /* remove HT */

    ret = hwloc_set_thread_cpubind(topo, myself, cpuset, 0);

    if (0 != ret) {
        char *str;
        int error = errno;
        hwloc_bitmap_asprintf(&str, cpuset);
        error("Couldn’t bind to cpuset %s: %s\n", str, strerror(error));
        free(str);
    }

    hwloc_bitmap_copy((hwloc_bitmap_t) data->data, cpuset);
    hwloc_bitmap_free(cpuset);

    data->cur_core_id = data->new_core_id;
    data->new_core_id = -1;
}

void sabo_hwloc_get_thread_affinity(struct sys_bind_data *data)
{
    int ret;
    pthread_t myself;
    hwloc_topology_t topo;
    hwloc_cpuset_t cpuset;

    if (NULL == (topo = topo_get_hwloc_topology())) {
        error("Can't get hwloc topology");
        return;
    }
    
    if (unlikely(NULL == data->data)) {
        sabo_hwloc_sys_bind_data_alloc(data);
    }

    myself = pthread_self();
    
    cpuset = hwloc_bitmap_alloc();
    
    ret = hwloc_get_thread_cpubind(topo, myself, cpuset, 0);

    if (0 != ret) {
        int error = errno;
        error("Couldn’t get thread cpubind (%s)", strerror(error)); 
    }

    hwloc_bitmap_copy((hwloc_bitmap_t) data->data, cpuset);
    hwloc_bitmap_free(cpuset);
}
