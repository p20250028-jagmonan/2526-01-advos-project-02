#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define N 1000  // Matrix size N x N

// Helper function to print matrix
void print_matrix(double *mat, int rows, int cols) {

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            printf("%6.2f ", mat[i * cols + j]);
        printf("\n");
    }

}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int rows_per_proc = N / size;
    int remainder = N % size;

    int start_row, end_row;
    if (rank < remainder) {
        start_row = rank * (rows_per_proc + 1);
        end_row = start_row + rows_per_proc;
    } else {
        start_row = rank * rows_per_proc + remainder;
        end_row = start_row + rows_per_proc - 1;
    }

    // Allocate matrices
    double *A = NULL;
    double *B = (double *)malloc(N * N * sizeof(double));
    double *C_local = (double *)malloc((end_row - start_row + 1) * N * sizeof(double));

    // Initialize matrix B on all processes
    for (int i = 0; i < N * N; i++)
        B[i] = i + 1;

    // Initialize matrix A only on rank 0
    if (rank == 0) {
        A = (double *)malloc(N * N * sizeof(double));
        for (int i = 0; i < N * N; i++)
            A[i] = i + 1;
    }

    // Scatter rows of A to all processes
    int *sendcounts = (int *)malloc(size * sizeof(int));
    int *displs = (int *)malloc(size * sizeof(int));
    int offset = 0;
    for (int i = 0; i < size; i++) {
        int rows = (i < remainder) ? (rows_per_proc + 1) : rows_per_proc;
        sendcounts[i] = rows * N;
        displs[i] = offset;
        offset += rows * N;
    }

    double *A_local = (double *)malloc(sendcounts[rank] * sizeof(double));
    MPI_Scatterv(A, sendcounts, displs, MPI_DOUBLE, A_local, sendcounts[rank], MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Perform local multiplication
    int local_rows = end_row - start_row + 1;
    for (int i = 0; i < local_rows; i++) {
        for (int j = 0; j < N; j++) {
            C_local[i * N + j] = 0.0;
            for (int k = 0; k < N; k++)
                C_local[i * N + j] += A_local[i * N + k] * B[k * N + j];
        }
    }

    // Gather results to rank 0
    double *C = NULL;
    if (rank == 0)
        C = (double *)malloc(N * N * sizeof(double));

    MPI_Gatherv(C_local, local_rows * N, MPI_DOUBLE, C, sendcounts, displs, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (rank == 0) {

        printf("Result matrix C:\n");
        print_matrix(C, N, N);
  
      free(A);
        free(C);
    }

    free(B);
    free(A_local);
    free(C_local);
    free(sendcounts);
    free(displs);

    MPI_Finalize();
    return 0;
}

