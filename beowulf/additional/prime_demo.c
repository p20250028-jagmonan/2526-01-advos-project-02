#include <mpi.h>
#include <stdio.h>
#include <math.h>

#define LIMIT 10000000000  // Checking up to 10 Million

int isPrime(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (int i = 5; i * i <= n; i = i + 6)
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    return 1;
}

int main(int argc, char *argv[]) {
    int rank, size;
    int local_count = 0;
    int global_count = 0;
    double start_time, end_time;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int name_len;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(hostname, &name_len);

    // Master starts timer
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        printf("\n=== PRIME NUMBER HUNT (Block Distribution) ===\n");
        printf("Searching for primes up to %d on %d processors...\n", LIMIT, size);
        start_time = MPI_Wtime();
    }

    // --- BLOCK DISTRIBUTION LOGIC ---
    // Calculate the range for each process
    int items_per_proc = LIMIT / size;
    int remainder = LIMIT % size;
    
    int start_num, end_num;

    // Distribute the remainder so no one gets left out
    if (rank < remainder) {
        start_num = rank * (items_per_proc + 1) + 1;
        end_num = start_num + items_per_proc;
    } else {
        start_num = rank * items_per_proc + remainder + 1;
        end_num = start_num + (items_per_proc - 1);
    }

    // Every rank checks its own distinct RANGE of numbers
    for (int i = start_num; i <= end_num; i++) {
        if (isPrime(i)) {
            local_count++;
        }
    }

    // Gather results
    MPI_Reduce(&local_count, &global_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    end_time = MPI_Wtime();

    // Print individual results (Now everyone should have ~100k)
    printf("  [Node %s | Rank %d] Range: %d to %d | Found %d primes.\n", 
           hostname, rank, start_num, end_num, local_count);
    
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        printf("\n--- SUMMARY ---\n");
        printf("Total Primes Found: %d\n", global_count);
        printf("Time Taken        : %.4f seconds\n", end_time - start_time);
        printf("=======================\n");
    }

    MPI_Finalize();
    return 0;
}
