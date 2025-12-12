#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Defines how "heavy" the calculation is. 
// Increase this to make the CPU work harder without changing the result count.
#define COMPLEXITY 1000 

int main(int argc, char *argv[]) {
    int rank, size;
    long long chunk_size, i;
    int j;
    long long local_found = 0, global_found = 0;
    double val;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 1. READ CHUNK SIZE FROM SCRIPT
    if (argc > 1) {
        chunk_size = atoll(argv[1]); 
    } else {
        chunk_size = 1000000; 
    }

    // Determine workload per node
    long long items_per_node = chunk_size / size;
    
    // We use a "random" start offset based on the chunk size 
    // to simulate searching different parts of a blockchain
    // (This is just a dummy offset to make the math vary per chunk)
    long long start_offset = chunk_size * rank; 

    // 2. HEAVY MINING CALCULATION
    for (i = 0; i < items_per_node; i++) {
        
        // The "Block Header" we are hashing
        long long current_item = start_offset + i;
        
        // Perform a computationally expensive operation
        // This simulates the SHA-256 hashing in Bitcoin
        val = current_item;
        for(j = 0; j < COMPLEXITY; j++) {
            val = sin(val) * cos(val) + tan(sqrt(fabs(val)));
        }

        // 3. THE "DIFFICULTY" CHECK
        // We look for a specific mathematical property (The "Gold Nugget")
        // Check if the first decimal digit is '7' and second is '7' (e.g. 0.77...)
        double decimal_part = val - floor(val);
        if (decimal_part > 0.77 && decimal_part < 0.771) {
            local_found++;
        }
    }

    // 4. AGGREGATE "GOLD" FOUND
    MPI_Reduce(&local_found, &global_found, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    // 5. PRINT OUTPUT FOR BASH SCRIPT
    if (rank == 0) {
        printf("CHUNK_RESULT: %lld\n", global_found);
    }

    MPI_Finalize();
    return 0;
}
