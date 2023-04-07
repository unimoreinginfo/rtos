#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define SHAVING_ITERATIONS 1000
#define PAYING_ITERATIONS  1000

#define NUM_CLI 10

struct gestore_t
{
    pthread_mutex_t mutex;
    pthread_cond_t divano;
    pthread_cond_t barbiere;
    pthread_cond_t cassiere;

    int n_cli_divano;
    int n_cli_barbiere;
    bool cassiere_libero;

} g_gestore;

void gestore_init(struct gestore_t* gestore)
{
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    pthread_mutexattr_init(&mutexattr);
    pthread_condattr_init(&condattr);

    pthread_mutex_init(&gestore->mutex, &mutexattr);
    pthread_cond_init(&gestore->divano, &condattr);
    pthread_cond_init(&gestore->barbiere, &condattr);
    pthread_cond_init(&gestore->cassiere, &condattr);

    gestore->n_cli_divano = 0;
    gestore->n_cli_barbiere = 0;
    gestore->cassiere_libero = true;

    pthread_condattr_destroy(&condattr);
    pthread_mutexattr_destroy(&mutexattr);
}

void* cliente(void* arg)
{
    int thread_id = *((int*) arg);
    int i;

    while (true)
    {
        // acquisisci divano
        pthread_mutex_lock(&g_gestore.mutex);

        while (g_gestore.n_cli_divano >= 4) {
            //printf("%d> Aspetto che il divano si liberi...\n", thread_id);
            pthread_cond_wait(&g_gestore.divano, &g_gestore.mutex);
        }

        //printf("%d> Divano liberato\n", thread_id);
        g_gestore.n_cli_divano++;

        pthread_mutex_unlock(&g_gestore.mutex);

        // acqusisci barbiere
        pthread_mutex_lock(&g_gestore.mutex);

        while (g_gestore.n_cli_barbiere >= 3) {
            //printf("%d> Aspetto che un barbiere si liberi...\n", thread_id);
            pthread_cond_wait(&g_gestore.barbiere, &g_gestore.mutex);
        }

        g_gestore.n_cli_divano--;
        pthread_cond_signal(&g_gestore.divano);
        g_gestore.n_cli_barbiere++;

        pthread_mutex_unlock(&g_gestore.mutex);

        // taglio la baraba
        //printf("%d> Mi taglio la barba...\n", thread_id);
        i = SHAVING_ITERATIONS;
        while (i-- > 0) {};

        // acquisci cassiere
        pthread_mutex_lock(&g_gestore.mutex);

        while (!g_gestore.cassiere_libero) {
            //printf("%d> Aspetto che il cassiere si liberi...\n", thread_id);
            pthread_cond_wait(&g_gestore.cassiere, &g_gestore.mutex);
        }

        g_gestore.cassiere_libero = false;
        g_gestore.n_cli_barbiere--;
        pthread_cond_signal(&g_gestore.barbiere);

        pthread_mutex_unlock(&g_gestore.mutex);;

        // pago
        //printf("%d> Cassiere liberato, pago...\n", thread_id);

        i = PAYING_ITERATIONS;
        while (i-- > 0) {};

        // libero il cassiere
        pthread_mutex_lock(&g_gestore.mutex);

        //printf("%d> Libero il cassiere...\n", thread_id);
        g_gestore.cassiere_libero = true;
        pthread_cond_signal(&g_gestore.cassiere);

        pthread_mutex_unlock(&g_gestore.mutex);

        printf("%d> FATTO!\n", thread_id); fflush(stdout);

        // e ricomincio...
        sleep(1);
    }
}



int main(int argc, char* argv[])
{
    pthread_t threads[NUM_CLI];
    int thread_ids[NUM_CLI];

    gestore_init(&g_gestore);

    for (int i = 0; i < NUM_CLI; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, cliente, &thread_ids[i]);
    }

    for (int i = 0; i < NUM_CLI; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
