#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define MAX_WORKERS 128

typedef struct {
    long global_counter;
    long counters[MAX_WORKERS];
} shared_t;

static int sem_op(int semid, int semnum, int op) {
    struct sembuf sb = { .sem_num = semnum, .sem_op = op, .sem_flg = 0 };
    return semop(semid, &sb, 1);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_workers> <num_increments_per_worker>\n", argv[0]);
        return 1;
    }

    int num_workers = atoi(argv[1]);
    long num_iters = atol(argv[2]);

    if (num_workers <= 0 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "num_workers must be 1..%d\n", MAX_WORKERS);
        return 1;
    }
    if (num_iters < 0) {
        fprintf(stderr, "num_increments_per_worker must be >= 0\n");
        return 1;
    }

    int shmid = shmget(IPC_PRIVATE, sizeof(shared_t), IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shmget failed");
        return 1;
    }

    shared_t *shared = (shared_t *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat failed");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    shared->global_counter = 0;
    for (int i = 0; i < MAX_WORKERS; i++) shared->counters[i] = 0;

    int semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semid == -1) {
        perror("semget failed");
        shmdt(shared);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    if (semctl(semid, 0, SETVAL, 1) == -1) {
        perror("semctl SETVAL failed");
        shmdt(shared);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);
        return 1;
    }

    pid_t pids[MAX_WORKERS];

    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            for (int k = 0; k < i; k++) kill(pids[k], SIGTERM);
            shmdt(shared);
            shmctl(shmid, IPC_RMID, NULL);
            semctl(semid, 0, IPC_RMID);
            return 1;
        }

        if (pid == 0) {
            for (long j = 0; j < num_iters; j++) {
                if (sem_op(semid, 0, -1) == -1) {
                    perror("sem_op P failed in child");
                    _exit(1);
                }
                shared->global_counter++;
                shared->counters[i]++;
                if (sem_op(semid, 0, 1) == -1) {
                    perror("sem_op V failed in child");
                    _exit(1);
                }
            }
            _exit(0);
        }

        pids[i] = pid;
    }

    for (int i = 0; i < num_workers; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    printf("Master: global_counter = %ld (expected %ld)\n",
           shared->global_counter, (long)num_workers * num_iters);

    long sum = 0;
    for (int i = 0; i < num_workers; i++) {
        printf(" worker %2d counter = %12ld\n", i, shared->counters[i]);
        sum += shared->counters[i];
    }
    printf("Sum of per-worker counters = %ld\n", sum);

    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    return 0;
}
