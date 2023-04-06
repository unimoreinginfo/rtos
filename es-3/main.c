#include "list.h"
#include "node.h"
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define AVAIL_ENVELOPES 2
#define THREADS 6

bool enabled = true;
typedef enum { low, medium, high } prio_t;
static void do_sender_routine(void* arg, prio_t priority);

typedef struct {

    list_t *mailbox; // la mailbox nella quale i mittenti inseriranno le buste piene (nodi)

    sem_t mutex; // mutex che garantisce l'atomicità delle operazioni
    sem_t available; // semaforo che sarà inizializzato al valore "available_envelopes".
                     // bloccherà quando non ci saranno più buste disponibili!
    sem_t blocked_low,
          blocked_medium; // semafori che rappresentano i processi bloccati in base alla loro priorità
                          // si noti come un semaforo che rappresenta i processi bloccati sulla priorità alta
                          // non c'è, poiché questi "saltano la coda"!
    sem_t receive; // semaforo usato dai riceventi per aspettare che vi siano messaggi nella mailbox!
                   // viene inizializzato a 0, poiché la receive è bloccante se non vi sono messaggi nella mailbox.

    int blocked_prio_low,
        blocked_prio_medium,
        blocked_prio_high; // contatori che rappresentano quanti thread sono bloccati per priorità
                           // verranno usati per valutare se un thread potrà accodarsi per prendere la 
                           // busta o meno

} mailer_t;

void mailer_init(mailer_t* mailer){

    mailer->mailbox = malloc(sizeof(list_t));
    mailer->blocked_prio_low = 0;
    mailer->blocked_prio_medium = 0;
    mailer->blocked_prio_high = 0;

    list_init(mailer->mailbox);
    if(sem_init(&mailer->mutex, 0, 1) < 0) { perror("sem_init"); exit(-1); };
    if(sem_init(&mailer->available, 0, AVAIL_ENVELOPES) < 0) { perror("sem_init"); exit(-1); };
    if(sem_init(&mailer->blocked_low, 0, 0) < 0) { perror("sem_init"); exit(-1); };
    if(sem_init(&mailer->blocked_medium, 0, 0) < 0) { perror("sem_init"); exit(-1); };
    if(sem_init(&mailer->receive, 0, 0) < 0) { perror("sem_init"); exit(-1); };

}

void if_schedulable(mailer_t *mailer, prio_t prio, int thread_id){
    
    sem_wait(&mailer->mutex);
    // questa funzione controlla che la priorità dei processi sia rispettata nell'ottenere
    // una busta.
    // in particolare, thread con priorità alta verranno schedulati "saltando la coda",
    // thread di priorità media salteranno quelli a priorità bassa, mentre quelli a priorità bassa
    // verranno schedulati per ultimi, dopo che tutti gli altri hanno ottenuto una busta

    if(prio == low){
        // se la priorità è "low", devo controllare che non ci siano thread
        // di priorità maggiore già bloccati
        if(mailer->blocked_prio_high > 0 || mailer->blocked_prio_medium > 0){
            mailer->blocked_prio_low++;
            sem_post(&mailer->mutex); // ricordiamoci di uscire dalla zona critica, folks!!
            sem_wait(&mailer->blocked_low);
        }else sem_post(&mailer->mutex);
    }else if(prio == medium){

        if(mailer->blocked_prio_high > 0){
            mailer->blocked_prio_medium++;
            sem_post(&mailer->mutex);
            sem_wait(&mailer->blocked_medium);
        }else sem_post(&mailer->mutex);

    }else if(prio == high){
        mailer->blocked_prio_high++; //./ se il thread ha priorità alta, l'unica condizione per la quale questo si blocca
                                     // è se non ci sono buste libere. se questo fosse il caso, un thread a priorità alta si bloccherebbe
                                     // su sem_wait(&mailer->available) nella funzione mailer_get_envelope, pertanto, se questo è il caso
                                     // incrementare andrebbe bene (perché si blocca effettivamente).
                                     // qualora non si blocchi, il numero dei thread a priorità alta bloccati decrementa in ogni caso una volta che si passa la
                                     // barriera data dalla wait succitata, quindi i dati tornano "sensati"
        sem_post(&mailer->mutex);
    }

}

void mailer_decrease_blocked_by_prio(mailer_t *mailer, prio_t prio, int thread_id){

    // questa funzione viene chiamata dentro una zona critica, pertanto non mi serve il mutex
    switch(prio){
        case low: mailer->blocked_prio_low--; break;
        case medium: mailer->blocked_prio_medium--; break;
        case high: mailer->blocked_prio_high--; break;
    }

    if(mailer->blocked_prio_medium > 0) 
        sem_post(&mailer->blocked_medium);
    else if(mailer->blocked_prio_low > 0)
        sem_post(&mailer->blocked_low);

    // il branching qua sopra sfrutta l'ipotesi che le code semaforiche siano FIFO,
    // quindi processi che hanno la stessa priorità verranno sbloccati nell'ordine
    // in cui sono arrivati al semaforo stesso

}

void mailer_get_envelope(mailer_t *mailer, prio_t prio, int thread_id){

    // notare come questo approccio definisca la coda dei processi che attendono per una 
    // busta nuova PRIMA che si vada ad attendere la disponibilità di eventuali buste
    if_schedulable(mailer, prio, thread_id);
    sem_wait(&mailer->available); // se il valore di questo semaforo è 0, mi blocco e non entro in questa zona!
    sem_wait(&mailer->mutex); // acquisisco il mutex perché devo fare delle modifiche al mailer_t
    mailer_decrease_blocked_by_prio(mailer, prio, thread_id);

    // a questo punto, il mittente ha ottenuto una busta vuota

    sem_post(&mailer->mutex);

}

void mailer_append_envelope(mailer_t* mailer, struct node_t *envelope, int thread_id){

    sem_wait(&mailer->mutex);
    list_enqueue(mailer->mailbox, envelope); // molto semplicemente, si tratta di fare una enqueue thread safe
    sem_post(&mailer->receive); // notifico un eventuale ricevente bloccato!
    sem_post(&mailer->mutex);

}

void mailer_get_mail(mailer_t* mailer, int thread_id){

    // questa funzione viene chiamata dai riceventi per prendere l'ultimo messaggio inserito
    // qualora non ci siano messaggi, questa funzione blocca finché non ve ne sarà uno
    sem_wait(&mailer->receive); // se non ci sono messaggi nella mailbox, mi blocco e aspetto che qualcuno li metta
    sem_wait(&mailer->mutex);

    struct node_t *recv_envelope = mailer->mailbox->head;
    printf("[RECEIVER %d] ricevuto messaggio %d\n", thread_id, recv_envelope->message);
    list_fifo_dequeue(mailer->mailbox); // la testa della lista viene deallocata con una free,
                                        // per questo è importante che il sender la allochi dinamicamente

    sem_post(&mailer->mutex);

}

void mailer_add_envelope(mailer_t *mailer){

    sem_post(&mailer->available); // c'è una nuova busta, pertanto notifico eventuali mittenti in coda

}

void *sender_low(void *arg){ 
    printf("low: %d\n", (int) pthread_self());
    do_sender_routine(arg, low);
    pthread_exit(0);
}
void *sender_medium(void *arg){ 
    printf("med: %d\n", (int) pthread_self());
    do_sender_routine(arg, medium); 
    pthread_exit(0);
}
void *sender_high(void *arg){ 
    printf("high: %d\n", (int) pthread_self());
    do_sender_routine(arg, high); 
    pthread_exit(0);
}

void do_sender_routine(void *arg, prio_t priority){

    mailer_t *mailer = (mailer_t*) arg;
    int message = (int) pthread_self();

    while(enabled){
        mailer_get_envelope(mailer, priority, message);
        // ho ottenuto una busta in modo atomico
        struct node_t *envelope = malloc(sizeof(struct node_t)); // la alloco, perché non posso fare free (quando un ricevente legge, fa la free del nodo letto)
                                                                // di roba sullo stack
        node_init(envelope, message);
        printf("[PRODUCER %d] produco...\n", message);
        mailer_append_envelope(mailer, envelope, message);
    }

}

void *receiver(void *arg){

    int thread_id = (int) pthread_self();

    while(enabled){
        mailer_t *mailer = (mailer_t*) arg;
        mailer_get_mail(mailer, thread_id);
        // sleep(1); // simulo un tempo lungo per processare il messaggio
        mailer_add_envelope(mailer);
        printf("[RECEIVER %d] aggiungo busta...\n", thread_id);
    }

    pthread_exit(0);
}

void test_queue(){

    list_t *list = malloc(sizeof(list_t));
    list_init(list);
    
    for(int i = 0; i < 10; i++){

        struct node_t *node = malloc(sizeof(struct node_t));
        node_init(node, i);
        list_enqueue(list, node);

    }

    printf("size: %d, head: %d, tail: %d\n", list->size, list->head->message, list->tail->message);

    for(int i = 0; i < 10; i++){

        struct node_t *curr = list->head;
        printf("curr message: %d\n", curr->message);
        list_fifo_dequeue(list);

    }

    printf("size: %d\n", list->size);
    printf("tail is null: %d\n", list->tail == NULL);
    printf("head is null: %d\n", list->head == NULL);

    free(list);

}

int main(void){

    // test_queue();
    pthread_attr_t a;
    pthread_t threads[THREADS];
    mailer_t *mailer = malloc(sizeof(mailer_t));
    mailer_init(mailer);

    pthread_attr_init(&a);

    pthread_create(&threads[0], &a, receiver, mailer);
    pthread_create(&threads[1], &a, receiver, mailer);
    pthread_create(&threads[2], &a, sender_high, mailer);
    pthread_create(&threads[3], &a, sender_low, mailer);
    pthread_create(&threads[4], &a, sender_high, mailer);
    pthread_create(&threads[5], &a, sender_medium, mailer);

    sleep(60);

    return 0;

}