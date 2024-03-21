/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_sys_h
#define include_sys_h

#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

extern void xfree(void *ptr);
extern void *zalloc(size_t size);

extern void *xmalloc(size_t size);
extern void *xzalloc(size_t size);

extern pid_t sys_get_tid(void);

static inline size_t page_size(void)
{
    return EXEC_PAGESIZE;
}

#endif /* #ifndef include_sys_h */
