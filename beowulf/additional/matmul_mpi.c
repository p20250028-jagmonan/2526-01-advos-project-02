#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Chunk-aware MPI matrix multiply (C = A * B)
 *
 * Usage (per chunk, driven by run_chunks.sh):
 *   mpirun -np <P> ... matmul_mpi N --chunk-id K --num-chunks M
 *
 * - N          : size of square matrices (N x N)
 * - chunk-id   : which chunk of rows this run should compute (0 .. M-1)
 * - num-chunks : total number of chunks
 *
 * Output:
 *   Rank 0 writes the rows for this chunk into:
 *     /cluster/results/C_chunk_<chunk_id>.txt
 *
 * For simplicity:
 *   A[i][j] = i + j
 *   B[i][j] = i * j
 *   C = A * B (naive O(N^3) per row)
 *
 * All ranks allocate A and B (replicated data) to keep MPI logic simple.
 * Each rank computes only the rows in:
 *   intersection(rank_row_range, chunk_row_range)
 * and sends them to rank 0 via MPI_Gatherv (with custom displacements).
 */

static void die(const char *msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    MPI_Abort(MPI_COMM_WORLD, 1);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s N [--chunk-id K --num-chunks M]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int N = atoi(argv[1]);
    if (N <= 0) {
        if (rank == 0) fprintf(stderr, "N must be > 0\n");
        MPI_Finalize();
        return 1;
    }

    /* Default chunking: 1 chunk that covers all rows */
    int chunk_id = 0;
    int num_chunks = 1;

    /* Parse optional args */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--chunk-id") == 0 && i + 1 < argc) {
            chunk_id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--num-chunks") == 0 && i + 1 < argc) {
            num_chunks = atoi(argv[++i]);
        }
    }

    if (chunk_id < 0 || chunk_id >= num_chunks) {
        if (rank == 0) {
            fprintf(stderr, "Invalid chunk-id %d (num-chunks=%d)\n", chunk_id, num_chunks);
        }
        MPI_Finalize();
        return 1;
    }

    /* Global chunk row range [chunk_start, chunk_end) */
    int chunk_start = (chunk_id * N) / num_chunks;
    int chunk_end   = ((chunk_id + 1) * N) / num_chunks;
    int chunk_rows  = chunk_end - chunk_start;
    if (chunk_rows <= 0) {
        /* Nothing to do for this chunk */
        if (rank == 0) {
            fprintf(stderr, "Chunk %d has no rows to compute (N=%d, num_chunks=%d)\n",
                    chunk_id, N, num_chunks);
        }
        MPI_Finalize();
        return 0;
    }

    if (rank == 0) {
        printf("[matmul] N=%d, chunk-id=%d, num-chunks=%d, rows=[%d,%d)\n",
               N, chunk_id, num_chunks, chunk_start, chunk_end);
        fflush(stdout);
    }

    /* Rank-specific row range [rank_start, rank_end) over all N rows */
    int rank_start = (rank * N) / size;
    int rank_end   = ((rank + 1) * N) / size;

    /* Intersection with chunk range */
    int local_start = (rank_start > chunk_start) ? rank_start : chunk_start;
    int local_end   = (rank_end   < chunk_end)   ? rank_end   : chunk_end;
    int local_rows  = (local_end > local_start) ? (local_end - local_start) : 0;

    /* Allocate A, B (replicated on all ranks for simplicity) */
    double *A = (double *)malloc((size_t)N * N * sizeof(double));
    double *B = (double *)malloc((size_t)N * N * sizeof(double));
    if (!A || !B) die("Not enough memory for A/B allocation");

    /* Initialize A and B deterministically */
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i * N + j] = (double)(i + j);
            B[i * N + j] = (double)(i * j);
        }
    }

    /* Compute local contribution C_local (only if we have rows) */
    double *C_local = NULL;
    if (local_rows > 0) {
        C_local = (double *)malloc((size_t)local_rows * N * sizeof(double));
        if (!C_local) die("Not enough memory for C_local");

        for (int r = 0; r < local_rows; r++) {
            int global_row = local_start + r;
            for (int c = 0; c < N; c++) {
                double sum = 0.0;
                for (int k = 0; k < N; k++) {
                    double a = A[global_row * N + k];
                    double b = B[k * N + c];
                    sum += a * b;
                }
                C_local[r * N + c] = sum;
            }
        }
    }

    free(A);
    free(B);

    /* Gather results on rank 0 into C_chunk (chunk_rows x N) */
    double *C_chunk = NULL;
    int *recvcounts = NULL;
    int *displs = NULL;

    if (rank == 0) {
        C_chunk = (double *)malloc((size_t)chunk_rows * N * sizeof(double));
        if (!C_chunk) die("Not enough memory for C_chunk");
        recvcounts = (int *)malloc(size * sizeof(int));
        displs     = (int *)malloc(size * sizeof(int));
        if (!recvcounts || !displs) die("Not enough memory for recvcounts/displs");

        /* Compute recvcounts and displs based on each rank's local range */
        for (int r = 0; r < size; r++) {
            int r_start = (r * N) / size;
            int r_end   = ((r + 1) * N) / size;
            int loc_start_r = (r_start > chunk_start) ? r_start : chunk_start;
            int loc_end_r   = (r_end   < chunk_end)   ? r_end   : chunk_end;
            int loc_rows_r  = (loc_end_r > loc_start_r) ? (loc_end_r - loc_start_r) : 0;

            recvcounts[r] = loc_rows_r * N;
            if (loc_rows_r > 0) {
                int offset_rows = loc_start_r - chunk_start; /* relative to chunk start */
                displs[r] = offset_rows * N;
            } else {
                displs[r] = 0;
            }
        }
    }

    int sendcount = local_rows * N;

    MPI_Gatherv(
        C_local, sendcount, MPI_DOUBLE,
        C_chunk, recvcounts, displs, MPI_DOUBLE,
        0, MPI_COMM_WORLD
    );

    if (C_local) free(C_local);
    if (recvcounts) free(recvcounts);
    if (displs) free(displs);

    /* Rank 0 writes this chunk's rows to a text file */
    if (rank == 0) {
        char fname[256];
        snprintf(fname, sizeof(fname),
                 "/cluster/results/C_chunk_%d.txt", chunk_id);

        FILE *f = fopen(fname, "w");
        if (!f) {
            perror("fopen C_chunk");
            free(C_chunk);
            MPI_Finalize();
            return 1;
        }

        /* We write chunk_rows lines, each with N numbers.
         * Chunks will be concatenated in order later.
         */
        for (int r = 0; r < chunk_rows; r++) {
            for (int c = 0; c < N; c++) {
                fprintf(f, "%.6f", C_chunk[r * N + c]);
                if (c != N - 1) fputc(' ', f);
            }
            fputc('\n', f);
        }

        fclose(f);
        printf("[matmul] Wrote chunk %d rows [%d,%d) to %s\n",
               chunk_id, chunk_start, chunk_end, fname);
        fflush(stdout);

        free(C_chunk);
    }

    MPI_Finalize();
    return 0;
}
