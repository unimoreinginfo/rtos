
#include <time.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>


#define my_printf(x, ...) printf(x, ##__VA_ARGS__); fflush(stdout)
#define do_something() usleep(rand() % 1000 + 1000)


struct handler_t {
    sem_t mutex;

    sem_t
        sa,
        sb,
        sr;
    
    int
        ca, // num of A threads blocked
        cb, // num of B threads blocked
        cr; // num of R threads blocked

    int
        na, // num of A threads running
        nb, // num of B threads running
        nr; // num of R threads running

} g_handler;


void handler_init(struct handler_t* handler)
{
    // mutex
    sem_init(&handler->mutex, 0, 1);

    // private semaphores
    sem_init(&handler->sa, 0, 0);
    sem_init(&handler->sb, 0, 0);
    sem_init(&handler->sr, 0, 0);

    // handler vars
    handler->ca = 0;
    handler->cb = 0;
    handler->cr = 0;

    handler->na = 0;
    handler->nb = 0;
    handler->nr = 0;
}

void handler_print(struct handler_t* handler)
{
    // nc'ho voggj
    //my_printf("cab=%d, cr=%d, nab=%d, nr=%d\n", handler->cab, handler->cr, handler->nab, handler->nr);
}

// ------------------------------------------------------------------------------------------------

void StartProcA()
{
    sem_wait(&g_handler.mutex);

    // WHEN ProcA CAN RUN?
    
    if (g_handler.nr == 0 && g_handler.na == 0) {
        // ProcA can run only if neither Reset nor ProcA threads are running
        sem_post(&g_handler.sa);
        g_handler.na++;
    } else {
        // Otherwise blocks the thread
        g_handler.ca++;
    }

    sem_post(&g_handler.mutex);

    sem_wait(&g_handler.sa);
}

void EndProcA()
{
    sem_wait(&g_handler.mutex);

    // ProcA is no more running, so we can decrement the running count
    g_handler.na--;

    if (g_handler.na == 0) {
        if (g_handler.nb == 0 && g_handler.cr > 0) {
            // Wakes up any blocked Reset thread _only if_ there's no B thread running
            g_handler.cr--;
            g_handler.nr++;
            sem_post(&g_handler.sr);
        } else if (g_handler.ca > 0) {
            g_handler.ca--;
            g_handler.na++;
            sem_post(&g_handler.sa);
        }
    }

    sem_post(&g_handler.mutex);
}

void StartProcB() // The same as StartProcA
{
    sem_wait(&g_handler.mutex);

    if (g_handler.nr == 0 && g_handler.nb == 0) {
        sem_post(&g_handler.sb);
        g_handler.nb++;
    } else {
        g_handler.cb++;
    }

    sem_post(&g_handler.mutex);

    sem_wait(&g_handler.sb);
}

void EndProcB() // The same as StartProcB
{
    sem_wait(&g_handler.mutex);

    g_handler.nb--;

    if (g_handler.nb == 0) {
        if (g_handler.na == 0 && g_handler.cr > 0) {
            // Wakes up any blocked Reset thread _only if_ there's no A thread running
            g_handler.cr--;
            g_handler.nr++;
            sem_post(&g_handler.sr);
        } else if (g_handler.cb > 0) {
            g_handler.cb--;
            g_handler.nb++;
            sem_post(&g_handler.sb);
        }
    }

    sem_post(&g_handler.mutex);
}

void StartReset()
{
    sem_wait(&g_handler.mutex);

    if (g_handler.na == 0 && g_handler.nb == 0 && g_handler.nr == 0) {
        // Reset can run only if there's no ProcA, ProcB, Reset thread running
        sem_post(&g_handler.sr);
        g_handler.nr++;
    } else {
        // Otherwise the thread is blocked (similar to above)
        g_handler.cr++;
    }

    sem_post(&g_handler.mutex);

    sem_wait(&g_handler.sr);
}

void EndReset()
{
    sem_wait(&g_handler.mutex);

    g_handler.nr--;
    
    if (g_handler.nr == 0) {
        if (g_handler.cr > 0) {
            // Gives the priority to blocked Reset threads
            g_handler.cr--;
            g_handler.nr++;
            sem_post(&g_handler.sr);
        } else {
            // Otherwise wakes up both blocked A and B threads
            if (g_handler.ca > 0) {
                g_handler.ca--;
                g_handler.na++;
                sem_post(&g_handler.sa);
            }
            if (g_handler.cb > 0) {
                g_handler.cb--;
                g_handler.nb++;
                sem_post(&g_handler.sb);
            }
        }
    }

    sem_post(&g_handler.mutex);
}

// ------------------------------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------------------------------

void ProcA(int thread_idx)
{
    StartProcA();

    my_printf("<%dA", thread_idx); do_something(); my_printf("A%d>", thread_idx);

    EndProcA();
}

void ProcB(int thread_idx)
{
    StartProcB();

    my_printf("<%dB", thread_idx); do_something(); my_printf("B%d>", thread_idx);

    EndProcB();
}

void Reset(int thread_idx)
{
    StartReset();

    my_printf("<%dR", thread_idx); do_something(); my_printf("R%d>", thread_idx);

    EndReset();
}

void* thread_body(void* arg)
{
    int thread_idx = *((int*) arg);
    while (1)
    {
        int r = rand();

        //my_printf("|%d->%s|", thread_idx, (r%3 ==0  ? "A" : (r%3==1 ? "B" : "R")));

        if (r % 3 == 0) ProcA(thread_idx);
        else if (r % 3 == 1) ProcB(thread_idx);
        else if (r % 3 == 2) Reset(thread_idx);
    }
}


int main(int argc, char* argv[])
{
    srand(time(NULL));

    const int k_num_threads = 10;
    pthread_t my_threads[k_num_threads];

    handler_init(&g_handler);

    // stores threads id in an array for debugging
    int thread_ids[k_num_threads];
    for (int i = 0; i < k_num_threads; i++)
        thread_ids[i] = i;

    // creates and starts threads
    for (int i = 0; i < k_num_threads; i++)
    {
        if (pthread_create(&my_threads[i], NULL, thread_body, (void*) &thread_ids[i]) != 0)
        {
            perror("pthread_create() error\n");
            return 1;
        }
    }

    sleep(60);

    return 0;
}
