#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

// Complexity simulator: Increase this to make the CPU work harder per hash
#define DIFFICULTY_LOOP 500 

int main(int argc, char *argv[]) {
    int rank, size;
    long long total_iters, my_iters, i;
    long long local_found = 0, global_found = 0;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int len;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(hostname, &len);

    // 1. Read Workload Size from Script
    if (argc > 1) {
        total_iters = atoll(argv[1]);
    } else {
        total_iters = 1000000;
    }

    // Distribute work
    my_iters = total_iters / size;
    
    // Seed random based on rank so every node calculates different numbers
    srand(rank + total_iters); 

    // 2. The Heavy Lifting (Mining Loop)
    for (i = 0; i < my_iters; i++) {
        // Simulate Hashing: Complex math operations
        double val = (double)i;
        for (int k = 0; k < DIFFICULTY_LOOP; k++) {
            val = sin(val) * cos(val) + tan(sqrt(fabs(val)));
        }

        // 3. Check if we found a "Rare Block" (Gold Nugget)
        // We look for a specific decimal pattern. Probability approx 1 in 1000
        double decimal = val - floor(val);
        if (decimal > 0.9990) {
            local_found++;
        }
    }

    // 4. Print Node Status (For the script to grep)
    // The script looks for lines starting with "[Node:"
    printf("[Node: %s] Rank %d mined %lld hashes | Found %lld Gold Nuggets\n", 
           hostname, rank, my_iters, local_found);

    // 5. Aggregate Results
    MPI_Reduce(&local_found, &global_found, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    // 6. Print Final Chunk Result (For the script to parse)
    if (rank == 0) {
        // Format matches script regex: "found" maps to the script's "inside" logic
        printf("CHUNK_RESULT: iters=%lld found=%lld\n", total_iters, global_found);
    }

    MPI_Finalize();
    return 0;
}
