#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define N 10

#define _printf(...) printf(__VA_ARGS__); fflush(stdout)

// Suppongo che ci sia:
// - un solo thread per cuoco
// - un solo thread per commesso
// - un solo thread per cliente

struct pasticceria_t
{
    pthread_mutex_t mutex;

    pthread_cond_t cuoco;
    pthread_cond_t commesso;
    pthread_cond_t richiesta;
    pthread_cond_t consegna;

    bool torta_richiesta;
    int num_torte_da_vendere;

} g_pasticceria;

void pasticceria_init(struct pasticceria_t* p)
{
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    pthread_mutexattr_init(&mutexattr);
    pthread_condattr_init(&condattr);

    pthread_mutex_init(&p->mutex, &mutexattr);
    pthread_cond_init(&p->cuoco, &condattr);
    pthread_cond_init(&p->commesso, &condattr);
    pthread_cond_init(&p->richiesta, &condattr);

    p->num_torte_da_vendere = 0;
    p->torta_richiesta = false;

    pthread_condattr_destroy(&condattr);
    pthread_mutexattr_destroy(&mutexattr);
}

void cuoco_inizio_torta(struct pasticceria_t* p)
{
    pthread_mutex_lock(&p->mutex);

    if (p->num_torte_da_vendere >= N)
        pthread_cond_wait(&p->cuoco, &p->mutex);

    pthread_mutex_unlock(&p->mutex);
}

void cuoco_fine_torta(struct pasticceria_t* p)
{
    pthread_mutex_lock(&p->mutex);
    
    p->num_torte_da_vendere++;
    pthread_cond_signal(&p->commesso);

    pthread_mutex_unlock(&p->mutex);
}

void commesso_prendo_torta(struct pasticceria_t* p)
{
    pthread_mutex_lock(&p->mutex);
    
    if (!p->torta_richiesta)
        pthread_cond_wait(&p->richiesta, &p->mutex);

    if (p->num_torte_da_vendere == 0)
        pthread_cond_wait(&p->commesso, &p->mutex);

    pthread_mutex_unlock(&p->mutex);
}

void commesso_vendo_torta(struct pasticceria_t* p)
{
    pthread_mutex_lock(&p->mutex);

    p->torta_richiesta = false;
    p->num_torte_da_vendere--;
    pthread_cond_signal(&p->consegna);
    pthread_cond_signal(&p->cuoco);

    pthread_mutex_unlock(&p->mutex);
}

void cliente_acquisto(struct pasticceria_t* p)
{
    pthread_mutex_lock(&p->mutex);

    p->torta_richiesta = true;
    pthread_cond_signal(&p->richiesta);
    pthread_cond_wait(&p->consegna, &p->mutex);

    pthread_mutex_unlock(&p->mutex);
}

// ------------------------------------------------------------------------------------------------

void* cuoco(void* arg)
{
    while (1)
    {
        cuoco_inizio_torta(&g_pasticceria);

        pthread_mutex_lock(&g_pasticceria.mutex);
        _printf("cuoco> Preparo la %d^ torta\n", g_pasticceria.num_torte_da_vendere + 1);
        pthread_mutex_unlock(&g_pasticceria.mutex);

        cuoco_fine_torta(&g_pasticceria);
        
        sleep(rand() % 2);
    }
}

void* commesso(void* arg)
{
    while (1)
    {
        commesso_prendo_torta(&g_pasticceria);

        _printf("commesso> Vendo la torta...\n");

        commesso_vendo_torta(&g_pasticceria);

        sleep(rand() % 2);
    }
}

void* un_cliente(void* arg)
{
    while (1)
    {
        _printf("cliente> Acquisto una torta...\n");

        cliente_acquisto(&g_pasticceria);

        _printf("cliente> Torta acquistata\n");

        sleep(rand() % 2);
    }
}

int main(int argc, char* argv[])
{
    pthread_t thread_cuoco;
    pthread_t thread_commesso;
    pthread_t thread_cliente;

    pasticceria_init(&g_pasticceria);

    pthread_create(&thread_cuoco, NULL, cuoco, NULL);
    pthread_create(&thread_commesso, NULL, commesso, NULL);
    pthread_create(&thread_cliente, NULL, un_cliente, NULL);

    pthread_join(thread_cuoco, NULL);
    pthread_join(thread_commesso, NULL);
    pthread_join(thread_cliente, NULL);

    return 0;
}
