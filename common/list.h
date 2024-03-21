/*
 * Copyright 2024 Bull SAS
 */

#ifndef include_list_h
#define include_list_h

#include <stddef.h>
#include <stdlib.h>
#include "compiler.h"

struct list_head {
    struct list_node *first;
    struct list_node *last;
};

struct listm_head {
    struct list_head list_head;
    size_t offset;
};

struct list_node {
    struct list_node *previous;
    struct list_node *next;
};

#define LIST_OFFSET(type, field) offsetof(type, field)

extern void list_head_init(struct list_head *list_head);
extern void list_head_finalize(const struct list_head *list_head);
extern void list_add_first(struct list_head *list_head, const void *new_node,
               size_t offset);
extern void list_add_last(struct list_head *list_head, const void *new_node,
              size_t offset);
extern void list_add_before(struct list_head *list_head, const void *node,
                const void *new_node, size_t offset);
extern void list_add_after(struct list_head *list_head, const void *node,
               const void *new_node, size_t offset);

static int list_is_empty(const struct list_head *list_head)
{
    return (list_head->first == NULL);
}

extern void *list_get_first(const struct list_head *list_head, size_t offset);
extern void *list_get_last(const struct list_head *list_head, size_t offset);
extern void *list_get_next(const void *node, size_t offset);
extern void *list_get_previous(const void *node, size_t offset);
extern void list_remove(struct list_head *list_head, const void *node,
            size_t offset);

extern void listm_head_init(struct listm_head *listm_head, size_t offset);
extern void listm_head_finalize(struct listm_head *listm_head);

static inline void listm_add_first(struct listm_head *listm_head,
                   const void *new_node)
{
    list_add_first(&listm_head->list_head, new_node, listm_head->offset);
}

static inline void listm_add_last(struct listm_head *listm_head,
                  const void *new_node)
{
    list_add_last(&listm_head->list_head, new_node, listm_head->offset);
}

static inline void listm_add_before(struct listm_head *listm_head, const void *node,
                    const void *new_node)
{
    list_add_before(&listm_head->list_head, node, new_node,
            listm_head->offset);
}

static inline void listm_add_after(struct listm_head *listm_head, const void *node,
                   const void *new_node)
{
    list_add_after(&listm_head->list_head, node, new_node,
               listm_head->offset);
}

static inline int listm_is_empty(const struct listm_head *listm_head)
{
    return list_is_empty(&listm_head->list_head);
}

static inline void *listm_get_first(const struct listm_head *listm_head)
{
    return list_get_first(&listm_head->list_head, listm_head->offset);
}

static inline void *listm_get_last(const struct listm_head *listm_head)
{
    return list_get_last(&listm_head->list_head, listm_head->offset);
}

static inline void *listm_get_next(const struct listm_head *listm_head,
                   const void *node)
{
    return list_get_next(node, listm_head->offset);
}

static inline void *listm_get_previous(const struct listm_head *listm_head,
                       const void *node)
{
    return list_get_previous(node, listm_head->offset);
}

static inline void listm_remove(struct listm_head *listm_head, const void *node)
{
    list_remove(&listm_head->list_head, node, listm_head->offset);
}

static inline int
listm_get_size(const struct listm_head *list)
{
    int length = 0;
    const void *ptr;

    if (listm_is_empty(list))
        return length;

    ptr = listm_get_first(list);
    length++;

    while (NULL != (ptr = listm_get_next(list, ptr)))
        length++;

    return length;
}

#endif /* #ifndef include_list_h */
