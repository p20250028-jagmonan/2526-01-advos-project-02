#include <stdio.h>
#include <mpi.h>

typedef struct {
    double length;
    double width;
} Rectangle;

double compute_perimeter(Rectangle r) {
    return 2 * (r.length + r.width);
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Example rectangles
    Rectangle rectangles[] = {
        {3.0, 0.0},  // You can change these
        {2.0, 2.5},
        {5.0, 5.0},
        {4.0, 6.0}
    };
    int n_rects = sizeof(rectangles) / sizeof(rectangles[0]);

    int per_process = (n_rects + size - 1) / size; // ceil(n_rects / size)
    int start = rank * per_process;
    int end = start + per_process;
    if (end > n_rects) end = n_rects;

    double local_sum = 0.0;
    for (int i = start; i < end; i++) {
        double p = compute_perimeter(rectangles[i]);
        printf("Process %d computed perimeter of rectangle %d: %.2f\n", rank, i, p);
        local_sum += p;
    }

    double total_perimeter = 0.0;
    MPI_Reduce(&local_sum, &total_perimeter, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Total perimeter of all rectangles = %.2f\n", total_perimeter);
    }

    MPI_Finalize();
    return 0;
}
