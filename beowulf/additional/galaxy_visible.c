#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// --- TUNING PARAMETERS ---
#define NUM_STARS 10000 
#define NUM_STEPS 50    

typedef struct {
    double x, y;
    double mass;
    double vx, vy;
} Star;

int main(int argc, char *argv[]) {
    int rank, size;
    int i, j, step;
    double dx, dy, distance, force, start_time, end_time;
    double G = 6.674e-11; 
    
    // Variables for Visual Demo
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int len;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(hostname, &len);

    // Allocate memory for stars
    Star *stars = (Star *)malloc(NUM_STARS * sizeof(Star));

    // Master initializes the Galaxy
    if (rank == 0) {
        printf("\n=== N-BODY GALAXY SIMULATION ===\n");
        printf("Simulating %d Stars for %d Time Steps...\n", NUM_STARS, NUM_STEPS);
        printf("------------------------------------------------\n");
        
        for (i = 0; i < NUM_STARS; i++) {
            stars[i].x = (rand() % 1000) * 1.0;
            stars[i].y = (rand() % 1000) * 1.0;
            stars[i].mass = (rand() % 100) * 10.0;
            stars[i].vx = 0;
            stars[i].vy = 0;
        }
        start_time = MPI_Wtime();
    }

    // Broadcast the universe
    MPI_Bcast(stars, NUM_STARS * sizeof(Star), MPI_BYTE, 0, MPI_COMM_WORLD);

    // --- WORK DISTRIBUTION CALCULATION ---
    // We calculate this ONCE at the start to print the status
    int stars_per_proc = NUM_STARS / size;
    int start_index = rank * stars_per_proc;
    int end_index = start_index + stars_per_proc;
    
    // Handle remainder for the last rank
    if (rank == size - 1) {
        end_index = NUM_STARS;
    }

    // --- VISUAL PROOF: Each Node Announces its Job ---
    MPI_Barrier(MPI_COMM_WORLD); // Sync so they don't print over the header
    
    printf("  [Node %s | Rank %d] Calculating forces for Stars %d to %d\n", 
           hostname, rank, start_index, end_index);
           
    MPI_Barrier(MPI_COMM_WORLD); // Wait for printing to finish
    if (rank == 0) printf("------------------------------------------------\n");

    // --- TIME STEP LOOP ---
    for (step = 0; step < NUM_STEPS; step++) {
        
        // Print progress bar on Master every 10 steps
        if (rank == 0 && step % 10 == 0) { 
            printf("Processing Step %d/%d...\n", step, NUM_STEPS); 
        }

        // --- HEAVY CALCULATION START ---
        for (i = start_index; i < end_index; i++) {
            double ax = 0.0;
            double ay = 0.0;

            for (j = 0; j < NUM_STARS; j++) {
                if (i == j) continue; 

                dx = stars[j].x - stars[i].x;
                dy = stars[j].y - stars[i].y;
                distance = sqrt(dx*dx + dy*dy);
                if (distance < 1.0) distance = 1.0; 

                force = (G * stars[i].mass * stars[j].mass) / (distance * distance);
                ax += force * dx / distance;
                ay += force * dy / distance;
            }
            stars[i].vx += ax;
            stars[i].vy += ay;
        }
        // --- HEAVY CALCULATION END ---

        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0) {
        end_time = MPI_Wtime();
        printf("\nSimulation Complete.\n");
        printf("Time Taken: %.4f seconds\n", end_time - start_time);
        printf("================================\n");
    }

    free(stars);
    MPI_Finalize();
    return 0;
}
