#include "node.h"
#include <stdlib.h>

void node_init(struct node_t *node, int value){

    node->value = value;
    node->next = NULL;

}