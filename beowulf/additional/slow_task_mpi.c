#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * slow_task_mpi.c
 *
 * Fake workload used to demonstrate:
 *  - normal distributed execution
 *  - what happens if a node is disconnected mid-chunk
 *
 * Usage:
 *   mpirun -np P ... slow_task_mpi --chunk-id K --num-chunks M
 *
 * It just prints some info and sleeps a bit.
 */

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int chunk_id = 0;
    int num_chunks = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--chunk-id") == 0 && i + 1 < argc) {
            chunk_id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--num-chunks") == 0 && i + 1 < argc) {
            num_chunks = atoi(argv[++i]);
        }
    }

    if (rank == 0) {
        printf("[slow_task] chunk-id=%d / %d, world size=%d\n",
               chunk_id, num_chunks, size);
        fflush(stdout);
    }

    printf("[slow_task] rank=%d starting work on chunk %d\n", rank, chunk_id);
    fflush(stdout);

    /* Simulate some work: sleep more for higher chunk_id */
    int base_sleep = 5;          /* seconds */
    int extra      = chunk_id;   /* more delay for later chunks */

    sleep(base_sleep + extra);

    printf("[slow_task] rank=%d finished chunk %d\n", rank, chunk_id);
    fflush(stdout);

    MPI_Finalize();
    return 0;
}
