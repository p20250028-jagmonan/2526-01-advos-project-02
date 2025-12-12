#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Each rectangle has width and height
    double widths[] = {2.0, 3.5, 4.0, 5.0};
    double heights[] = {3.0, 2.0, 5.0, 4.0};
    int n = sizeof(widths)/sizeof(widths[0]);

    double local_area = 0.0;

    // Each process computes the area of rectangles assigned to it
    for (int i = rank; i < n; i += size) {
        double area = widths[i] * heights[i];
        printf("Process %d computed area of rectangle %d: %.2f\n", rank, i, area);
        local_area += area;
    }

    // Reduce to get total area
    double total_area = 0.0;
    MPI_Reduce(&local_area, &total_area, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\nTotal area of all rectangles = %.2f\n", total_area);
    }

    MPI_Finalize();
    return 0;
}
