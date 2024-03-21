/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef __include_sabo_h
#define __include_sabo_h

#ifdef __cplusplus
extern "C" {
#endif
    void __sabo_omp_balanced__(void) __attribute__((weak));
#ifdef __cplusplus
}
#endif

#define sabo_omp_balanced() do { \
    if (__sabo_omp_balanced__) { \
        __sabo_omp_balanced__(); \
    } \
} while (0)

#endif /* #ifndef __include_sabo_h */
