#pragma once
#include "node.h"

typedef struct {
    int size;
    struct node_t *head, *tail;
} list_t;

extern void list_init(list_t *list); // inizializza la lista
extern void list_enqueue(list_t *list, struct node_t *el); // inserisce un elemento in coda alla lista
extern void list_fifo_dequeue(list_t *list); // rimuove il primo elemento dalla lista