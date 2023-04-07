#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define N 10

#define _printf(...) printf(__VA_ARGS__); fflush(stdout)

struct pasticceria_t
{
    sem_t prepara_torte; // 10
    sem_t torte_pronte;
    sem_t richieste;
    sem_t consegne;

} g_pasticceria;

void pasticceria_init(struct pasticceria_t* p)
{
    sem_init(&p->prepara_torte, 0, N);
    sem_init(&p->torte_pronte, 0, 0);
    sem_init(&p->richieste, 0, 0);
    sem_init(&p->consegne, 0, 0);
}

void cuoco_inizio_torta(struct pasticceria_t* p)
{
    sem_wait(&p->prepara_torte);

}

void cuoco_fine_torta(struct pasticceria_t* p)
{
    sem_post(&p->torte_pronte);
}

void commesso_prendo_torta(struct pasticceria_t* p)
{
    sem_wait(&p->richieste);
    sem_wait(&p->torte_pronte);
}

void commesso_vendo_torta(struct pasticceria_t* p)
{
    sem_post(&p->consegne);
    sem_post(&p->prepara_torte);
}

void cliente_acquisto(struct pasticceria_t* p)
{
    sem_post(&p->richieste);
    sem_wait(&p->consegne);
}

// ------------------------------------------------------------------------------------------------

void* cuoco(void* arg)
{
    while (1)
    {
        cuoco_inizio_torta(&g_pasticceria);

        int torte_da_preparare;
        sem_getvalue(&g_pasticceria.prepara_torte, &torte_da_preparare); // May not be thread-safe
        _printf("cuoco> Ho preparato la %d^ torta\n", N - torte_da_preparare);

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
