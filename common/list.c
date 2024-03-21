/*
 * Copyright 2021-2024 Bull SAS
 */

#include <assert.h>
#include <stdint.h>

#include "list.h"

static inline struct list_node *void2node(const void *ptr, size_t offset)
{
    assert(offset != ~(size_t)0);

    return (void *)((uintptr_t)ptr + offset);
}

static inline void *node2void(const struct list_node *node, size_t offset)
{
    assert(offset != ~(size_t)0);

    return (void *)((uintptr_t)node - offset);
}

void list_head_init(struct list_head *list_head)
{
    list_head->first = NULL;
    list_head->last = NULL;
}

void list_head_finalize(const struct list_head *list_head __attribute__((unused)))
{
    assert(list_head->first == NULL);
    assert(list_head->last == NULL);
}

void list_add_first(struct list_head *list_head, const void *new_node,
            size_t offset)
{
    struct list_node *ptr_new_node;

    ptr_new_node = void2node(new_node, offset);
    ptr_new_node->previous = NULL;
    ptr_new_node->next = list_head->first;
    if (list_head->first)
        list_head->first->previous = ptr_new_node;
    else
        list_head->last = ptr_new_node;
    list_head->first = ptr_new_node;
}

void list_add_last(struct list_head *list_head, const void *new_node,
           size_t offset)
{
    struct list_node *ptr_new_node = void2node(new_node, offset);

    ptr_new_node->next = NULL;
    ptr_new_node->previous = list_head->last;
    if (list_head->last)
        list_head->last->next = ptr_new_node;
    else
        list_head->first = ptr_new_node;
    list_head->last = ptr_new_node;
}

void list_add_before(struct list_head *list_head, const void *node,
             const void *new_node, size_t offset)
{
    struct list_node *ptr_node;
    struct list_node *ptr_new_node;

    ptr_node = void2node(node, offset);
    ptr_new_node = void2node(new_node, offset);

    ptr_new_node->previous = ptr_node->previous;
    if (ptr_node->previous)
        ptr_node->previous->next = ptr_new_node;
    else
        list_head->first = ptr_new_node;
    ptr_new_node->next = ptr_node;
    ptr_node->previous = ptr_new_node;
}

void list_add_after(struct list_head *list_head, const void *node,
            const void *new_node, size_t offset)
{
    struct list_node *ptr_node;
    struct list_node *ptr_new_node;

    ptr_node = void2node(node, offset);
    ptr_new_node = void2node(new_node, offset);

    ptr_new_node->previous = ptr_node;
    ptr_new_node->next = ptr_node->next;
    if (ptr_node->next)
        ptr_node->next->previous = ptr_new_node;
    else
        list_head->last = ptr_new_node;
    ptr_node->next = ptr_new_node;
}

void *list_get_first(const struct list_head *list_head, size_t offset)
{
    if (list_head->first)
        return node2void(list_head->first, offset);
    return NULL;
}

void *list_get_last(const struct list_head *list_head, size_t offset)
{
    if (list_head->last)
        return node2void(list_head->last, offset);
    return NULL;
}

void *list_get_next(const void *node, size_t offset)
{
    const struct list_node *ptr_node = void2node(node, offset)->next;
    if (ptr_node)
        return node2void(ptr_node, offset);
    return NULL;
}

void *list_get_previous(const void *node, size_t offset)
{
    const struct list_node *ptr_node = void2node(node, offset)->previous;
    if (ptr_node)
        return node2void(ptr_node, offset);
    return NULL;
}

void list_remove(struct list_head *list_head, const void *node, size_t offset)
{
    struct list_node *ptr_node;

    ptr_node = void2node(node, offset);
    if (ptr_node->previous)
        ptr_node->previous->next = ptr_node->next;
    else
        list_head->first = ptr_node->next;
    if (ptr_node->next)
        ptr_node->next->previous = ptr_node->previous;
    else
        list_head->last = ptr_node->previous;
}

void listm_head_init(struct listm_head *listm_head, size_t offset)
{
    listm_head->offset = offset;
    list_head_init(&listm_head->list_head);
}

void listm_head_finalize(struct listm_head *listm_head)
{
    list_head_finalize(&listm_head->list_head);
    listm_head->offset = ~(size_t)0;
}
