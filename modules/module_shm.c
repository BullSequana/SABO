/*
 * Copyright 2021-2024 Bull SAS
 */

#include <limits.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <assert.h>

#include "comm.h"
#include "arch.h"
#include "env.h"
#include "log.h"
#include "module_shm.h"
#include "sys.h"

#define SABO_MAX_PROCESSES_ON_NODE 256

struct shm_shared_context {
    char syncfile[PATH_MAX];    /**< SHM file with sync data    */
    char shmfile[PATH_MAX];        /**< SHM file with shared data    */

    size_t size;
    int value;
    int pad0;
};

struct shm_shared_data {
    int world_ranks[SABO_MAX_PROCESSES_ON_NODE];
    uint32_t gen_id;
    uint32_t nwritter;
};    

struct module_shm_ctx {
    struct shm_shared_data *shared_data;
    double *rbuf;
    uint32_t gen_id;
    uint32_t pad0;
};

static int sabo_shm_module_initialized = 0;
static int *sabo_shm_module_translate_node_rank = NULL;
static struct module_shm_ctx *sabo_shm_module_ctx = NULL;

static int shm_get_node_size(void);
static int shm_get_node_rank(void);
static int shm_get_world_rank(void);

static void module_shm_sync_step_1(void)
{
    for (;;) { /* Naive barrier */
        uint32_t gen_id, *gen_id_ptr;
            gen_id_ptr = &(sabo_shm_module_ctx->shared_data->gen_id);
        gen_id = __atomic_load_n(gen_id_ptr, __ATOMIC_RELAXED);
        if (sabo_shm_module_ctx->gen_id == gen_id)
            break;

        cpu_relax();
    }
}

static void module_shm_sync_step_2(void)
{
    uint32_t id;
    uint32_t *nwritter;
    
    const int node_num_processes = shm_get_node_size();

    nwritter = &(sabo_shm_module_ctx->shared_data->nwritter);

    /* Catch last processor */
    id = __atomic_fetch_add(nwritter, 1, __ATOMIC_RELAXED);
    if (id == node_num_processes - 1) {
        uint32_t *gen_id;
            gen_id = (&sabo_shm_module_ctx->shared_data->gen_id);
        __atomic_store_n(nwritter, 0, __ATOMIC_RELAXED);
        __atomic_fetch_add(gen_id, 1, __ATOMIC_RELAXED);
    }
    
    sabo_shm_module_ctx->gen_id++;
}

static size_t shm_get_mmap_size(void)
{
    size_t size;
    const int node_num_processes = shm_get_node_size();
    const int window = env_get_num_steps_exchanged();

    size = sizeof(struct shm_shared_data);
    size += sizeof(double) * window * node_num_processes;    

    /* Round-up expected size */
    return (size / page_size() + ((size % page_size()) ? 1 : 0)); 
}

static void shm_generate_shmfile(char *shmfile)
{
    const char *name = "syncfile";
    snprintf(shmfile, PATH_MAX - 1, "%s", name);
}

static int shm_sync_open_syncfile(const char *filename, int o_creat)
{
    mode_t mode;
    int fd, flags;

    /* open the shared file */
    mode = S_IRUSR | S_IWUSR;
    flags = O_RDWR | O_SYNC | ((o_creat) ? O_CREAT : 0);

    if ((fd = open(filename, flags, mode)) < 0) {
        /* Avoid Spamm during client init */
        if (errno == ENOENT && o_creat == 0)
            return -1;

        sys_error("open", "\"%s\", %s, 0%o", filename,
              (o_creat) ? "O_RDWR | O_CREAT" : "O_RDWR",
              mode);
        return -1;
    }

    return fd;
}

static int shm_sync_remote_client(struct shm_shared_context *ctx)
{
    struct shm_shared_context remote_ctx;

    /* reset my context */
    memset(&remote_ctx, 0, sizeof(struct shm_shared_context));

    while(1) {
        ssize_t nread;
        int success, fd;

        if((fd = shm_sync_open_syncfile(ctx->syncfile, 0)) < 0)
            continue;

        if (flock(fd, LOCK_EX) < 0)
            fatal_sys_error("flock", "%d, LOCK_EX", fd);

        if ((nread = read(fd, &remote_ctx, sizeof(remote_ctx))) < 0) {
            fatal_sys_error("read", "%d, %p, %lu", fd,
                     (void *) &remote_ctx, sizeof(remote_ctx));
        }

        success = (nread == sizeof(remote_ctx)) ? 1 : 0;

        if (flock(fd, LOCK_UN) < 0)
            fatal_sys_error("flock", "%d, LOCK_UN", fd);

        close(fd);

        if (success)
            break;

        usleep(1000);
    }

    /* Use data in remote context */
    *ctx = remote_ctx;

    return 0;
}

static int shm_sync_remote_master(struct shm_shared_context *ctx)
{
    int fd;
    ssize_t nwrite;

    if((fd = shm_sync_open_syncfile(ctx->syncfile, 1)) < 0)
        return -1;

    if (flock(fd, LOCK_EX) < 0)
        fatal_sys_error("flock", "%d, LOCK_EX", fd);

    if (ftruncate(fd, (off_t) page_size()) < 0) {
        sys_error("ftruncate", "%d, %lu", fd, page_size());
        if (flock(fd, LOCK_UN) < 0)
            fatal_sys_error("flock", "%d, LOCK_UN", fd);
        close(fd);
        return -1;
    }

    if ((nwrite = write(fd, ctx, sizeof(*ctx))) < 0) {
        fatal_sys_error("write", "%d, %p, %lu", fd,
                 (void *) ctx, sizeof(*ctx));
    }

    if (nwrite != sizeof(*ctx)) {
        fatal_error("Truncated write");
    }

    if (flock(fd, LOCK_UN) < 0)
        fatal_sys_error("flock", "%d, LOCK_UN", fd);

    close(fd);

    return 0;
}

static int shm_get_world_rank(void)
{
    return env_get_world_task_id();
}

static int shm_get_world_size(void)
{
    return env_get_world_num_tasks();
}

static int shm_get_node_rank(void)
{
    return env_get_node_task_id();
}

static int shm_get_node_size(void)
{
    return env_get_node_num_tasks();
}

static int shm_get_world_rank_from_node_rank(const int rank)
{
    if (unlikely(!sabo_shm_module_translate_node_rank))
        return -1; /* Not available */

    return sabo_shm_module_translate_node_rank[rank];
}

static void shm_alloc_node_comm(void)
{
    return;    /* Nothing to do */
}

static void shm_free_node_comm(void)
{
    return;    /* Nothing to do */
}

static int shm_init_node_master(void)
{
    void *ptr;
    int fd, rc;
    struct shm_shared_context ctx;

    env_get_shared_node_filename(ctx.syncfile,
                     sizeof(ctx.syncfile));

    /* generate shm filename */
    shm_generate_shmfile(ctx.shmfile);

    /* get mmap size */
    ctx.size = shm_get_mmap_size(); 

    /* open shmfile */
    fd = shm_open(ctx.shmfile, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
    if (unlikely(0 > fd))
        return -1;

    /* truncate shmfile */
    ftruncate(fd, ctx.size);

    /* mmap shmfile */
    ptr = mmap(NULL , ctx.size, PROT_READ | PROT_WRITE , MAP_SHARED , fd, 0);
    if (unlikely(MAP_FAILED == ptr))
        return -1;
    memset(ptr, 0, ctx.size);
    
    sabo_shm_module_ctx = xzalloc(sizeof(struct module_shm_ctx));
    sabo_shm_module_ctx->shared_data = (struct shm_shared_data *) ptr; 

    sabo_shm_module_ctx->rbuf = (double *)((char *)ptr + sizeof(struct module_shm_ctx));

    /* put master informations */
    rc = shm_sync_remote_master(&ctx);

    return rc;
}

static int shm_init_node_client(void)
{
    void *ptr;
    int fd, rc;
    struct shm_shared_context ctx;

    const int wrank = shm_get_world_rank();
    const int nrank = shm_get_node_rank();    
    
    env_get_shared_node_filename(ctx.syncfile,
                     sizeof(ctx.syncfile));

    /* put master informations */
    rc = shm_sync_remote_client(&ctx);

    /* open shmfile */
    fd = shm_open(ctx.shmfile, O_RDWR, S_IRUSR | S_IWUSR);

    /* mmap shmfile */
    ptr = mmap(NULL , ctx.size, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0);
    if (unlikely(MAP_FAILED == ptr))
        return -1;

    sabo_shm_module_ctx = xzalloc(sizeof(struct module_shm_ctx));
    sabo_shm_module_ctx->shared_data = (struct shm_shared_data *) ptr; 
    
    sabo_shm_module_ctx->rbuf = (double *)((char *)ptr + sizeof(struct module_shm_ctx));
    sabo_shm_module_ctx->shared_data->world_ranks[nrank] = wrank;

    return rc;
}

static int sabo_module_shm_init(int *argc, char ***argv)
{
    int rc, node_size;

    UNUSED(argc);
    UNUSED(argv);

    if (0 == env_get_node_task_id()) {
        rc = shm_init_node_master();
    } else {
        rc = shm_init_node_client();
    }

    if (unlikely(0 != rc))
        fatal_error("Can't sync process on node");

    /* sync process */
    module_shm_sync_step_2();
    module_shm_sync_step_1();

    node_size = shm_get_node_size();

    /* Init translation from node rank to world rank */
    sabo_shm_module_translate_node_rank = xzalloc(sizeof(int) * node_size); 

    for (int i = 0; i < node_size; i++) {
        int wrank = sabo_shm_module_ctx->shared_data->world_ranks[i];
        sabo_shm_module_translate_node_rank[i] = wrank;
    }

    return 0;
}

static int sabo_module_shm_fini(void)
{
    fatal_error("not implemented");
}

static void shm_allgather(const void *sbuf, void *rbuf, const int count)
{
    size_t offset;
    struct shm_shared_data *shared_data;

    const int node_rank = shm_get_node_rank();
    const int node_num_processes = shm_get_node_size();

    shared_data = sabo_shm_module_ctx->shared_data;
    assert(NULL != shared_data);

    offset = count * node_rank * sizeof(double);
    
    /* Wait right generation id */
    module_shm_sync_step_1();

    /* Add my contribution */
    memcpy(sabo_shm_module_ctx->rbuf + offset, sbuf,
           count * sizeof(double));

    /* Wait all contributions */
    module_shm_sync_step_2(); 
    module_shm_sync_step_1();

    memcpy(rbuf, sabo_shm_module_ctx->rbuf,
           node_num_processes * count * sizeof(double));

    module_shm_sync_step_2();
}

static int shm_is_initialized(void)
{
    return __atomic_load_n(&sabo_shm_module_initialized,
                   __ATOMIC_RELAXED);
}

void sabo_module_shm_register_cb(struct comm_module_funcs *funcs)
{
    funcs->is_initialized = shm_is_initialized;

    funcs->init = sabo_module_shm_init;
    funcs->fini = sabo_module_shm_fini;

    funcs->get_world_rank = shm_get_world_rank;
    funcs->get_world_size = shm_get_world_size;
    funcs->get_world_rank_from_node_rank = shm_get_world_rank_from_node_rank;

    funcs->get_node_rank = shm_get_node_rank;
    funcs->get_node_size = shm_get_node_size;

    funcs->alloc_node_comm = shm_alloc_node_comm;
    funcs->free_node_comm = shm_free_node_comm;

    funcs->allgather = shm_allgather;
}
