#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

int main(int argc, char *argv[]) {
    int rank, size, i;
    long long total_iters, my_iters;
    long long my_hits = 0, total_hits = 0;
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
    
    total_iters = atoll(argv[1]);
    my_iters = total_iters / size;

    // Seed random number generator unique to rank
    unsigned int seed = time(NULL) + rank;

    // 2. THE LAND SURVEY (Monte Carlo Simulation)
    // We are looking for the area of the shape: |x|^3 + |y|^3 <= 1
    // This is a "Super-Ellipse" (looks like a swollen square)
    for (i = 0; i < my_iters; i++) {
        // Generate random X and Y between -1.0 and 1.0
        x = (double)rand_r(&seed) / RAND_MAX * 2.0 - 1.0;
        y = (double)rand_r(&seed) / RAND_MAX * 2.0 - 1.0;

        // The "Island" Formula: abs(x)^3 + abs(y)^3 <= 1
        if ( (pow(fabs(x), 3) + pow(fabs(y), 3)) <= 1.0 ) {
            my_hits++;
        }
    }

    // 3. VISUAL LOGGING (For the Demo)
    usleep(rank * 2000); // Small delay so prints don't overlap
    printf("[Node: %s | Rank %d] Scanned %lld points\n", hostname, rank, my_iters);

    // 4. Gather Results
    MPI_Reduce(&my_hits, &total_hits, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    // 5. Final Output (Formatted for the Script)
    if (rank == 0) {
        // Output format MUST BE: CHUNK_RESULT=ITERS=INSIDE
        printf("CHUNK_RESULT=%lld=%lld\n", total_iters, total_hits);
    }

    MPI_Finalize();
    return 0;
}
