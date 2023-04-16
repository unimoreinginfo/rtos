#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define P 30
#define E 5
#define N 2
#define M 2

struct coda_t;

struct palestra_t {
    int prenotazioni[P];
    pthread_mutex_t m;
    pthread_cond_t cv[P];
    int utenti[N];
    int l_coda[N];
    struct coda_t *coda[N];
};

void init_palestra(struct palestra_t *s) {
    pthread_mutexattr_t mattr;
    pthread_condattr_t cattr;

    pthread_mutexattr_init(&mattr);
    pthread_condattr_init(&cattr);

    for(int i = 0; i < P; i++) {
        s->prenotazioni[i] = -1;
        pthread_cond_init(s->cv+i, &cattr);
    }

    pthread_mutex_init(&s->m, &mattr);

    for(int i = 0; i < N; i++) {
        s->utenti[i] = 0;
        s->l_coda[i] = 0;
        s->coda[i] = NULL;
    }
}

struct coda_t {
    int p; // persona
    struct coda_t *next;
};

typedef struct coda_t coda_t;

void accoda_testa(struct palestra_t *p, int np, int ta); // def. dopo
void accoda(struct palestra_t *p, int np, int ta);
void disaccoda(struct palestra_t *p, int np, int ta);

void usaattrezzo(struct palestra_t *p, int numeropersona, int tipoattrezzo) {
    int risvegliato = 0;

    // non serve mutex per prenotazioni perché sono aree di mem. distinte
    if(p->prenotazioni[numeropersona] != tipoattrezzo) {
        printf("%d: nbookd esercizio ? con attrezzo %d\n", numeropersona, tipoattrezzo);
        pthread_mutex_lock(&p->m);
        while(p->utenti[tipoattrezzo] == M) {
            if(!risvegliato)
                accoda(p, numeropersona, tipoattrezzo);
            else
                accoda_testa(p, numeropersona, tipoattrezzo);
            printf("%d: blking esercizio ? con attrezzo %d\n", numeropersona, tipoattrezzo);
            pthread_cond_wait(&p->cv[numeropersona], &p->m);

            disaccoda(p, numeropersona, tipoattrezzo);
            risvegliato = 1;
        }
        p->utenti[tipoattrezzo]++;
        pthread_mutex_unlock(&p->m);
    } else {
        printf("%d: booked esercizio ? con attrezzo %d\n", numeropersona, tipoattrezzo);
        p->prenotazioni[numeropersona] = -1;
    }
}

void accoda_testa(struct palestra_t *p, int np, int ta) {
    coda_t *coda = malloc(sizeof(coda_t));

    p->l_coda[ta]++;
    coda->p = np;
    coda->next = p->coda[ta];

    p->coda[ta] = coda;
}

void accoda(struct palestra_t *p, int np, int ta) {
    p->l_coda[ta]++;

    if(p->coda[ta] == NULL) {
        p->coda[ta] = calloc(1, sizeof(coda_t));
        p->coda[ta]->p = np;
        return;
    }

    coda_t *coda = p->coda[ta];
    while(coda->next != NULL) {
        coda = coda->next;
    }
    coda->next = calloc(1, sizeof(coda_t));
    coda->next->p = np;
}

void disaccoda(struct palestra_t *p, int np, int ta) {
    p->l_coda[ta]--;
    coda_t *n = p->coda[ta]->next;
    free(p->coda[ta]);
    p->coda[ta] = n;
}

void prenota(struct palestra_t *p, int numeropersona, int tipoattrezzo) {
    pthread_mutex_lock(&p->m);
    if(p->utenti[tipoattrezzo] == M) {
        pthread_mutex_unlock(&p->m);
        return;
    }

    p->utenti[tipoattrezzo]++;
    p->prenotazioni[numeropersona] = tipoattrezzo;

    pthread_mutex_unlock(&p->m);
}

void fineuso(struct palestra_t *p, int numeropersona, int tipoattrezzo) {
    printf("%d: fineus esercizio ? con attrezzo %d\n", numeropersona, tipoattrezzo);
    pthread_mutex_lock(&p->m);
    p->utenti[tipoattrezzo]--;
    if(p->l_coda[tipoattrezzo]) {
        printf("%d: wakeup utente    %d con attrezzo %d\n", numeropersona, p->coda[tipoattrezzo]->p, tipoattrezzo);
        pthread_cond_signal(&p->cv[p->coda[tipoattrezzo]->p]);
    }

    pthread_mutex_unlock(&p->m);
}

struct palestra_t palestra;

void pausetta(void) {
    struct timespec t;
    t.tv_nsec = (rand()%10+1)*1000000;
    t.tv_sec = 0;
    nanosleep(&t, NULL);
}

void *persona(void *arg) {
    pausetta();
    // si prende direttamente il valore
    int numeropersona = *(int *) arg;
    int attrezzocorrente = rand()%N;
    int prossimoattrezzo = rand()%N;

    for(int i = E; i > 0; i--) {
        printf("%d: inizio esercizio %d con attrezzo %d\n", numeropersona, i, attrezzocorrente);
        usaattrezzo(&palestra, numeropersona, attrezzocorrente);

        if(i != 1){
            printf("%d: prenot esercizio %d con attrezzo %d\n", numeropersona, i, prossimoattrezzo);
            prenota(&palestra, numeropersona, prossimoattrezzo);
            printf("%d: endpre esercizio %d con attrezzo %d\n", numeropersona, i, prossimoattrezzo);
        }
        fineuso(&palestra, numeropersona, attrezzocorrente);

        printf("%d: fine   esercizio %d con attrezzo %d\n", numeropersona, i, attrezzocorrente);

        if(i != 1) {
            attrezzocorrente = prossimoattrezzo;
            prossimoattrezzo = rand()%N;
        }
    }
}

int main(void) {
    pthread_t users[P];
    int indici[P];
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    init_palestra(&palestra);

    for(int i = 0; i < P; i++) {
        indici[i] = i;
        pthread_create(users+i, &attr, persona, (void *) (indici+i));
    }

    for(int i = 0; i < P; i++) {
        pthread_join(users[i], NULL);

    }

    return 0;
}