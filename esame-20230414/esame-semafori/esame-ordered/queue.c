#include "queue.h"
#include <stdlib.h>

void queue_init(queue_t *list){

    list->size = 0;
    list->head = NULL;
    list->tail = NULL;

}

void queue_enqueue(queue_t *list, struct node_t *el){

    // si assume che il nodo sia già inizializzato a dovere

    if(list->size == 0){

        list->head = el;
        list->tail = el;

    }else{

        list->tail->next = el;
        list->tail = el;      

    }

    list->size++;

}

void queue_fifo_dequeue(queue_t *list){

    if(list->size == 0) return;

    struct node_t *old_head = list->head;

    list->head = list->head->next;
    list->size--;
    
    if(list->head == NULL) list->tail = NULL;

    free(old_head); 

}