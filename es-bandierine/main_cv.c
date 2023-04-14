#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>

struct bandierine_t
{
    pthread_mutex_t mutex;
    bool via;
    pthread_cond_t via_cv;
    bool pronto[2];
    pthread_cond_t attendi_gioc_cv;
    int bandierina;
    int salvo;
    pthread_cond_t fine_cv;

} bandierine;

void init_bandierine(struct bandierine_t* b)
{
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    pthread_mutexattr_init(&mutexattr);
    pthread_condattr_init(&condattr);

    pthread_mutex_init(&b->mutex, &mutexattr);
    b->via = false;
    pthread_cond_init(&b->via_cv, &condattr);
    b->pronto[0] = false;
    b->pronto[1] = false;
    pthread_cond_init(&b->attendi_gioc_cv, &condattr);
    b->bandierina = -1;
    b->salvo = -1;
    pthread_cond_init(&b->fine_cv, &condattr);

    pthread_condattr_destroy(&condattr);
    pthread_mutexattr_destroy(&mutexattr);
}

void via(struct bandierine_t* b)
{
    pthread_mutex_lock(&b->mutex);

    b->via = true;
    pthread_cond_broadcast(&b->via_cv);

    pthread_mutex_unlock(&b->mutex);
}

void attendi_il_via(struct bandierine_t* b, int n)
{
    pthread_mutex_lock(&b->mutex);

    b->pronto[n] = true;
    pthread_cond_broadcast(&b->attendi_gioc_cv);
    if (!b->via) {
        pthread_cond_wait(&b->via_cv, &b->mutex);
    }

    pthread_mutex_unlock(&b->mutex);
}

int bandierina_presa(struct bandierine_t* b, int n)
{
    pthread_mutex_lock(&b->mutex);

    int presa = 0;
    if (b->bandierina < 0) {
        b->bandierina = n;
        presa = 1;
    }

    pthread_mutex_unlock(&b->mutex);

    return presa;
}

int sono_salvo(struct bandierine_t* b, int n)
{
    pthread_mutex_lock(&b->mutex);

    int salvo = 0;
    if (b->salvo < 0) {
        b->salvo = 1;
        salvo = 1;
        pthread_cond_broadcast(&b->fine_cv);
    }

    pthread_mutex_unlock(&b->mutex);

    return salvo;
}

int ti_ho_preso(struct bandierine_t* b, int n)
{
    pthread_mutex_lock(&b->mutex);

    int preso = 0;
    if (b->salvo < 0) {
        b->salvo = 0;
        preso = 1;
        pthread_cond_broadcast(&b->fine_cv);
    }

    pthread_mutex_unlock(&b->mutex);

    return preso;
}

int risultato_gioco(struct bandierine_t* b)
{
    pthread_mutex_lock(&b->mutex);

    if (b->bandierina < 0 || b->salvo < 0) {
        pthread_cond_wait(&b->fine_cv, &b->mutex);
    }

    int vincitore =
        b->salvo ?
        b->bandierina : 
        (b->bandierina + 1) % 2;

    pthread_mutex_unlock(&b->mutex);

    return vincitore;
}

void attendi_giocatori(struct bandierine_t* b)
{
    pthread_mutex_lock(&b->mutex);

    while (!b->pronto[0] || !b->pronto[1]) {
        pthread_cond_wait(&b->attendi_gioc_cv, &b->mutex);
    }

    pthread_mutex_unlock(&b->mutex);
}

// ------------------------------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------------------------------

void* giocatore(void* arg)
{
    int numerogiocatore = *((int*) arg);
    attendi_il_via(&bandierine, numerogiocatore);
    if (bandierina_presa(&bandierine, numerogiocatore)) {
        usleep((rand() % 1000) * 1000);
        if (sono_salvo(&bandierine, numerogiocatore)) printf("%d> salvo!\n", numerogiocatore);
    }
    else {
        usleep((rand() % 1000) * 1000);
        if (ti_ho_preso(&bandierine, numerogiocatore)) printf("%d> ti ho preso!\n", numerogiocatore);
    }
    return 0;
}

void* giudice(void* arg)
{
    attendi_giocatori(&bandierine);
 
    printf("giudice> pronti..., attenti...\n");
    via(&bandierine);

    printf("giudice> il vincitore e': %d\n", risultato_gioco(&bandierine));
    
    return 0;
}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    pthread_t tgiocatore1, tgiocatore2, tgiudice;

    init_bandierine(&bandierine);

    int n1 = 0, n2 = 1;
    pthread_create(&tgiocatore1, NULL, giocatore, &n1);
    pthread_create(&tgiocatore2, NULL, giocatore, &n2);
    pthread_create(&tgiudice, NULL, giudice, NULL);

    pthread_join(tgiocatore1, NULL);
    pthread_join(tgiocatore2, NULL);
    pthread_join(tgiudice, NULL);

    return 0;
}
