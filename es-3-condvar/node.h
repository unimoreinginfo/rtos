#pragma once

struct node_t {
    int message;
    struct node_t *next;
};


extern void node_init(struct node_t *node, int message);