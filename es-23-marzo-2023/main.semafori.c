/*
    ven 24 marzo 2023

    siano a, b, c, d, e le procedure che un insieme di processi (p1, p2, ...,
   pn) possono invocare e che devono essere eseguite rispettando i seguenti
   vincoli di sincronizzazione:
        - sono possibili solo due sequenze di esecuzioni delle procedure, tra di
   loro mutualmente esclusive
        - la prima sequenza prevede che venga eseguita per prima la procedura A,
   a cui può seguire esclusivamente l'esecuzione di una o più attivazioni
   concorrenti della procedura B
        - la seconda sequenza è costituita dall'esecuzione della procedura C, a
   cui può seguire esclusivamente l'esecuzione della proceudra D, o in
   alternativa a D della prodedura E.

    una volta terminata una delle due sequenze, una nuova sequenza può essere di
   nuovo iniziata. utilizzando il meccanismo dei semafori, realizzare le
   funzioni startA, endA, startB, endB, ..., startE, endE che, invocate dai
   processi client P1, ..., Pn rispettivamente prima e dopo le corrispondenti
   procedure, garantiscono il rispetto dei precedenti vincoli. Nel risolvere il
   problema non è richiesta la soluzione di eventuali problemi di starvation

   questa soluzione viene implementata mediante l'utilizzo di FSM

    stato iniziale: in questo stato è possibile chiamare o A o C
        - se A viene chiamata è possibile chiamare solo B (stato A)
            - quando viene chiamata B, è possibile chiamare solo B oppure C
        - se C viene chiamata è possibile chiamare solo o D o E
            - quando vengono chiamate D o E, è possibile solo tornare allo stato
              "iniziale"

    si utilizzano semafori privati per distinguere in modo univoco chi deve
   essere svegliato.
   quando si è allo stato iniziale non è specificato quale procedura svegliare,
   pertanto è possibile utilizzare un solo semaforo (anche se nella traccia non
   lo si fa) su cui A e C si bloccheranno. qualora sia necessaria una
   prioritizzazione di routine, allora sarà necessario utilizzare tanti semafori
   privati quante sono le cose da prioritizzare
*/

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

typedef enum { none, a, b, c, d, e } state_t;
typedef struct {
  sem_t mutex;
  sem_t sa, sb, sc, sd, se; // ogni semaforo privato per ciascuna procedura (si
                            // ignora quanto detto prima, per qualche motivo)
  int ca, cb, cc, cd, ce; // counter per ogni procedura: misurano quanti thread
                          // sono bloccati su tale procedura
  int nb; // questo counter indica quante procedure "B" sono in esecuzione,
          // poiché queste sono le uniche che possono essere chiamate
          // concorrentemente
  state_t state; // traccio lo stato del programma
} manager;

void init_manager(manager *m) {
  sem_init(&m->mutex, 0, 1);

  sem_init(&m->sa, 0, 0);
  sem_init(&m->sb, 0, 0);
  sem_init(&m->sc, 0, 0);
  sem_init(&m->sd, 0, 0);
  sem_init(&m->se, 0, 0);

  m->ca = m->cb = m->cc = m->cd = m->ce = 0;
  m->state = none;
}

void wake_a_or_c_up(manager *m) {
  if (m->ca > 0) {
    m->state = a; // ricordarsi di settare sempre lo stato!! il thread
                  // bloccato si aspetta che sia già settato!
    m->ca--;
    sem_post(&m->sa);
  } else if (m->cc > 0) {
    m->state = c; // ricordarsi di settare sempre lo stato!! il thread
                  // bloccato si aspetta che sia già settato!
    m->cc--;
    sem_post(&m->sc);
  } else {
    m->state = none; // se non c'è nessuno, allora metto "none", in modo tale
                     // che così mi metto in attesa di eventuali A o C
  }
}

void startA(manager *m) {
  sem_wait(&m->mutex);
  if (m->state == none) {
    // se la sequenza è quella iniziale, allora non mi blocco e faccio una
    // preventive post, in modo da non bloccarmi alla sem_wait successiva
    m->state = a;
    sem_post(&m->sa);
  } else {
    // se la sequenza non è quella iniziale, allora mi blocco semplicemente
    m->ca++;
  }
  sem_post(&m->mutex);
  sem_wait(&m->sa);
}
void endA(manager *m) {
  sem_wait(&m->mutex);
  // la responsabilità di questa funzione è quella di svegliare semplicemente i
  // thread B
  m->state = b; // quando finisce A (e solo quando finisce A), l'unica funzione
                // che può partire è B
  while (m->cb-- > 0) { // decrementa la cb nel while (grana syntax)
    m->nb++;            // ci possono essere quante B vogliamo (in parallelo!)
    sem_post(&m->sb);
  }
  sem_post(&m->mutex);
}

void startB(manager *m) {
  // in questa funzione mi basta semplicemente incrementare cb se mi blocco,
  // poiché è la endA che si occupa di settare lo stato a B quando A termina!
  sem_wait(&m->mutex);
  if (m->state == b) {
    m->nb++;
    sem_post(&m->sb);
  } else {
    m->cb++;
  }
  sem_post(&m->mutex);
  sem_wait(&m->cb);
}

void endB(manager *m) {
  sem_wait(&m->mutex);
  // basta aspettare semplicemente che tutti i b finiscano. questo lo posso fare
  // decrementando cb ad ogni "endB" e, successivamente, se cb è 0 (ovvero è
  // appena passato l'ultimo b), allora posso svegliare o a o c (senza controllo
  // di starvation, quindi posso scegliere l'ordine indipendentemente)
  // ovviamente, se scelgo di far partire a o c devo settare lo stato
  // corrispondente, perché il thread che sta aspettando di essere svegliato non
  // ha lo stato assegnato!
  m->nb--;

  if (m->nb == 0) { // se sono l'ultimo b...
    wake_a_or_c_up(m);
  }

  sem_post(&m->mutex);
}

void startC(manager *m) {

  sem_wait(&m->mutex);
  // la procedura è analoga a startA, poiché hanno la stessa logica
  if (m->state == none) {
    m->state = c;
    sem_post(&m->sc);
  } else {
    m->cc++;
  }

  sem_post(&m->mutex);
  sem_wait(&m->sc);
}

void endC(manager *m) {

  sem_wait(&m->mutex);
  // in questo caso posso far partire solo D o E, anche qui non c'è nessuna
  // prevenzione di starvation, quindi no worries
  // si noti come posso non gestire l'else "catch-all", nel senso che se non ci
  // sono thread bloccati su C o su E, è inutile stare ad usare un nuovo stato,
  // poiché baste
  m->cc--;
  if (m->cd) {
    m->state = d;
    m->cd--;
    sem_post(&m->sd);
  } else if (m->ce) {
    m->state = e;
    m->ce--;
    sem_post(&m->se);
  }
  sem_post(&m->mutex);
}

void startD(manager *m) {

  sem_wait(&m->mutex);
  // analoga a sopra
  if (m->state == c) {
    m->state = d;
    sem_post(&m->sd);
  } else {
    m->cd++;
  }

  sem_post(&m->mutex);
  sem_wait(&m->sd);
}

void endD(manager *m) {
  sem_wait(&m->mutex);
  wake_a_or_c_up(m); // molto semplicemente devo sbloccare a o c, poiché non
                     // posso fare partire più D consecutivamente
  sem_post(&m->mutex);
}

void startE(manager *m) {

  sem_wait(&m->mutex);
  // analoga a sopra
  if (m->state == c) {
    m->state = e;
    sem_post(&m->se);
  } else {
    m->ce++;
  }

  sem_post(&m->mutex);
  sem_wait(&m->se);
}

void endE(manager *m) {
  sem_wait(&m->mutex);
  wake_a_or_c_up(m); // molto semplicemente devo sbloccare a o c, poiché non
                     // posso fare partire più D consecutivamente
  sem_post(&m->mutex);
}

int main(void) {}
