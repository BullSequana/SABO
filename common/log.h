/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_log_h
#define include_log_h

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "comm.h"

#define LOG_LEVEL_BASE            1
#define LOG_LEVEL_DEBUG            2
#define LOG_LEVEL_ERROR            3
#define LOG_LEVEL_INT_ERROR        4
#define LOG_LEVEL_SYS_ERROR        5
#define LOG_LEVEL_FATAL            (1 << 15)

/* for performance reasons, need to be seen from everywhere to avoid */
/* entering do_log and return immediately */
extern uint64_t log_debug;

#define LOG_DEBUG_NONE            0
#define LOG_DEBUG_ANY            UINT64_MAX
#define LOG_DEBUG_CORE            (UINT64_C(1) << 0)
#define LOG_DEBUG_ENV            (UINT64_C(1) << 1)
#define LOG_DEBUG_TOPO            (UINT64_C(1) << 2)
#define LOG_DEBUG_DECISION_TREE        (UINT64_C(1) << 3)
#define LOG_DEBUG_OMPT            (UINT64_C(1) << 4)
#define LOG_DEBUG_MPI            (UINT64_C(1) << 5)
#define LOG_DEBUG_BINDING        (UINT64_C(1) << 6)
#define LOG_DEBUG_PERF            (UINT64_C(1) << 7)
#define LOG_DEBUG_TOOLS            (UINT64_C(1) << 8)

#define ERROR_CONCAT(call, rc, fmt, ...)                \
    "%s(" fmt ") returned error %d: %s",                \
        call, ##__VA_ARGS__, rc, strerror(rc)

extern __attribute__((format(printf,4,5)))
void do_log(int level, const char *file, int line, const char *fmt, ...);

#define print(...) do {                            \
        do_log(LOG_LEVEL_BASE, __FILE__, __LINE__,         \
               __VA_ARGS__);                    \
    } while(0)

#define debug(type, ...) do {                        \
        if (log_debug && (log_debug & type) == type) {        \
            do_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__,    \
                   __VA_ARGS__);                \
        }                            \
    } while(0)

#define mdebug(type, ...) do {                        \
        if (log_debug && (log_debug & type) == type &&        \
           !comm_get_world_rank()) {            \
            do_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__,    \
                   __VA_ARGS__);                \
        }                            \
    } while(0)

#define ndebug(type, ...) do {                        \
        if (log_debug && (log_debug & type) == type &&        \
           !comm_get_node_rank()) {                \
            do_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__,    \
                   __VA_ARGS__);                \
        }                            \
    } while(0)

#define error(...) do {                            \
        do_log(LOG_LEVEL_ERROR, __FILE__, __LINE__,         \
               __VA_ARGS__);                    \
    } while(0)

#define fatal_error(...) do {                        \
        do_log(LOG_LEVEL_ERROR | LOG_LEVEL_FATAL, __FILE__,    \
               __LINE__, __VA_ARGS__);                \
        abort();                        \
    } while(0)

#define fatal_int_error(...) do {                    \
        do_log(LOG_LEVEL_INT_ERROR | LOG_LEVEL_FATAL, __FILE__,    \
               __LINE__, __VA_ARGS__);                \
        abort();                        \
    } while(0)

#define sys_error(call, ...) do {                    \
        do_log(LOG_LEVEL_SYS_ERROR, __FILE__, __LINE__,        \
               ERROR_CONCAT(call, errno, __VA_ARGS__));        \
    } while(0)

#define fatal_sys_error(call, ...) do {                    \
        do_log(LOG_LEVEL_SYS_ERROR | LOG_LEVEL_FATAL, __FILE__,    \
               __LINE__, ERROR_CONCAT(call, errno,        \
                          __VA_ARGS__));        \
        abort();                        \
    } while(0)

#define UNEXPECTED() fatal_int_error("unexpected situation");

#endif /* #ifndef include_log_h */
