#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define THREADS 5

typedef struct {

    unsigned int voters, quorum;
    pthread_mutex_t vote_mutex;
    pthread_cond_t cond_quorum;
    unsigned int votes_positive, votes_negative;

} urn_t;

urn_t global_urn;

void urn_init(urn_t* urn){

    pthread_mutexattr_t m_attr;
    pthread_condattr_t c_attr;
    
    urn->voters = THREADS;
    urn->votes_positive = 0;
    urn->votes_negative = 0;
    
    double half = ((double) urn->voters) / 2;
    urn->quorum = (unsigned int) ceil(half);

    if(urn->voters % 2 == 0){
        printf("ci deve essere un numero dispari di votanti!");
        exit(-1);
    }

    if(pthread_mutexattr_init(&m_attr) < 0){
        perror("pthread_mutexattr_init: ");
        exit(-1);
    }
    if(pthread_condattr_init(&c_attr) < 0){
        perror("pthread_condattr_init: ");
        exit(-1);
    }
    if(pthread_mutex_init(&urn->vote_mutex, &m_attr) < 0){
        perror("pthread_mutex_init: ");
        exit(-1);
    }
    if(pthread_cond_init(&urn->cond_quorum, &c_attr) < 0){
        perror("pthread_cond_mutex_init: ");
        exit(-1);
    }

}

void vote(int v){

    pthread_mutex_lock(&global_urn.vote_mutex);

    if(v == 0) global_urn.votes_negative++;
    if(v == 1) global_urn.votes_positive++;

    pthread_mutex_unlock(&global_urn.vote_mutex);
    pthread_cond_broadcast(&global_urn.cond_quorum); // ad ogni voto broadcasto che l'ho fatto, così tutti i thread
                                                     // in attesa si svegliano per verificare se hanno vinto o no

}

int risultato(){

    pthread_mutex_lock(&global_urn.vote_mutex);

    unsigned int positive_wins = global_urn.votes_positive >= global_urn.quorum;
    unsigned int negative_wins = global_urn.votes_negative >= global_urn.quorum;
    int result = -1;

    /*printf("[%d] pos: %d > %d = %d\n", pthread_self(), global_urn.votes_positive, global_urn.quorum, positive_wins);
    printf("[%d] neg: %d > %d = %d\n", pthread_self(), global_urn.votes_negative, global_urn.quorum, negative_wins);*/

    while(positive_wins == 0 && negative_wins == 0){

        assert(positive_wins == 0 && negative_wins == 0); // debugging assertion, questa condizione deve essere vera, altrimenti c'è un problema!

        pthread_cond_wait(&global_urn.cond_quorum, &global_urn.vote_mutex);
        positive_wins = global_urn.votes_positive >= global_urn.quorum;
        negative_wins = global_urn.votes_negative >= global_urn.quorum;
        /*printf("[%d] pos: %d > %d = %d\n", pthread_self(), global_urn.votes_positive, global_urn.quorum, positive_wins);
        printf("[%d] neg: %d > %d = %d\n", pthread_self(), global_urn.votes_negative, global_urn.quorum, negative_wins);*/
    }

    if(positive_wins > 0) result = 1; else result = 0;

    pthread_mutex_unlock(&global_urn.vote_mutex);
    return result;

}

void *thread(void *arg){

    int thread_id = pthread_self();
    int v = rand() % 2;
    printf("thread %d sta andando a votare\n", thread_id);

    vote(v);

    printf("thread %d ha votato %d\n", thread_id, v);

    if (v == risultato()) printf("Ho vinto (%d)!\n", thread_id);
    else printf("Ho perso (%d)!\n", thread_id);

    pthread_exit(0);

}

int main(void){

    srand(time(NULL));

    pthread_attr_t a;
    pthread_t threads[THREADS];

    pthread_attr_init(&a);

    urn_init(&global_urn);

    for(int i = 0; i < THREADS; i++){

        pthread_create(&threads[i], &a, thread, NULL);

    }

    for(int i = 0; i < THREADS; i++){

        int *retval;
        pthread_join(threads[i], (void**) &retval);

    }

    return 0;

}