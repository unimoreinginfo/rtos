#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define RUNNERS 150

typedef struct {

    pthread_mutex_t mutex; // mutex usato per modificare le variabili interne
    pthread_cond_t ref_wait; // condvar usata dall'arbitro per aspettare che l'ultimo runner in attesa lo notifichi
    pthread_cond_t ref_wait_end; // condvar usata dall'ultimo runner in arrivo per notificare l'arbitro che la gara è finita
    pthread_cond_t runner_wait; // condvar usata dall'arbitro per notificare i runner che la gara parte (con un broadcast)
    
    int total_runners, // numero totale di runner, utile per le condizioni bloccanti
        finished, // numero di runner che hanno finito ad un determinato momento
        waiting, // numero di runner che stanno aspettando ad un determinato momento
        winner, last; // id dei thread vincitore e perdente

} race_t;

void race_init(race_t *race){

    pthread_condattr_t cond_attr;
    pthread_mutexattr_t mutex_attr;

    pthread_condattr_init(&cond_attr);
    pthread_mutexattr_init(&mutex_attr);

    race->total_runners = RUNNERS;
    race->finished = 0;
    race->waiting = 0;
    race->winner = -1;
    race->last = -1;

    pthread_mutex_init(&race->mutex, &mutex_attr);
    pthread_cond_init(&race->ref_wait, &cond_attr);
    pthread_cond_init(&race->runner_wait, &cond_attr);
    pthread_cond_init(&race->ref_wait_end, &cond_attr);

}

void ref_wait(race_t *race){

    pthread_mutex_lock(&race->mutex);

    /*
        qua sono possibili due scenari:
            - o tutti i thread corridori si sono già posizionati
            - ci sono ancora thread corridori che non sono posizionati

        in entrambi i casi, la semantica di questa funzione è corretta, poiché:
            - nel primo caso, il while non verrebbe attraversato e l'arbitro andrebbe direttamente a far partire la corsa
            - nel secondo caso, il while verrebbe attraversato e l'arbitro si bloccherebbe, in attesa che l'ultimo corridore
                vada a fare la signal.
    */

    while(race->waiting != race->total_runners){
        pthread_cond_wait(&race->ref_wait, &race->mutex);
    }

    pthread_mutex_unlock(&race->mutex);

}

void runner_wait(race_t *race, int runner_no){

    pthread_mutex_lock(&race->mutex);

    race->waiting++;
    if(race->waiting == race->total_runners){
        printf("%d dice che è tutto ok e sveglia l'arbitro\n", runner_no);
        pthread_cond_signal(&race->ref_wait); // tutti i corridori sono sulla linea di partenza
    }

    printf("%d è in posizione\n", runner_no);
    pthread_cond_wait(&race->runner_wait, &race->mutex); // non è necessario mettere questa wait in un while, perché un corridore non può diventare
                                                         // "non pronto"
    pthread_mutex_unlock(&race->mutex);

}

void ref_wait_runners(race_t *race, int* first, int* last){

    pthread_mutex_lock(&race->mutex);

    /*
        in modo analogo alla ref_wait, anche qui i due casi sono sempre quelli.
        la semantica li soddisfa entrambi
    */

    while(race->finished != race->total_runners){
        pthread_cond_wait(&race->ref_wait_end, &race->mutex);
    }

    // sicuramente tutti i corridori hanno finito
    printf("[arbitro] ha vinto %d e l'ultimo classificato è %d", *first, *last);

    pthread_mutex_unlock(&race->mutex);

}

void runner_finish_line(race_t *race, int runner_no){

    pthread_mutex_lock(&race->mutex);

    race->finished++; 
    printf("%d taglia il traguardo!\n", runner_no);
    
    // se sono il primo ad aver finito, "finished" sarà necessariamente a 1
    // mentre se finished è uguale al numero di corridori, sarò necessariamente l'ultimo

    if(race->finished == 1) race->winner = runner_no;
    if(race->finished == race->total_runners){
        race->last = runner_no;
        pthread_cond_signal(&race->ref_wait_end); // sono l'ultimo arrivato: posso dire all'arbitro che abbiamo finito
    }

    pthread_mutex_unlock(&race->mutex);

}


void *ref(void *arg){

    race_t *race = (race_t*) arg;

    ref_wait(race);
    printf("[arbitro] sono tutti pronti\n");
    printf("[arbitro] ready...\n");
    printf("[arbitro] set...\n");
    printf("[arbitro] GO!\n");
    pthread_cond_broadcast(&race->runner_wait); // tutti i corridori vengono sbloccati = "arbitro_via"
    ref_wait_runners(race, &race->winner, &race->last);

}

void *runner(void* arg){

    race_t *race = (race_t*) arg;
    int thread = (int) pthread_self();

    runner_wait(race, thread);
    runner_finish_line(race, thread);

}

int main(){

  srand(time(NULL));
  pthread_attr_t attr;
  pthread_t threads[RUNNERS + 1];
  race_t *race = malloc(sizeof(race_t));

  race_init(race);
  pthread_attr_init(&attr);

  for(int i = 0; i < RUNNERS + 1; i++){
    if(i == 0){
      pthread_create(&threads[i], &attr, ref, (void* ) race);
    }else{
      pthread_create(&threads[i], &attr, runner, (void* ) race);
    }
  }

  for(int i = 0; i < RUNNERS + 1; i++){
    
    pthread_join(threads[i], NULL);

  }

  return 0;

}