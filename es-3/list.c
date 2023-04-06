#include "list.h"
#include <stdlib.h>

void list_init(list_t *list){

    list->size = 0;
    list->head = NULL;
    list->tail = NULL;

}

void list_enqueue(list_t *list, struct node_t *el){

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

void list_fifo_dequeue(list_t *list){

    if(list->size == 0) return;

    struct node_t *old_head = list->head;

    list->head = list->head->next;
    list->size--;
    
    if(list->head == NULL) list->tail = NULL;

    free(old_head); 

}