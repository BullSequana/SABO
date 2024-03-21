/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_sabo_ompt_h
#define include_sabo_ompt_h

struct ompt_threads_data {
    double start; /* master parallel begin */
    double *elapsed; /* omp paralel elapsed time */
    int num_calls;
    int pad0;
};
typedef struct ompt_threads_data ompt_threads_data_t;

#endif /* #ifndef include_sabo_ompt_h */
