
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
        sab,
        sr;
    
    int
        cab, // num of A/B threads blocked
        cr; // num of R threads blocked

    int
        nab, // num of A/B threads running
        nr;

} g_handler;


void handler_init(struct handler_t* handler)
{
    // mutex
    sem_init(&handler->mutex, 0, 1);

    // private semaphores
    sem_init(&handler->sab, 0, 0);
    sem_init(&handler->sr, 0, 0);

    // handler vars
    handler->cab = 0;
    handler->cr = 0;

    handler->nab = 0;
    handler->nr = 0;
}

void handler_print(struct handler_t* handler)
{
    my_printf("cab=%d, cr=%d, nab=%d, nr=%d\n", handler->cab, handler->cr, handler->nab, handler->nr);
}

// ------------------------------------------------------------------------------------------------

void StartProcAOrB()
{
    sem_wait(&g_handler.mutex);

    if (g_handler.nr == 0) {
        // if Reset is not running, then we can start (preemptive post)
        sem_post(&g_handler.sab);
        g_handler.nab++;
    } else {
        // otherwise blocks until Reset doesn't wake me up
        g_handler.cab++;
    }

    sem_post(&g_handler.mutex);

    sem_wait(&g_handler.sab);
}

void EndProcAOrB()
{
    sem_wait(&g_handler.mutex);

    g_handler.nab--;

    if (g_handler.nab == 0) {
        // does something only when there isn't any other a/b running

        if (g_handler.cr > 0) {
            // gives the priority to the Reset thread
            g_handler.cr--;
            g_handler.nr++;
            sem_post(&g_handler.sr);
        } else if (g_handler.cab > 0) {
            // if no Reset thread is blocked, wakes up A or B
            g_handler.cab--;
            g_handler.nab++;
            sem_post(&g_handler.sab);
        }
    }



    sem_post(&g_handler.mutex);
}

void StartReset()
{
    sem_wait(&g_handler.mutex);

    if (g_handler.nab == 0 && g_handler.nr == 0) {
        // if a, b and Reset itself are not running then Reset can run (preemptive post)
        sem_post(&g_handler.sr);
        g_handler.nr++;
    } else {
        // otherwise blocks the thread
        g_handler.cr++;
    }

    sem_post(&g_handler.mutex);

    sem_wait(&g_handler.sr);
}

void EndReset()
{
    sem_wait(&g_handler.mutex);

    g_handler.nr--;
    
    if (g_handler.cr > 0) {
        // gives the priority to the Reset thread
        g_handler.cr--;
        g_handler.nr++;
        sem_post(&g_handler.sr);
    } else if (g_handler.cab > 0) {
        // if no Reset thread is blocked, wakes up A or B
        g_handler.cab--;
        g_handler.nab++;
        sem_post(&g_handler.sab);
    }

    sem_post(&g_handler.mutex);
}

// ------------------------------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------------------------------

void ProcA(int thread_idx)
{
    StartProcAOrB();

    my_printf("<%dA", thread_idx); do_something(); my_printf("A%d>", thread_idx);

    EndProcAOrB();
}

void ProcB(int thread_idx)
{
    StartProcAOrB();

    my_printf("<%dB", thread_idx); do_something(); my_printf("B%d>", thread_idx);

    EndProcAOrB();
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

    // stores thread ids in an array for debugging
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
