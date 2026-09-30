/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of Zephyr's singly linked list, with the type and field names Zephyr uses.
 */

#pragma once

#include <stddef.h>
#include <zephyr/sys/util.h>

struct _snode {
    struct _snode *next;
};
typedef struct _snode sys_snode_t;

struct _slist {
    sys_snode_t *head;
    sys_snode_t *tail;
};
typedef struct _slist sys_slist_t;

#define SYS_SLIST_STATIC_INIT(ptr_to_list) {NULL, NULL}

static inline void sys_slist_init(sys_slist_t *list) {
    list->head = NULL;
    list->tail = NULL;
}

static inline void sys_slist_append(sys_slist_t *list, sys_snode_t *node) {
    node->next = NULL;
    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
}

#define SYS_SLIST_FOR_EACH_CONTAINER(__sl, __cn, __n)                                              \
    for ((__cn) = ((__sl)->head != NULL)                                                           \
                      ? CONTAINER_OF((__sl)->head, __typeof__(*(__cn)), __n)                       \
                      : NULL;                                                                      \
         (__cn) != NULL;                                                                           \
         (__cn) = ((__cn)->__n.next != NULL)                                                       \
                      ? CONTAINER_OF((__cn)->__n.next, __typeof__(*(__cn)), __n)                   \
                      : NULL)
