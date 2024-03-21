/*
 * Copyright 2021-2024 Bull SAS
 */

#ifndef include_comm_h
#define include_comm_h

struct comm_module_funcs {
    int (*is_initialized)(void);

    int (*init)(int *argc, char ***argv);
    int (*init_thread)(int *argc, char ***argv, int req, int *prov);
    int (*fini)(void);

    int (*get_world_rank)(void);
    int (*get_world_size)(void);
    int (*get_world_rank_from_node_rank)(const int rank);

    int (*get_node_rank)(void);
    int (*get_node_size)(void);

    void (*alloc_node_comm)(void);
    void (*free_node_comm)(void);

    void (*allgather)(const void *sbuf, void *rbuf, const int count);
};

void comm_init(void);
void comm_fini(void);
void comm_load_module_mpi(void);
void comm_unload_module_mpi(void);

int comm_is_initialized(void);

/* World communicator */
int comm_get_world_rank(void);
int comm_get_world_size(void);
int comm_get_world_rank_from_node_rank(const int rank);

/* Node communicator */
int comm_get_node_rank(void);
int comm_get_node_size(void);

/* MPI Communication */
void comm_allgather(const double *sbuf, double *rbuf, const int count);

double *comm_get_send_buffer(const int window);
double *comm_get_recv_buffer(const int window);

#endif /* #ifndef include_comm_h */
