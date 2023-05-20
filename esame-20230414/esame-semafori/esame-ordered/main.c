/*
    premessa: non ho, disattentamente, letto l'ultima, importantissima, frase del compito: "Altrimenti l'accesso alle macchine è in ordine di arrivo".
    supponendo che le code semaforiche non siano FIFO, è necessario l'utilizzo di una coda. 
*/


#define _GNU_SOURCE

#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "queue.h"

#define N 2
#define M 1
#define P 3
#define E 5

typedef struct {

    int prenotazioni[P];
    int attrezzi[N];
    int coda_attrezzi[N];
    // sem_t sem_attrezzi[N]; // non serve un semaforo per attrezzo, ma un semaforo PER PERSONA
                              // cosicché gli utenti della palestra abbiano un semaforo privato sul quale bloccarsi
                              // questa soluzione prevede l'utilizzo, quindi, di N code nelle quali le persone si andranno ad accodare.
                              // all'interno di ciascuna coda vi sarà l'indice della persona in coda, in modo tale da poter segnalare, al momento
                              // del termine dell'utilizzo dell'attrezzo, mantenendo l'ordine di arrivo
    
    queue_t code[N];          // per quanto succitato, vi saranno N code nelle quali verranno memorizzati semplicemente gli indici di chi
                              // deve procedere, in modo che questi vengano rimossi in modo FIFO, garantendo l'ordine di arrivo al semaforo!          
    sem_t mutex;
    sem_t priv_persone[P]; // il semaforo privato succitato

} palestra_t;

palestra_t palestra;

void init_palestra(palestra_t* p){

    sem_init(&p->mutex, 0, 1);

    for(int i = 0; i < N; i++){

        p->attrezzi[i] = M;
        p->coda_attrezzi[i] = 0;

            // esame.c: 
            // sem_init(&p->sem_attrezzi[i], 0, 0); // lo rimuovo per quello che è stato detto sopra
        
        // ogni attrezzo avrà la sua coda inizializzata
        queue_init(&p->code[i]);

    }
    for(int i = 0; i < P; i++){

        p->prenotazioni[i] = -1;
        
        // aggiunta rispetto ad esame.c:
        sem_init(&p->priv_persone[i], 0, 0);
        // fine aggiunta

    }

}

void usaattrezzo(palestra_t* p, int persona, int attrezzo){

    sem_wait(&p->mutex);

    if(p->prenotazioni[persona] == attrezzo || p->attrezzi[attrezzo] > 0){

        if(p->prenotazioni[persona] != attrezzo) p->attrezzi[attrezzo]--;

        if(p->prenotazioni[persona] == attrezzo)
            p->prenotazioni[persona] = -1;
        
        printf("[%d] ottengo %d\n", persona, attrezzo);
        // sem_post(&p->sem_attrezzi[attrezzo]); // rimosso perché cambia la semantica di arrivo delle persone (guardare nota iniziale)
        sem_post(&p->priv_persone[persona]);
        
    }else{
        printf("[%d] si blocca in coda attrezzo %d\n", persona, attrezzo);
        
        // aggiunte rispetto ad esame.c per la semantica di arrivo
        struct node_t *nodo_persona = malloc(sizeof(struct node_t)); // lo alloco sull'heap perché poi la dequeue farà una free
                                                                     // ho scelto di allocare i nodi sull'heap perché così siamo al riparo
                                                                     // da eventuali stack overflow se ci sono tante persone/tanti esercizi
        node_init(nodo_persona, persona);
        queue_enqueue(&p->code[attrezzo], nodo_persona); // metto in coda l'indice della persona,
                                                          // quando qualcuno terminerà di usare l'attrezzo, si userà questo
                                                          // per garantire l'ordinamento delle persone
        // fine aggiunte 

        p->coda_attrezzi[attrezzo]++;
    }

    sem_post(&p->mutex);
    // sem_wait(&p->sem_attrezzi[attrezzo]); // rimosso perché cambia la semantica di attesa
    sem_wait(&p->priv_persone[persona]);
    
}

void prenota(palestra_t* p, int persona, int attrezzo){

    sem_wait(&p->mutex);

    if(p->attrezzi[attrezzo] > 0){
        printf("[%d] prenota attrezzo %d\n", persona, attrezzo);
        p->prenotazioni[persona] = attrezzo;
        p->attrezzi[attrezzo]--; 
    }
    
    sem_post(&p->mutex);

}

void fineuso(palestra_t* p, int persona, int attrezzo){

    sem_wait(&p->mutex);

    printf("[%d] finisce con attrezzo %d\n", persona, attrezzo);
    if(p->coda_attrezzi[attrezzo] > 0){
        p->coda_attrezzi[attrezzo]--;
    
        // sem_post(&p->sem_attrezzi[attrezzo]);

        // aggiunte rispetto ad esame.c:
        // tutto quello che devo fare è notificare la prima persona in coda,
        // ergo il nodo in testa alla coda!
        
        int sem_index = p->code[attrezzo].head->value;
        printf("[%d] sblocco l'utente %d\n", persona, sem_index);
        sem_post(&p->priv_persone[sem_index]);
        queue_fifo_dequeue(&p->code[attrezzo]); // rimuovo il nodo dalla coda
        // fine aggiunte

    }else{
        p->attrezzi[attrezzo]++;
    }

    sem_post(&p->mutex);

}

void nanopause(){
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = (rand()%10+1)*1000000;
    // Nano second.
    nanosleep(&t,NULL);
}

void *persona(void *arg){

    int numeropersona = *(int*) arg;
    int i;
    int attrezzocorrente = rand() % N,
        prossimoattrezzo = rand() % N;

    for(i = E; i > 0; i--){
        printf("[%d] uso: %d, userò: %d\n", numeropersona, attrezzocorrente, prossimoattrezzo);
        usaattrezzo(&palestra, numeropersona, attrezzocorrente);
        printf("[%d] sta usando attrezzo %d\n", numeropersona, attrezzocorrente);
        // sleep(1);
        // if(i != 0) prenota(&palestra, numeropersona, prossimoattrezzo); // attenzione! se il ciclo termina a i == 1! 
                                                                           // se la condizione fosse i != 0 una persona può prenotare e smettere di allenarsi, 
                                                                           // bloccando tutti gli altri!! 
        if(i != 1) prenota(&palestra, numeropersona, prossimoattrezzo);
        fineuso(&palestra, numeropersona, attrezzocorrente);

        if(i != 0){

            attrezzocorrente = prossimoattrezzo;
            prossimoattrezzo = rand() % N;

        }
        nanopause();
    }

    pthread_exit(0);

}

int main(void){

    srand(time(NULL));
    pthread_attr_t a;
    pthread_t persone[P];
    int id_persone[P];

    init_palestra(&palestra);
    pthread_attr_init(&a);

    for(int i = 0; i < P; i++){

        id_persone[i] = i;
        pthread_create(&persone[i], &a, persona, &id_persone[i]);

    }

    for(int i = 0; i < P; i++){

        void* ret;
        pthread_join(persone[i], &ret);

    }

    return 0;

}