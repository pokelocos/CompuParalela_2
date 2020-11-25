#include "mpi.h"
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>

const int N = 100;
const int RANGE = 1000;
const int PROCESSOR = 4;

void Swap(int* a, int* b);
int Partition (int *array, int low, int high) ;
void QuickSort(int *array, int low, int high);
std::vector<int> PSRS(std::vector<int> array, int rank);


int main(int argc, char *argv[])
{
    MPI_Init (&argc, &argv); // Initialize MPI envirnmnt
	int rank, size, namelen;
	char name[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_rank (MPI_COMM_WORLD, &rank); // ID of current process
	MPI_Get_processor_name (name, &namelen); // Hostname of node
	MPI_Comm_size (MPI_COMM_WORLD, &size); // Number of processes
    
    std::vector<int> array(N);

    if(rank == 0)
    {
        //Initialization of vector
        std::mt19937 rng;
        for(int i = 0; i < N; i++)
        {
            rng.seed(i);
            std::generate(array.begin(),array.end(),[&](){return rng()%RANGE;});
        }
        std::cout<<"unrodered array :"<<std::endl;
        for(int i = 0; i < N; i++)
        {
            std::cout<<" " << array[i] << " -"; 
        }
        std::cout<<std::endl<<std::endl;
    }
    MPI_Bcast(array.data(), N, MPI_INT, 0, MPI_COMM_WORLD);
    array = PSRS(array, rank);

    if(rank == 0)
    {
        std::cout<<" Vector: "<<std::endl; 
        for(int i = 0; i < N; i++)
        {
            std::cout<<" " << array[i] << " -"; 
        }
    }
    
}


std::vector<int> PSRS(std::vector<int> array, int rank)
{
    int *toSort = new int[N/PROCESSOR]; // -> disjointed vector where the sorting will be performed

    // BEGIN STEP 1 //

    if(rank == 0)
    {
        MPI_Scatter(array.data(), N/PROCESSOR, MPI_INT, toSort, N/PROCESSOR, MPI_INT, 0, MPI_COMM_WORLD);
    }
    /*
        //partition and send initial vector among processors
        for(int i = 1; i < PROCESSOR; i++)
        {
            MPI_Send(array.data() + (i-1)*N/PROCESSOR, N/PROCESSOR, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

        //rank 0 works on final tract as N/PROCESSOR may not be an integer
        toSort = array.data() + (PROCESSOR-1)*N/PROCESSOR;
    }
    else
    {
        toSort = new int[N/PROCESSOR]; // -> initialization of array to set to buffer size
        MPI_Recv(toSort, N/PROCESSOR, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // -> receive vector tract from rank 0
    }*/

    if(rank == 0)
    {
        std::cout<<" ToSort "<<std::endl;
        int x;
        for(x = 0; x < N/PROCESSOR; x++)
        {
            std::cout<<toSort[x]<<" - ";
        }
        std::cout<<std::endl;
        //std::cout<<"rank "<<rank<<" size of array to Sort: "<<x<<std::endl;
    }

    std::cout<<" Sorting "<<std::endl;
    QuickSort(toSort, 0, N/PROCESSOR); //-> apply quicksort in all processors

    if(rank == 0)
    {
        std::cout<<" Sorted "<<std::endl;
        int x;
        for(x = 0; x < N/PROCESSOR; x++)
        {
            std::cout<<toSort[x]<<" - ";
        }
        std::cout<<std::endl;
        //std::cout<<"rank "<<rank<<" size of array to Sort: "<<x<<std::endl;
    }
    

    // END STEP 1 //

    // BEGIN STEP 2 //

    
    int pivots[PROCESSOR -1];

    if(rank == 0)
    {
        int candidatePivots[PROCESSOR*PROCESSOR];
        for(int i = 0; i < PROCESSOR; i++)
        {
            candidatePivots[i] = toSort[i*(N/PROCESSOR*PROCESSOR)]; //  -> N/PROCESSOR*PROCESSOR = (N/PROCESSOR)/PROCESSOR
            std::cout<<"rank "<<rank<< "| pivot "<<i<<": "<<candidatePivots[i]<<std::endl;

        }

        for(int i = 1; i < PROCESSOR; i++)
        {
            MPI_Recv(&candidatePivots[(i)*PROCESSOR], PROCESSOR, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        QuickSort(candidatePivots, candidatePivots[0], candidatePivots[PROCESSOR*PROCESSOR - 1]);

        for(int i = 0; i < PROCESSOR - 1; i++)
        {
            pivots[i] = candidatePivots[(i+1)*PROCESSOR]; // -> (i+1)*PROCESSOR = (i+1)*(PROCESSOR*PROCESSOR/PROCESSOR)
        }
        MPI_Bcast(pivots, PROCESSOR-1, MPI_INT, 0, MPI_COMM_WORLD);

    }
    else
    {
        int pivots[PROCESSOR];
        for(int i = 0; i < PROCESSOR; i++)
        {
            pivots[i] = toSort[i*(N/PROCESSOR*PROCESSOR)];
            std::cout<<"rank "<<rank<< "| pivot "<<i<<": "<<pivots[i]<<std::endl;
        }
        MPI_Send(pivots, PROCESSOR, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    // END STEP 2 //

    // BEGIN STEP 3 //

    //calcular largo de datos a enviar y luego enviar con scatter y scatterV

    int lastIndex = 0;
    int transferAmount;

    

    for(int i = 0; i < PROCESSOR -1 ; i++)
    {
        for(int j = lastIndex; j < N/PROCESSOR; j++)
        {
            if(toSort[j] <= pivots[i]) continue;
            transferAmount = j - lastIndex + 1;
            std::cout<<"rank = "<<rank<<" / transfer amount = "<<transferAmount<<std::endl;
            MPI_Send(&transferAmount, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&toSort[j], transferAmount, MPI_INT, i, 1, MPI_COMM_WORLD);
            lastIndex = j;
            break;
        }
    }
    transferAmount = (N/PROCESSOR) - 1 -lastIndex;
    MPI_Send(&transferAmount, 1, MPI_INT, PROCESSOR - 1, 0, MPI_COMM_WORLD);
    MPI_Send(&toSort[lastIndex], transferAmount, MPI_INT, PROCESSOR - 1, 1, MPI_COMM_WORLD);

    // END STEP 3 //

    // BEGIN STEP 4 //

    lastIndex = 0;

    for(int i = 0; i < PROCESSOR; i++)
    {
        MPI_Recv(&transferAmount, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&toSort[lastIndex], transferAmount, MPI_INT, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        lastIndex += transferAmount;
    }

    // END STEP 4 //

    // FINAL STEP //

    transferAmount = lastIndex;

    for(int i = 0; i < PROCESSOR; i++)
    {
        MPI_Send(&transferAmount, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(toSort, transferAmount, MPI_INT, 0, 1, MPI_COMM_WORLD);
    }

    if(rank == 0)
    {
        lastIndex = 0;
        for(int i = 0; i < PROCESSOR; i++)
        {
            MPI_Recv(&transferAmount, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(array.data() + lastIndex, transferAmount, MPI_INT, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            lastIndex += transferAmount;
        }
    }

    return array;
}


void Swap(int* a, int* b) 
{ 
	int t = *a; 
	*a = *b; 
	*b = t; 
} 

/* This function takes last element as pivot, places 
the pivot element at its correct position in sorted 
	array, and places all smaller (smaller than pivot) 
to left of pivot and all greater elements to right 
of pivot */
int Partition (int *array, int low, int high) 
{ 
	int pivot = array[high]; // pivot 
	int i = (low - 1); // Index of smaller element 

	for (int j = low; j <= high- 1; j++) 
	{ 
		// If current element is smaller than or 
		// equal to pivot 
		if (array[j] <= pivot) 
		{ 
			i++; // increment index of smaller element 
			Swap(&array[i], &array[j]); 
		} 
	} 
	Swap(&array[i + 1], &array[high]); 
	return (i + 1); 
} 

/* The main function that implements QuickSort 
arr[] --> Array to be sorted, 
low --> Starting index, 
high --> Ending index */
void QuickSort(int *array, int low, int high)
{ 
	if (low < high) 
	{ 
		/* pi is partitioning index, arr[p] is now 
		at right place */
		int pi = Partition(array, low, high); 

		// Separately sort elements before 
		// partition and after partition 
		QuickSort(array, low, pi - 1); 
		QuickSort(array, pi + 1, high); 
	} 
} 