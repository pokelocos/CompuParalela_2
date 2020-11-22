#include "mpi.h"
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>

const int N = 100;
const int RANGE = 1000;
const int PROCESSOR = 4;

int main(int argc, char *argv[])
{
    MPI_Init (&argc, &argv); // Initialize MPI envirnmnt
	int rank, size, namelen;
	char name[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_rank (MPI_COMM_WORLD, &rank); // ID of current process
	MPI_Get_processor_name (name, &namelen); // Hostname of node
	MPI_Comm_size (MPI_COMM_WORLD, &size); // Number of processes

    int *array;
    int *toSort;

    if(rank == 0)
    {
        InitializeRandomArray(array);


        for(int i = 1; i < PROCESSOR; i++)
        {
            MPI_Send(array[(i-1)*N/PROCESSOR], N/PROCESSOR, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

        toSort = array[(PROCESSOR-1)*N/PROCESSOR];
    }
    else
    {
        MPI_Recv(toSort, N/PROCESSOR, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    
    
}

void InitializeRandomArray(int *array)
{
    std::mt19937 rng;
    for(int i = 0; i < N; i++)
    {
        rng.seed(i);
        std::generate_n(array[i],N,[&](){return rng()%RANGE;});
    }
}