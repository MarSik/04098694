#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <assert.h>

int THREAD_BATCH = 100;
int MAX_THREADS = 20000;
int THREADS_PER_MUTEX = 20;
int LOOP_COUNT = 1000;
int THREAD_SLEEP_US = 1000; // 1ms
int MAX_MUTEXES;
int OPERATIONS = 0;
int CPU_PIN_FIRST_CPU = -1;

#define MUTEX_COUNT (MAX_THREADS / THREADS_PER_MUTEX)

pthread_spinlock_t spinlocks[20000];
pthread_spinlock_t mt_count;

int ops = 0;

// Dummy logging function
void timestamp_log(const char *fmt, int val) {
    printf(fmt, val);
}

void* thread_func(void* arg) {
    int index = *(int *)arg;
    free(arg);

    // Pin thread to a specific CPU core
    if (CPU_PIN_FIRST_CPU >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        int cpu_id =
            CPU_PIN_FIRST_CPU +
            (index % (sysconf(_SC_NPROCESSORS_ONLN) - CPU_PIN_FIRST_CPU));
        CPU_SET(cpu_id, &cpuset);
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
            fprintf(stderr, "Warning: Could not set CPU affinity for thread %d: %s\n", index, strerror(errno));
        } else {
            printf("Thread %d pinned to cpu %d\n", index, cpu_id);
        }
    }

    pthread_spinlock_t *lock = &spinlocks[index / THREADS_PER_MUTEX];

    while (1) {
        pthread_spin_lock(lock);

        volatile int temp = 0;
        for (int i = 0; i < LOOP_COUNT; i++) {
            temp += i;
        }

        pthread_spin_unlock(lock);

        if (OPERATIONS > 0) {
            pthread_spin_lock(&mt_count);
            if (ops >= OPERATIONS) {
                timestamp_log("operation finished - %d\n", ops);
                exit(0);  // Exit inside the lock as requested
		pthread_spin_unlock(&mt_count);
            }
            ops++;
            pthread_spin_unlock(&mt_count);
        }

        usleep(THREAD_SLEEP_US);
    }

    return NULL;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -m <MAX_THREADS>                 Total number of threads (default: 20000)\n");
    printf("  -t <THREADS_PER_MUTEX>           Number of threads per mutex (default: 20)\n");
    printf("  -l <LOOP_COUNT>                  Number of CPU loop iterations inside thread (default: 1000)\n");
    printf("  -d <THREAD_SLEEP_US>             Sleep time inside thread function in microseconds (default: 1000)\n");
    printf("  -o <OPERATIONS>                  How much operation will perform per batch(default: unlimit)\n");
    printf("  -p <FIRST_CPU>                   Use cpu pinning starting with first cpu (default: disabled).\n");
    printf("  -h                               Show this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "i:b:m:t:l:d:o:p:h")) != -1) {
        switch (opt) {
            case 'b':
                THREAD_BATCH = atoi(optarg);
                break;
            case 'm':
                MAX_THREADS = atoi(optarg);
                break;
            case 't':
                THREADS_PER_MUTEX = atoi(optarg);
                break;
            case 'l':
                LOOP_COUNT = atoi(optarg);
                break;
            case 'd':
                THREAD_SLEEP_US = atoi(optarg);
                break;
            case 'o':
                OPERATIONS = atoi(optarg);
                break;
            case 'p':
                CPU_PIN_FIRST_CPU = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }

    assert(MUTEX_COUNT < 20000);

    pthread_t threads[MAX_THREADS];

    // Initialize spinlocks
    for (int i = 0; i < MUTEX_COUNT; i++) {
        if (pthread_spin_init(&spinlocks[i], PTHREAD_PROCESS_PRIVATE) != 0) {
            perror("spinlock init failed");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_spin_init(&mt_count, PTHREAD_PROCESS_PRIVATE) != 0) {
        perror("spinlock init failed");
        exit(EXIT_FAILURE);
    }

    // Launch threads
    for (int i = 0; i < MAX_THREADS; i++) {
        int *arg = malloc(sizeof(int));
        if (!arg) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        *arg = i;
        if (pthread_create(&threads[i], NULL, thread_func, arg) != 0) {
            perror("thread create failed");
            exit(EXIT_FAILURE);
        }
    }

    // Join threads
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destroy spinlocks
    for (int i = 0; i < MUTEX_COUNT; i++) {
        pthread_spin_destroy(&spinlocks[i]);
    }
    pthread_spin_destroy(&mt_count);

    return 0;
}
