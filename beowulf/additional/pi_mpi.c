#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int rank, size, i;
    long long total_iters, my_iters;
    long long my_inside = 0, total_inside = 0;
    double x, y;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int len;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(hostname, &len);

    // 1. Get Iterations from Command Line (Passed by the Script)
    if (argc != 2) {
        if (rank == 0) printf("Usage: %s <iterations>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    
    // Convert string argument to long long
    total_iters = atoll(argv[1]);
    
    // Distribute work
    my_iters = total_iters / size;

    // Seed random number generator uniquely for each rank
    unsigned int seed = time(NULL) + rank;

    // 2. Monte Carlo Simulation
    for (i = 0; i < my_iters; i++) {
        // Generate random X and Y between 0 and 1
        x = (double)rand_r(&seed) / RAND_MAX;
        y = (double)rand_r(&seed) / RAND_MAX;

        if ((x * x + y * y) <= 1.0) {
            my_inside++;
        }
    }

    // 3. VISUAL LOGGING (For the Script to grep)
    // We create a temp file per rank so outputs don't get jumbled, 
    // or just rely on stdout handling.
    // Let's print to stdout but use a sleep to help ordering (optional but nice)
    usleep(rank * 1000); 
    printf("[Node: %s | Rank %d] Processed %lld iterations\n", hostname, rank, my_iters);

    // 4. Gather Results
    MPI_Reduce(&my_inside, &total_inside, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    // 5. Final Output (Formatted for the Script)
    if (rank == 0) {
        // Output format: CHUNK_RESULT=ITERS=INSIDE
        printf("CHUNK_RESULT=%lld=%lld\n", total_iters, total_inside);
    }

    MPI_Finalize();
    return 0;
}
