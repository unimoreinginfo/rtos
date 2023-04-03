#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <semaphore.h>

typedef struct {

    char message[6]; // stringa di 5 caratteri null-terminata = 6 caratteri
    sem_t producers[5]; // 5 semafori per ciascun produttore 
    sem_t mutex; // semaforo che permette di modificare le variabili interne del buffer (numero di caratteri depositati, ad esempio)
    sem_t consumer; // semaforo che permette di "svegliare" il consumer quando il messaggio è pronto
    char iterations;  // variabile che permette di far stampare un messaggio diverso ogni volta (sommo questo)
                         // "padding" a ciascun carattere della stringa ad ogni iterazione, così avrò, ad esempio
                         // abcde, bcdef, cdefg, ..

    int n_chars; // numero di caratteri depositati
    int stop; // var che stoppa il consumer

} buf_t;

buf_t global_buf;
char* shared_message = "abcde";

void buf_init(buf_t* buf){

    buf->iterations = 0;
    buf->n_chars = 0;
    buf->message[5] = 0; // null-terminiamo il messaggio, così è stampabile in modo sicuro
    sem_init(&buf->mutex, 0, 1); // mutex, quindi valore = 1
    sem_init(&buf->consumer, 0, 0); // il consumer è dormiente finché non viene popolato tutto il buffer!
    for(int i = 0; i < 5; i++) sem_init(&buf->producers[i], 0, 0); // ogni produttore potrà depositare (e poi si bloccherà!)

}

void* producer(void* arg){ // ce ne saranno 5 di questi

    int thread_idx = *(int*) arg;

    char thread_char = shared_message[thread_idx]; // ogni thread prenderà un carattere della stringa "abcde".

    while(1){

        global_buf.message[thread_idx] = (thread_char + (global_buf.iterations));  // non mi serve fare mutua esclusione sul messaggio,
                                                                      // poiché ciascun thread opera su un indice diverso!
        printf("thread %d prende '%c'\n", thread_idx, thread_char);

        sem_wait(&global_buf.mutex);

        global_buf.n_chars++; // ciò su cui devo fare mutua esclusione, è la variabile che rappresenta quanti caratteri
                              // sono stati depositati finora, poiché potrebbero verificarsi eventuali race condition su quest'ultima,
                              // dato che è una risorsa condivisa
        if(global_buf.n_chars == 5)
            sem_post(&global_buf.consumer);

        sem_post(&global_buf.mutex);
        sem_wait(&global_buf.producers[thread_idx]); // ciascun produttore si blocca sul proprio semaforo, in attesa che il consumatore lo svegli
        // quando il consumatore sveglia il produttore, questo ricomincia il suo ciclo
        sleep(1);

    }


    pthread_exit(0);

}

void* consumer(void* arg){

    while(1){

        // tutto quello che deve fare il consumer è leggere la stringa e resettare le variabili di stato del buffer
        sem_wait(&global_buf.consumer); // questo mutex è a 0, quindi il consumatore si bloccherà finché qualcuno
                                        // non gli dirà qualcosa
        sem_wait(&global_buf.mutex);
        // il consumer è sveglio e potrà fare le sue cose indisturbato
        printf("letto il messaggio: %s\n", global_buf.message);
        global_buf.n_chars = 0; // resetto il numero di caratteri depositati 
        for(int i = 0; i < 5; i++)
            sem_post(&global_buf.producers[i]); // sveglio tutti i produttori

        global_buf.iterations = (global_buf.iterations + 1) % 26;
        sem_post(&global_buf.mutex);
        // mi addormento di nuovo
        
    }

    pthread_exit(0);

}

int main(void){

    pthread_attr_t a;
    pthread_t threads[6];
    int consumer_ids[5] = { 0, 1, 2, 3, 4 };
    int current_consumer = 0;

    pthread_attr_init(&a);

    buf_init(&global_buf);

    for(int i = 0; i < 6; i++){

        if(i == 0) 
            pthread_create(&threads[i], &a, consumer, NULL); // il primo processo è il lettore
        else{
            // è necessario fare un array per memorizzare gli indici dei thread, poiché,
            // se si passasse semplicemente il puntatore a producer_idx, questo risulterebbe
            // nella condivisione di tale puntatore da parte di tutti i thread, causando
            // un comportamento non voluto!

            // usando un array è garantito che ciascun thread prenderà il puntatore all'elemento
            // di tale array, risultando in un'associazione univoca thread => id.

            pthread_create(&threads[current_consumer], &a, producer, (void*) &consumer_ids[current_consumer]); // gli altri sono consumatori, ai quali assegno un indice
            current_consumer++;
        }

    }

    for(int i = 0; i < 6; i++){

        int *retval;
        pthread_join(threads[i], (void**) &retval);

    }

    return 0;

}