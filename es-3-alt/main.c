#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <math.h>

#define printf(...) printf(__VA_ARGS__); fflush(stdout)
#define thread_idx (abs((int) pthread_self()) % 100)

#define M 4
#define R 4
#define N 4

typedef int T;

struct busta_t
{
    T messaggio;
    struct busta_t* next;
};

struct mailbox_t // GESTORE
{
    sem_t s_mutex;

    int num_buste_vuote;
    int num_b_buste_vuote[3]; // numero di processi in attesa di buste vuote, per prioita'
    sem_t s_buste_vuote[3 /* priorita' */];

    sem_t s_buste_accodate;

    // buste accodate
    struct busta_t* head;
    struct busta_t* tail;

} g_mailbox;

void accoda_busta(struct mailbox_t* mailbox, struct busta_t* busta)
{
    if (mailbox->head == NULL || mailbox->tail == NULL)
    {
        mailbox->head = busta;
        mailbox->tail = busta;
    }
    else if (mailbox->head == mailbox->tail)
    {
        mailbox->tail = busta;
        mailbox->head->next = busta;
    }
    else
    {
        mailbox->tail->next = busta;
        mailbox->tail = busta;
    }
}

struct busta_t* disaccoda_busta(struct mailbox_t* mailbox)
{
    if (mailbox->head == NULL)
        return NULL;

    struct busta_t* result = mailbox->head;
    mailbox->head = mailbox->head->next; 
    return result;
}

void init_mailbox(struct mailbox_t* mailbox)
{
    sem_init(&mailbox->s_mutex, 0, 1);

    for (int priorita = 0; priorita < 3; priorita++) {
        sem_init(&mailbox->s_buste_vuote[priorita], 0, 0);
        g_mailbox.num_b_buste_vuote[priorita] = 0;
    }
    sem_init(&mailbox->s_buste_accodate, 0, 0);

    mailbox->num_buste_vuote = N;

    mailbox->head = NULL;
    mailbox->tail = NULL;
}

// ------------------------------------------------------------------------------------------------

void richiedi_busta_vuota(int priorita)
{
    sem_wait(&g_mailbox.s_mutex);

    if (g_mailbox.num_buste_vuote > 0) {
        // alla richiesta di una busta vuota, se presente, assegna la busta vuota al processo
        // richiedente indipendentemente dalla priorita'
        sem_post(&g_mailbox.s_buste_vuote[priorita]); // preemptive post
        g_mailbox.num_buste_vuote--;
    } else {
        g_mailbox.num_b_buste_vuote[priorita]++;
    }

    sem_post(&g_mailbox.s_mutex);

    sem_wait(&g_mailbox.s_buste_vuote[priorita]);
}

void riponi_busta_vuota()
{
    sem_wait(&g_mailbox.s_mutex);

    g_mailbox.num_buste_vuote++;
    for (int priorita = 0; priorita < 3; priorita++) {
        // in ordine di priorita', risveglia il primo processo che sta aspettando una busta vuota
        if (g_mailbox.num_b_buste_vuote[priorita] > 0) {
            sem_post(&g_mailbox.s_buste_vuote[priorita]);
            g_mailbox.num_b_buste_vuote[priorita]--;
            break;
        }
    }

    sem_post(&g_mailbox.s_mutex);
}

void send(T messaggio, int priorita)
{
    // aspetto una busta vuota
    //printf("%d send> Aspetto la consegna della busta vuota\n", thread_idx);
    richiedi_busta_vuota(priorita);

    // accodo la busta nella mailbox
    //printf("%d send> Accodo la busta nella mailbox\n", thread_idx);
    sem_wait(&g_mailbox.s_mutex);

    struct busta_t* busta = (struct busta_t*) malloc(sizeof(struct busta_t));
    busta->messaggio = messaggio;
    busta->next = NULL;
    accoda_busta(&g_mailbox, busta);

    sem_post(&g_mailbox.s_buste_accodate);

    sem_post(&g_mailbox.s_mutex);
}

T receive()
{
    // aspetta che ci sia un busta accodata
    //printf("%d> receive / Aspetto che ci sia una busta\n", thread_idx);
    sem_wait(&g_mailbox.s_buste_accodate);

    // la disaccoda in mutex
    //printf("%d> receive / Disaccodo la busta\n", thread_idx);
    sem_wait(&g_mailbox.s_mutex);
    
    struct busta_t* busta = disaccoda_busta(&g_mailbox);
    if (busta == NULL)
    {
        printf("Illegal state\n");
        exit(1);
    }

    sem_post(&g_mailbox.s_mutex);
    
    riponi_busta_vuota();

    // estrae il messaggio
    T messaggio = busta->messaggio;
    free(busta);

    return messaggio;
}

// ------------------------------------------------------------------------------------------------
// Threads
// ------------------------------------------------------------------------------------------------

void* ricevente(void* arg)
{
    while (true)
    {
        //printf("%d recv> Aspetto un messaggio...\n", thread_idx);

        T messaggio = receive();

        printf("%d recv> Ho ricevuto il messaggio \"%d\"\n", thread_idx, messaggio);

        sleep(rand() % 6);
    }
}

void* mittente(void* arg)
{
    while (true)
    {
        int messaggio = rand() % 100;
        int priorita = rand() % 3;

        //printf("%d send> Invio messaggio %d\n", thread_idx, messaggio);

        send(messaggio, priorita);

        printf("%d send> Messaggio \"%d\" inviato con priorita' \"%d\"!\n", thread_idx, messaggio, priorita);

        sleep(rand() % 2);
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    pthread_t t_mittenti[M];
    pthread_t t_riceventi[R];

    init_mailbox(&g_mailbox);

    for (int m = 0; m < M; m++)
        pthread_create(&t_mittenti[m], NULL, mittente, NULL);

    for (int r = 0; r < R; r++)
        pthread_create(&t_riceventi[r], NULL, ricevente, NULL);

    sleep(60);

    return 0;
}
