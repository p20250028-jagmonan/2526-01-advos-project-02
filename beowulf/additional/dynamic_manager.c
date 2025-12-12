#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TOTAL_TASKS 50
#define LOG_FILE "/cluster/task_log.txt"

// Function to check if a task is already done
int is_task_done(int task_id) {
    if (access(LOG_FILE, F_OK) == -1) return 0;
    
    FILE *fp = fopen(LOG_FILE, "r");
    int completed_id;
    while (fscanf(fp, "%d", &completed_id) != EOF) {
        if (completed_id == task_id) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// Function to mark task as done
void mark_task_done(int task_id) {
    FILE *fp = fopen(LOG_FILE, "a");
    fprintf(fp, "%d\n", task_id);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // --- MASTER (MANAGER) ---
    if (rank == 0) {
        printf("\n=== DYNAMIC WORKLOAD MANAGER ===\n");
        printf("Checking logs for previous progress...\n");

        int tasks_assigned = 0;
        int active_workers = size - 1;
        int *worker_status = (int *)calloc(size, sizeof(int)); // 0=Idle, 1=Busy
        int task_iterator = 0;

        // 1. Assign Initial Work
        for (int i = 1; i < size; i++) {
            // Find next incomplete task
            while (task_iterator < TOTAL_TASKS && is_task_done(task_iterator)) {
                task_iterator++;
            }

            if (task_iterator < TOTAL_TASKS) {
                MPI_Send(&task_iterator, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                printf("[MANAGER] Assigned Task %d to Worker %d\n", task_iterator, i);
                worker_status[i] = 1;
                task_iterator++;
            } else {
                // No work left, send kill signal
                int kill = -1;
                MPI_Send(&kill, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                active_workers--;
            }
        }

        // 2. Dynamic Loop (Receive Result -> Send New Task)
        while (active_workers > 0) {
            int done_task;
            MPI_Status status;
            
            // Wait for ANY worker to finish
            MPI_Recv(&done_task, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            
            int source = status.MPI_SOURCE;
            printf("   -> [SUCCESS] Worker %d finished Task %d\n", source, done_task);
            mark_task_done(done_task);

            // Find next incomplete task
            while (task_iterator < TOTAL_TASKS && is_task_done(task_iterator)) {
                task_iterator++;
            }

            if (task_iterator < TOTAL_TASKS) {
                MPI_Send(&task_iterator, 1, MPI_INT, source, 0, MPI_COMM_WORLD);
                printf("[MANAGER] Re-Assigned Task %d to Worker %d\n", task_iterator, source);
                task_iterator++;
            } else {
                // No more work
                int kill = -1;
                MPI_Send(&kill, 1, MPI_INT, source, 0, MPI_COMM_WORLD);
                active_workers--;
            }
        }
        printf("=== ALL TASKS COMPLETED ===\n");
    } 
    
    // --- WORKER ---
    else {
        int task_id;
        while (1) {
            MPI_Recv(&task_id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (task_id == -1) break; // Kill signal

            // Simulate Heavy Work
            sleep(1); 
            
            // Return Task ID as proof of work
            MPI_Send(&task_id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}
