#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

struct bandierine_t
{
    sem_t sg[2]; // Semafori per dare il via ai giocatori
    sem_t ag[2]; // Semafori per attendere i giocatori
    sem_t mutex;
    sem_t fine;

    int bandierina; // Chi ha preso la bandierina (0 o 1)
    int salvo; // Se il giocatore che ha preso la bandierina e' salvo o meno (true o false)
} bandierine;

void init_bandierine(struct bandierine_t* b)
{
    sem_init(&b->sg[0], 0, 0);
    sem_init(&b->sg[1], 0, 0);
    sem_init(&b->ag[0], 0, 0);
    sem_init(&b->ag[1], 0, 0);
    sem_init(&b->mutex, 0, 1);
    sem_init(&b->fine, 0, 0);

    b->bandierina = -1;
    b->salvo = -1;
}

void via(struct bandierine_t* b)
{
    sem_post(&b->sg[0]);
    sem_post(&b->sg[1]);
}

void attendi_il_via(struct bandierine_t* b, int n)
{
    sem_post(&b->ag[n]);
    sem_wait(&b->sg[n]);
}

int bandierina_presa(struct bandierine_t* b, int n)
{
    sem_wait(&b->mutex);

    int presa;
    if (b->bandierina < 0) {
        b->bandierina = n;
        presa = 1;
    } else {
        presa = 0;
    }

    sem_post(&b->mutex);

    return presa;
}

int sono_salvo(struct bandierine_t* b, int n)
{
    sem_wait(&b->mutex);

    int salvo = 0;
    if (b->salvo < 0) {
        b->salvo = 1;
        salvo = 1;
        sem_post(&b->fine);
    }

    sem_post(&b->mutex);

    return salvo;
}

int ti_ho_preso(struct bandierine_t* b, int n)
{
    sem_wait(&b->mutex);

    int preso = 0;
    if (b->salvo < 0) {
        b->salvo = 0;
        preso = 1;
        sem_post(&b->fine);
    }

    sem_post(&b->mutex);

    return preso;
}

int risultato_gioco(struct bandierine_t* b)
{
    sem_wait(&b->fine);

    int vincitore;
    sem_wait(&b->mutex);
    vincitore = b->salvo ? b->bandierina : (b->bandierina + 1) % 2;
    sem_post(&b->mutex);

    sem_post(&b->fine);

    return vincitore;
}

void attendi_giocatori(struct bandierine_t* b)
{
    sem_wait(&b->ag[0]);
    sem_wait(&b->ag[1]);
}

// ------------------------------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------------------------------

void* giocatore(void* arg)
{
    int numerogiocatore = *((int*) arg);
    attendi_il_via(&bandierine, numerogiocatore);
    if (bandierina_presa(&bandierine, numerogiocatore)) {
        if (sono_salvo(&bandierine, numerogiocatore)) printf("%d> salvo!\n", numerogiocatore);
    }
    else {
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
