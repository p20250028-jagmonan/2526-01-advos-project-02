#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h> // For checking file existence

#define NUM_STARS 10000 
#define NUM_STEPS 100    // Increased steps so you have time to kill it
#define CHECKPOINT_FILE "/cluster/checkpoint.dat"

typedef struct {
    double x, y;
    double mass;
    double vx, vy;
} Star;

void save_checkpoint(Star *stars, int step) {
    FILE *fp = fopen(CHECKPOINT_FILE, "wb");
    if (fp) {
        fwrite(&step, sizeof(int), 1, fp);
        fwrite(stars, sizeof(Star), NUM_STARS, fp);
        fclose(fp);
        printf("[CHECKPOINT] Progress saved at Step %d\n", step);
    }
}

int load_checkpoint(Star *stars) {
    if (access(CHECKPOINT_FILE, F_OK) != -1) {
        FILE *fp = fopen(CHECKPOINT_FILE, "rb");
        int step = 0;
        if (fp) {
            fread(&step, sizeof(int), 1, fp);
            fread(stars, sizeof(Star), NUM_STARS, fp);
            fclose(fp);
            return step;
        }
    }
    return 0; // If no file, start at 0
}

int main(int argc, char *argv[]) {
    int rank, size;
    int i, j, step, start_step = 0;
    double dx, dy, distance, force;
    double G = 6.674e-11; 

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Star *stars = (Star *)malloc(NUM_STARS * sizeof(Star));

    // --- INITIALIZATION / RESUME LOGIC ---
    if (rank == 0) {
        // Try to load from file first
        start_step = load_checkpoint(stars);
        
        if (start_step > 0) {
            printf("\n=== RESUMING GALAXY SIMULATION FROM STEP %d ===\n", start_step);
        } else {
            printf("\n=== NEW GALAXY SIMULATION ===\n");
            // Random init if no checkpoint
            for (i = 0; i < NUM_STARS; i++) {
                stars[i].x = (rand() % 1000) * 1.0;
                stars[i].y = (rand() % 1000) * 1.0;
                stars[i].mass = (rand() % 100) * 10.0;
                stars[i].vx = 0;
                stars[i].vy = 0;
            }
        }
    }

    // Broadcast the Start Step so everyone knows where to begin
    MPI_Bcast(&start_step, 1, MPI_INT, 0, MPI_COMM_WORLD);
    // Broadcast the Star Data (either Random or Loaded from file)
    MPI_Bcast(stars, NUM_STARS * sizeof(Star), MPI_BYTE, 0, MPI_COMM_WORLD);

    int stars_per_proc = NUM_STARS / size;
    int start_index = rank * stars_per_proc;
    int end_index = (rank == size - 1) ? NUM_STARS : start_index + stars_per_proc;

    // --- MAIN LOOP ---
    // Note: We start loop at 'start_step', not 0!
    for (step = start_step; step < NUM_STEPS; step++) {
        
        if (rank == 0 && step % 10 == 0) { 
            printf("Processing Step %d/%d...\n", step, NUM_STEPS); 
            // Save Checkpoint every 10 steps
            save_checkpoint(stars, step);
        }

        // Heavy Math
        for (i = start_index; i < end_index; i++) {
            double ax = 0.0, ay = 0.0;
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

        // IMPORTANT: In a real simulation, we need to gather positions here 
        // to update the shared state for the next checkpoint.
        // For this demo, we just sync.
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0) printf("Simulation Complete.\n");
    
    // Cleanup checkpoint file on success so next run starts fresh
    if (rank == 0) remove(CHECKPOINT_FILE);

    free(stars);
    MPI_Finalize();
    return 0;
}
