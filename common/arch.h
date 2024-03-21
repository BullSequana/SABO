/*
 * Copyright 2021-2024 Bull SAS
 */


#ifndef include_arch_h
#define include_arch_h

#include <stdint.h>
#include "compiler.h"

#if !defined(__x86_64)
#error Unsupported architecture
#endif /* #if !defined(__x86_64) */

static __always_inline void cpu_relax(void)
{
    __asm __volatile("pause\n\t": : :"memory");
}

#endif /* #ifndef include_arch_h */
