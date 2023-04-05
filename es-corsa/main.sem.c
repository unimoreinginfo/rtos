#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define RUNNERS_NO 150

typedef struct {

  int runners_no; // quanti sono i thread che partecipano alla corsa
  int finished; // quanti thread hanno finito di correre
  int waiting; // quanti thread sono arrivati alla linea di partenza
  bool first; // rappresenta se qualcuno è arrivato prima del thread che andrà a leggere questa variabile
  int first_no, last_no; // i numeri dei primi e degli ultimi

  sem_t mutex; // mutex che protegge la zona critica
  sem_t race_start; // semaforo che segnala l'inizio della corsa, sul quale tutti i thread "corridori" aspetteranno
  sem_t ref_notify; // semaforo che rappresenta l'arbitro, usato per segnalare all'arbitro stesso che tutti i corridori sono pronti


} race_t;

void random_sleep(){

  int rand_time = (rand() % (10 - 2) + 1); // random fra 1 e 10
  sleep(rand_time);

}

void race_init(race_t* race){

  race->runners_no = RUNNERS_NO;
  race->finished = 0;
  race->waiting = 0;
  race->first = false;

  sem_init(&race->mutex, 0, 1); // mutex, quindi inizializzato a 1
  sem_init(&race->race_start, 0, 0); // questo è a 0, poiché tutti i thread dovranno aspettare che l'arbitro dia il via
  sem_init(&race->ref_notify, 0, 0); // l'arbitro deve aspettare

}

void runner_wait(race_t* race, int runner_no){

  /*
    quello che il corridore deve fare è semplicemente incrementare il contatore
    che rappresenta i corridori in attesa, in modo che l'ultimo thread
    corridore possa "dire" all'arbitro che la gara è pronta per partire.
  */

  sem_wait(&race->mutex);
  race->waiting++;
  printf("corridore %d pronto\n", runner_no);

  if(race->waiting == race->runners_no)
    sem_post(&race->ref_notify);

  sem_post(&race->mutex);
  sem_wait(&race->race_start);

}

void runner_finish_line(race_t* race, int runner_no){

  /*
    quando un corridore arriva alla fine della gara deve "registrare" il proprio arrivo,
    inoltre deve controllare se è primo o ultimo!
    se nessuno è arrivato primo, necessariamente race.first sarà falso, pertanto il thread
    potrà registrarsi come vincitore.
    se tutti gli altri sono arrivati ("finished" è uguale al numero di partecipanti), allora il thread sarà l'ultimo.
  */

  sem_wait(&race->mutex);
  printf("%d taglia il traguardo!\n", runner_no);
  race->finished++;
  if(!race->first){
    race->first = true;
    race->first_no = runner_no;
  }
  if(race->finished == race->runners_no){
    // sono l'ultimo thread :(
    race->last_no = runner_no;
    sem_post(&race->ref_notify); // dico all'arbitro che l'ultimo è arrivato
  }
  sem_post(&race->mutex);
}

void ref_wait(race_t* race){

  sem_wait(&race->ref_notify); // l'arbitro deve aspettare che l'ultimo corridore gli
                               // dica che tutti sono schierati

}

void ref_announce(race_t* race, int *first, int *last){

  sem_wait(&race->ref_notify);

  printf("[arbitro] %d è arrivato primo, mentre %d è arrivato ultimo", *first, *last);

}

void *ref(void *arg){

  race_t *race = (race_t*) arg; 

  ref_wait(race);
  sem_wait(&race->mutex); // l'arbitro deve fare robe sulla sezione critica, quindi deve prendere
                          // il controllo del mutex

  race->waiting = 0;
  printf("[arbitro] ready...\n");
  sleep(1);
  printf("[arbitro] set...\n");
  random_sleep();
  printf("[arbitro] GO!\n");
  sem_post(&race->mutex);

  for(int i = 0; i < race->runners_no; i++){
    sem_post(&race->race_start);
  }

  ref_announce(race, &race->first_no, &race->last_no);

}

void *runner(void *arg){

  race_t *race = (race_t*) arg;
  int thread_id = (int) pthread_self();
  
  runner_wait(race, thread_id);
  printf("corridore %d scatta!\n", thread_id);
  runner_finish_line(race, thread_id);

}

int main(){

  srand(time(NULL));
  pthread_attr_t attr;
  pthread_t threads[RUNNERS_NO + 1];
  race_t *race = malloc(sizeof(race_t));

  race_init(race);
  pthread_attr_init(&attr);

  for(int i = 0; i < RUNNERS_NO + 1; i++){
    if(i == 0){
      pthread_create(&threads[i], &attr, ref, (void* ) race);
    }else{
      pthread_create(&threads[i], &attr, runner, (void* ) race);
    }
  }

  for(int i = 0; i < RUNNERS_NO + 1; i++){
    
    pthread_join(threads[i], NULL);

  }

  return 0;

}
