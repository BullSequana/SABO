/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_compiler_h
#define include_compiler_h

#include <features.h>

#if defined(__GNUC__)
/* if GCC is defined but do not support inline keyword, define inline */
/* to the appropriate intrinsic */
#if !defined(__GNUC_STDC_INLINE__)
#define inline __inline
#endif /* #if !defined(__GNUC_STDC_INLINE__) */

#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))

#else /* #if defined(__GNUC__) */
/* for all other compiler, just ensure that the build is possible */
#define inline
#define likely(x) (x)
#define unlikely(x) (x)
#endif /* #if defined(__GNUC__) */

#endif /* #ifndef include_compiler_h */
