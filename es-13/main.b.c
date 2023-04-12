#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define MAILBOX_SIZE 10

typedef enum {
    R1, R2
} thread_type_t;

typedef struct {
    int value;
} mes_t;

typedef struct {

    sem_t is_full, // fa entrare `THREADS` thread al massimo 
          is_empty,
          notify_r1, notify_r2,
          mutex;

    mes_t *messages; // array di messaggi
    int head, tail; // indici di testa e coda

} mailbox_t;

void print_mailbox(mailbox_t* mailbox){
    
    // assumo una mailbox di 10 elementi, non ho voglia di fare altro
    mes_t* m = mailbox->messages;
    printf("[%d] dice: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, head: %d, tail: %d\n", (int) pthread_self(), m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], mailbox->head, mailbox->tail);

}

void mailbox_init(mailbox_t* mailbox){

    mailbox->messages = malloc(sizeof(mes_t) * MAILBOX_SIZE);
    sem_init(&mailbox->mutex, 0, 1);
    sem_init(&mailbox->is_full, 0, MAILBOX_SIZE); // quando "finiscono i posti", questo semaforo blocca
    sem_init(&mailbox->is_empty, 0, 0); 
    sem_init(&mailbox->notify_r1, 0, 0);
    sem_init(&mailbox->notify_r2, 0, 0);
    mailbox->head = 0;
    mailbox->tail = 0;

}

void mailbox_put(mailbox_t* mailbox, mes_t* message, int thread_id){

    sem_wait(&mailbox->is_full);
    sem_wait(&mailbox->mutex);

    mailbox->messages[mailbox->head] = *message;
    mailbox->head = (mailbox->head + 1) % MAILBOX_SIZE;

    print_mailbox(mailbox);

    sem_post(&mailbox->mutex);
    sem_post(&mailbox->is_empty); // posto un messaggio, sveglio un eventuale thread bloccato


}

void receive_1(mailbox_t* mailbox, mes_t* received){

    sem_wait(&mailbox->is_empty);
    sem_wait(&mailbox->mutex);
    
    /*
        R1 deve semplicemente leggere, quindi non deve fare altro che prelevare il
        messaggio dalla mailbox
    */

    *received = mailbox->messages[mailbox->tail];

    sem_post(&mailbox->mutex);

}

void receive_2(mailbox_t* mailbox, mes_t* received){

    /*
        R2 dovrà leggere, incrementare la coda e notificare i sender che 
        la mailbox ha un elemento in meno!
        Non è necessario che R2 faccia una wait su is_empty, poiché se R1 ha letto, vuol dire che c'è
        almeno un elemento
    */
   
    sem_wait(&mailbox->notify_r2); // r1 ha finito, ottimo!
    sem_wait(&mailbox->mutex);

    *received = mailbox->messages[mailbox->tail];
    mailbox->tail = (mailbox->tail + 1) % MAILBOX_SIZE;

    sem_post(&mailbox->is_full);
    sem_post(&mailbox->mutex);

}

int done_1(mailbox_t* mailbox){

    /*
        una volta che avrà letto, R1 dovrà notificare R2 e poi dovrà
        bloccarsi in attesa che R2 abbia finito
    */
    sem_post(&mailbox->notify_r2); // dice a R2 di aver finito
    sem_wait(&mailbox->notify_r1); // aspetta che R2 abbia finito

}

int done_2(mailbox_t* mailbox){
    
    sem_post(&mailbox->notify_r1); // dice a R1 di aver finito, il processo ricomincia

}

void* r1(void* arg){

    mailbox_t *mailbox = (mailbox_t*) arg;
    while(1){
        mes_t *message = malloc(sizeof(mes_t));
        receive_1(mailbox, message);
        printf("R1 legge: %d\n", message->value);
        free(message);
        done_1(mailbox);
    }

    pthread_exit(0);

}

void* r2(void* arg){

    mailbox_t *mailbox = (mailbox_t*) arg;
    while(1){
        mes_t *message = malloc(sizeof(mes_t));
        receive_2(mailbox, message);
        printf("R2 legge: %d\n", message->value);
        free(message);
        done_2(mailbox);
    }

    pthread_exit(0);

}

void *sender(void* arg){

    int mess = 15;
    int thread_id = (int) pthread_self();
    printf("sender: %d\n", thread_id);

    while(1){

        mailbox_t *mailbox = (mailbox_t*) arg;
        mes_t *message = malloc(sizeof(mes_t));
        message->value = mess;

        mailbox_put(mailbox, message, thread_id);
        mess++;

    }

}

int main(void){

    // test_queue();
    pthread_attr_t a;
    pthread_t threads[6];
    mailbox_t *mailbox = malloc(sizeof(mailbox_t));
    mailbox_init(mailbox);

    pthread_attr_init(&a);

    pthread_create(&threads[0], &a, r1, mailbox); // R1
    pthread_create(&threads[1], &a, r2, mailbox); // R2

    pthread_create(&threads[2], &a, sender, mailbox); // 4 sender
    pthread_create(&threads[3], &a, sender, mailbox);
    pthread_create(&threads[4], &a, sender, mailbox);
    pthread_create(&threads[5], &a, sender, mailbox);

    sleep(60);

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(threads[2], NULL);

    return 0;

}