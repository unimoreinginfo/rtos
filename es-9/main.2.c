#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <semaphore.h>

typedef enum { a, b, none } owner_t;

typedef struct {

    owner_t owner; // qual è il gruppo che controlla il buffer
    char message[6]; // stringa di 5 caratteri null-terminata = 6 caratteri
    sem_t priv_sem[10]; // 10 semafori, uno per produttore
    sem_t mutex; // semaforo che permette di modificare le variabili interne del buffer (numero di caratteri depositati, ad esempio)
    sem_t consumer; // semaforo che permette di "svegliare" il consumer quando il messaggio è pronto

    int n_chars; // numero di caratteri depositati
    int blocked_threads[10]; // indice = 1 => thread bloccato

} buf_t;

buf_t global_buf;
char* shared_message_a = "msg_a";
char* shared_message_b = "msg_b";

void zerofill(int* buf){

    for(int i = 0; i < 10; i++) buf[i] = 0;

}

void buf_init(buf_t* buf){

    zerofill(buf->blocked_threads);
    buf->owner = none;
    buf->n_chars = 0;
    buf->message[5] = 0; // null-terminiamo il messaggio, così è stampabile in modo sicuro
    sem_init(&buf->mutex, 0, 1); // mutex, quindi valore = 1
    sem_init(&buf->consumer, 0, 0); // il consumer è dormiente finché non viene popolato tutto il buffer!

    for(int i = 0; i < 10; i++){
        sem_init(&buf->priv_sem[i], 0, 0); // ogni thread avrà il suo semaforo
    }

}

void* producerA(void* arg){ // ce ne saranno 5 di questi

    int thread_idx = *(int*) arg;
    char thread_char = shared_message_a[thread_idx % 5]; // ogni thread prenderà un carattere della stringa "msg_a".

    while(1){

        sem_wait(&global_buf.mutex);
        
        if(global_buf.owner == none){
            printf("vince A grazie a %d_a!\n", thread_idx);
            global_buf.owner = a;
        }else if(global_buf.owner == b){
            printf("%d_a si sospende\n", thread_idx);
            global_buf.blocked_threads[thread_idx] = 1;

            sem_post(&global_buf.mutex);
            sem_wait(&global_buf.priv_sem[thread_idx]);
            continue; // skippo il ciclo
        }
        printf("%d_a procede\n", thread_idx);

        global_buf.message[thread_idx % 5] = thread_char; // ora il buffer è conteso, poiché ci sono più thread che accedono agli stessi indici!
        global_buf.n_chars++;

        if(global_buf.n_chars == 5){
            sem_post(&global_buf.consumer);
        }

        global_buf.blocked_threads[thread_idx] = 1;
        sem_post(&global_buf.mutex);
        sem_wait(&global_buf.priv_sem[thread_idx]); // aspetto che il consumer mi svegli

    }


    pthread_exit(0);

}


void* producerB(void* arg){ // ce ne saranno 5 di questi

    int thread_idx = *(int*) arg;
    char thread_char = shared_message_b[thread_idx % 5]; // ogni thread prenderà un carattere della stringa "msg_b".

    while(1){

        sem_wait(&global_buf.mutex);
        
        if(global_buf.owner == none){
            printf("vince B grazie a %d_b!\n", thread_idx);
            global_buf.owner = b;
        }else if(global_buf.owner == a){
            printf("%d_b si sospende\n", thread_idx);
            global_buf.blocked_threads[thread_idx] = 1;

            sem_post(&global_buf.mutex);
            sem_wait(&global_buf.priv_sem[thread_idx]);
            continue; // skippo il ciclo
        }

        printf("%d_b procede\n", thread_idx);
        global_buf.message[thread_idx % 5] = thread_char; // ora il buffer è conteso, poiché ci sono più thread che accedono agli stessi indici!
        global_buf.n_chars++;

        if(global_buf.n_chars == 5){
            sem_post(&global_buf.consumer);
        }

        global_buf.blocked_threads[thread_idx] = 1;
        sem_post(&global_buf.mutex);
        sem_wait(&global_buf.priv_sem[thread_idx]); // aspetto che il consumer mi svegli

    }


    pthread_exit(0);

}


void* consumer(void* arg){

    while(1){

        // tutto quello che deve fare il consumer è leggere la stringa e resettare le variabili di stato del buffer
        sem_wait(&global_buf.consumer); // questo mutex è a 0, quindi il consumatore si bloccherà finché qualcuno
                                        // non gli dirà qualcosa

        sem_wait(&global_buf.mutex);

        if(global_buf.owner == a) printf("stampo il messaggio del gruppo a: %s\n", global_buf.message);
        else printf("stampo il messaggio del gruppo b: %s\n", global_buf.message);

        global_buf.n_chars = 0;
        global_buf.owner = none;

        for(int i = 0; i < 10; i++){
            if(global_buf.blocked_threads[i] == 1){
                sem_post(&global_buf.priv_sem[i]); // sveglio tutti E SOLO quelli bloccati
                // nota: se facessi post su tutti gli indici di questo array, senza verificare se il thread è bloccato o meno
                // andrei ad aumentare di 1 semafori che non necessitano di essere incrementati! 
                // ciò porterebbe a fare "passare" alcuni thread che non ne hanno bisogno!

                /*
                    supponiamo, ad esempio, che i thread vengano schedulati in questo ordine e che non si stia controllando la condizione succitata:
                    A1, A2, A3, A4, A5, consumer, B1, B2, B3, B4, B5
                    in questo particolare scenario, vincerebbe sicuramente il gruppo A e, inoltre,
                    i thread B non avrebbero modo di eseguire prima che venga stampato il messaggio,
                    poiché non appena A5 finisce di mettere il messaggio nel buffer, questo notificherà
                    il consumer, che oltretutto è il prossimo thread in coda, che procederà a fare la sua routine.
                    a questo punto, se il consumer facesse post su tutti i semafori in modo indiscriminato, esso
                    andrebbe a mettere a 1 tutti i semafori dei thread Ax, comportamento normale, poiché tutti i thread Ax
                    stanno aspettando di essere svegliati, ma anche tutti i semafori dei thread Bx!
                    A questo punto, quando vincerà un thread B, questo NON SI FERMERÀ A LINEA 118 sem_wait(&global_buf.priv_sem[thread_idx]),
                    pertanto NON ASPETTERÀ CHE IL THREAD CONSUMER LO SVEGLI. 
                    
                    Oltretutto, se lo scheduling dei thread fosse particolarmente sfortunato, potrebbe capitare che il semaforo
                    di ciascun thread Bx venisse settato a valori superiori a 1, causando deadlock nel momento in cui uno dei thread
                    Bx vincesse la corsa sul mutex!

                */
            }
        }

        zerofill(global_buf.blocked_threads);
        sleep(1);
        sem_post(&global_buf.mutex);
        // mi addormento di nuovo
        
    }

    pthread_exit(0);

}

int main(void){

    pthread_attr_t a;
    pthread_t threads[11];
    int consumer_ids[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    int current_consumer = 0;

    pthread_attr_init(&a);

    buf_init(&global_buf);

    pthread_create(&threads[0], &a, consumer, NULL); // il primo processo è il lettore
    pthread_create(&threads[1], &a, producerA, (void*) &consumer_ids[0]);
    pthread_create(&threads[2], &a, producerA, (void*) &consumer_ids[1]);
    pthread_create(&threads[3], &a, producerA, (void*) &consumer_ids[2]);
    pthread_create(&threads[4], &a, producerA, (void*) &consumer_ids[3]);
    pthread_create(&threads[5], &a, producerA, (void*) &consumer_ids[4]);
    pthread_create(&threads[6], &a, producerB, (void*) &consumer_ids[5]);
    pthread_create(&threads[7], &a, producerB, (void*) &consumer_ids[6]);
    pthread_create(&threads[8], &a, producerB, (void*) &consumer_ids[7]);
    pthread_create(&threads[9], &a, producerB, (void*) &consumer_ids[8]);
    pthread_create(&threads[10], &a, producerB, (void*) &consumer_ids[9]);
    
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(threads[2], NULL);
    pthread_join(threads[3], NULL);
    pthread_join(threads[4], NULL);
    pthread_join(threads[5], NULL);
    pthread_join(threads[6], NULL);
    pthread_join(threads[7], NULL);
    pthread_join(threads[8], NULL);
    pthread_join(threads[9], NULL);
    pthread_join(threads[10], NULL);


    return 0;

}