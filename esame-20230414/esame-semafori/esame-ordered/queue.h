#pragma once
#include "node.h"

typedef struct {
    int size;
    struct node_t *head, *tail;
} queue_t;

extern void queue_init(queue_t *list); // inizializza la coda
extern void queue_enqueue(queue_t *list, struct node_t *el); // inserisce un elemento in coda
extern void queue_fifo_dequeue(queue_t *list); // rimuove il primo elemento dalla coda