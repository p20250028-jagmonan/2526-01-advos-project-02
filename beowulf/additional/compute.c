#include <mpi.h>
#include <stdio.h>

double work(long n) {
  double x = 0.0;
  for (long i = 0; i < n; ++i) {
    x += (i % 7) * 0.123456789;
  }
  return x;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  long N = 50000000L;
  double r = work(N);
  printf("Rank %d finished work: %f\n", rank, r);

  MPI_Finalize();
  return 0;
}
