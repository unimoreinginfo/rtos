#include "node.h"
#include <stdlib.h>

void node_init(struct node_t *node, int message){

    node->message = message;
    node->next = NULL;

}