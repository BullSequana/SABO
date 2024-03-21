/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "omp.h"

#include "compiler.h"
#include "log.h"
#include "sys.h"
#include "topo.h"

void xfree(void *ptr)
{
    free(ptr);
}

void *xmalloc(size_t size)
{
    void *ptr;

    ptr = malloc(size);
    if (unlikely(ptr == NULL))
        fatal_sys_error("malloc", "%lu", (long unsigned)size);

    return ptr;
}

void *zalloc(size_t size)
{
    void *ptr;

    ptr = malloc(size);
    if (unlikely(ptr == NULL))
        return NULL;
    memset(ptr, 0, size);

    return ptr;
}

void *xzalloc(size_t size)
{
    void *ptr;

    ptr = zalloc(size);
    if (unlikely(ptr == NULL))
        fatal_sys_error("zalloc", "%lu", (long unsigned)size);

    return ptr;
}

pid_t sys_get_tid(void)
{
#ifdef SYS_gettid
    return (pid_t) syscall(SYS_gettid);
#else /* #ifdef SYS_gettid */
#error "SYS_gettid unavailable on this system"
    return -1;
#endif /* #ifdef SYS_gettid */
}
