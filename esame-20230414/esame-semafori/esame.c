#define _GNU_SOURCE

#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define N 2
#define M 1
#define P 3
#define E 5

typedef struct {

    int prenotazioni[P];
    int attrezzi[N];
    int coda_attrezzi[N];
    sem_t sem_attrezzi[N];
    sem_t mutex;

} palestra_t;

palestra_t palestra;

void init_palestra(palestra_t* p){

    for(int i = 0; i < N; i++){

        p->attrezzi[i] = M;
        p->coda_attrezzi[i] = 0;
        sem_init(&p->sem_attrezzi[i], 0, 0);

    }
    
    sem_init(&p->mutex, 0, 1);

    for(int i = 0; i < P; i++){

        p->prenotazioni[i] = -1;

    }

}

void usaattrezzo(palestra_t* p, int persona, int attrezzo){

    sem_wait(&p->mutex);

    // if(p->prenotazioni[persona] == attrezzo || p->attrezzi[attrezzo] != 0){ // rimosso perché è più "semanticamente corretto" scrivere "> 0"
                                                                
    if(p->prenotazioni[persona] == attrezzo || p->attrezzi[attrezzo] > 0){

        if(p->prenotazioni[persona] != attrezzo) p->attrezzi[attrezzo]--; // aggiunta: se la persona non ha prenotato, è necessario decrementare,
                                                                          // altrimenti no, perché l'ha già fatto in prenotazione!
                                                                          // in questo modo sono sicuro che se ci sono 0 attrezzi disponibili, ma ho prenotato,
                                                                          // allora posso entrare senza causare comportamenti non definiti, poiché non decremento di nuovo
        if(p->prenotazioni[persona] == attrezzo)
            p->prenotazioni[persona] = -1;

        printf("[%d] ottengo %d\n", persona, attrezzo);
        sem_post(&p->sem_attrezzi[attrezzo]);

    }else{
        printf("[%d] si blocca in coda attrezzo %d\n", persona, attrezzo);
        p->coda_attrezzi[attrezzo]++;
    }

    sem_post(&p->mutex);
    // sem_wait(&p->attrezzi[attrezzo]); // typo sul nome del semaforo
    sem_wait(&p->sem_attrezzi[attrezzo]);
    
}

void prenota(palestra_t* p, int persona, int attrezzo){

    sem_wait(&p->mutex);

    // if(p->coda_attrezzi[attrezzo] > 0){ // typo: non coda_attrezzi, ma attrezzi.
                                           // corretto perché è necessario controllare che vi siano attrezzi disponibili,
                                           // non che non vi sia coda, poiché è possibile che la persona in questione
                                           // stia utilizzando l'ultimo attrezzo rimasto e che nessuno si sia ancora accodato.
                                           // in tale caso, la coda sarebbe nulla, ma non ci sarebbero attrezzi disponibili, causando un undefined behaviour!
    if(p->attrezzi[attrezzo] > 0){
        printf("[%d] prenota attrezzo %d\n", persona, attrezzo);
        p->prenotazioni[persona] = attrezzo;
        p->attrezzi[attrezzo]--; // riservo già il posto per la persona. se esaurisco i posti, chi vorrà usare questo attrezzo aspetterà
    }    
    sem_post(&p->mutex);

}

void fineuso(palestra_t* p, int persona, int attrezzo){

    sem_wait(&p->mutex);

    printf("[%d] finisce con attrezzo %d\n", persona, attrezzo);
    if(p->coda_attrezzi[attrezzo] > 0){

        p->coda_attrezzi[attrezzo]--;
        sem_post(&p->sem_attrezzi[attrezzo]);

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