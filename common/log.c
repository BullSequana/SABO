/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include "log.h"
#include <pthread.h>
#include <omp.h>

struct level_param {
    unsigned fatal : 1;
    unsigned fileline : 1;
    unsigned pad31 : 30;
    unsigned pad;

    FILE *fd;
    const char *header;
};

static pthread_spinlock_t log_lock;

__attribute__((constructor)) static void lock_constructor(void)
{
    if (0 != pthread_spin_init(&log_lock, 0))
        fatal_error("Failed pthread_spin_init");
}

__attribute__((destructor)) static void lock_destructor(void)
{
    if (0 != pthread_spin_destroy(&log_lock))
        fatal_error("Failed pthread_spin_destroy");
}

uint64_t log_debug;

static void level2param(int level, struct level_param *param)
{
    switch (level) {
    case LOG_LEVEL_BASE:
        param->fatal = 0;
        param->fileline = 0;
        param->fd = stdout;
        param->header = NULL;
        break;

    case LOG_LEVEL_DEBUG:
        param->fatal = 0;
        param->fileline = 1;
        param->fd = stdout;
        param->header = "debug";
        break;

    case LOG_LEVEL_ERROR:
        param->fatal = 0;
        param->fileline = 0;
        param->fd = stderr;
        param->header = "error";
        break;

    case LOG_LEVEL_ERROR | LOG_LEVEL_FATAL:
        param->fatal = 1;
        param->fileline = 1;
        param->fd = stderr;
        param->header = "fatal error";
        break;

    case LOG_LEVEL_INT_ERROR | LOG_LEVEL_FATAL:
        param->fatal = 1;
        param->fileline = 1;
        param->fd = stderr;
        param->header = "fatal internal error";
        break;

    case LOG_LEVEL_SYS_ERROR:
        param->fatal = 0;
        param->fileline = 0;
        param->fd = stderr;
        param->header = "sys error";
        break;

    case LOG_LEVEL_SYS_ERROR | LOG_LEVEL_FATAL:
        param->fatal = 1;
        param->fileline = 1;
        param->fd = stderr;
        param->header = "fatal sys error";
        break;

    default:
        fatal_error("unexpected message level 0x%4d", level);
    }
}

static void do_print(struct level_param *param, const char *file, int line,
             const char *fmt, va_list lst)
{
    ssize_t ssize;
    int rc;

    pthread_spin_lock(&log_lock);

#ifndef NDEBUG
    ssize = fprintf(param->fd, "%f %d/%d ", omp_get_wtime(), getpid(), (pid_t) syscall(SYS_gettid));
    if (ssize < 0)
        abort();
#endif /*# ifndef NDEBUG */

    if (param->header) {
        ssize = fprintf(param->fd, "%s:", param->header);
        if (ssize < 0)
            abort();
    }

#ifndef NDEBUG
    if (param->fileline) {
        ssize = fprintf(param->fd, "%s:%d", file, line);
        if (ssize < 0)
            abort();
    }
#endif /*# ifndef NDEBUG */

    if (param->header || param->fileline) {
        rc = fputc(' ', param->fd);
        if (rc < 0)
            abort();
    }

    if (fmt != NULL) {
        ssize = vfprintf(param->fd, fmt, lst);
        if (ssize < 0)
            abort();
    }

    rc = fputc('\n', param->fd);
    if (rc < 0)
        abort();

    fflush(param->fd);

    pthread_spin_unlock(&log_lock);
}

void do_log(int level, const char *file, int line, const char *fmt, ...)
{
    static FILE *fd_prev = NULL;
    struct level_param param;
    va_list lst;

    level2param(level, &param);

    if (fd_prev != param.fd && fd_prev != NULL)
        fflush(fd_prev);

    va_start(lst, fmt);
    do_print(&param, file, line, fmt, lst);
    va_end(lst);

    fd_prev = param.fd;

    if (param.fatal)
        abort();
}
