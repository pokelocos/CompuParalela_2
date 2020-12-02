#include "mpi.h"
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>

const int N = 100;
const int RANGE = 100000;
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
    }

    if(rank == 0)
    {
        std::cout<<" Vector: "<<std::endl; 
        for(int i = 0; i < N; i++)
        {
            std::cout<<" " << array[i] << " -"; 
        }
        std::cout << std::endl << std::endl;
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
        std::cout << std::endl << std::endl;
    }
    MPI_Bcast(array.data(), N, MPI_INT, 0, MPI_COMM_WORLD);
    
}


std::vector<int> PSRS(std::vector<int> array, int rank)
{
    int *toSort = new int[N/PROCESSOR]; // -> disjointed vector where the sorting will be performed
    // BEGIN STEP 1 //
    
    MPI_Scatter(array.data(), N/PROCESSOR, MPI_INT, toSort, N/PROCESSOR, MPI_INT, 0, MPI_COMM_WORLD);
    
    QuickSort(toSort, 0, N/PROCESSOR); //-> apply quicksort in all processors

    if(rank == 0)
    {
        std::cout<<" Vector: "<<std::endl; 
        for(int i = 0; i < N/PROCESSOR; i++)
        {
            std::cout<<" " << toSort[i] << " -"; 
        }
        std::cout << std::endl << std::endl;
    }

    // END STEP 1 //

    // BEGIN STEP 2 //

    
    int processorPivots[PROCESSOR];
    int candidatePivots[PROCESSOR*PROCESSOR];
    int pivots[PROCESSOR - 1];

    int size = N/PROCESSOR;

    for(int i = 0; i < PROCESSOR; i++)
    {
        processorPivots[i] = toSort[i*size/PROCESSOR];
    }
    MPI_Gather(processorPivots, PROCESSOR, MPI_INT, candidatePivots, PROCESSOR, MPI_INT, 0, MPI_COMM_WORLD);


    if(rank == 0)
    {        
        QuickSort(candidatePivots, 0, PROCESSOR*PROCESSOR);
        for(int i = 0; i < PROCESSOR - 1; i++)
        {
            pivots[i] = candidatePivots[(i+1)*PROCESSOR]; // -> (i+1)*PROCESSOR = (i+1)*(PROCESSOR*PROCESSOR/PROCESSOR)
        }
    }
    MPI_Bcast(pivots, PROCESSOR-1, MPI_INT, 0, MPI_COMM_WORLD); 
    // END STEP 2 //

    // BEGIN STEP 3 //

    //calcular largo de datos a enviar y luego enviar con scatter y scatterV
    int lastIndex = 0;
    int indexes[PROCESSOR];
    int sizes[PROCESSOR];

    int j = 0;
    for(int i = 0; i < N/PROCESSOR; i++)
    {
        if(toSort[i] < pivots[j]) continue;
        indexes[j] = lastIndex;
        lastIndex = i;
        sizes[j] = lastIndex - indexes[j];
        j++;
    }
    indexes[j] = lastIndex;
    sizes[j] = N/PROCESSOR - indexes[j];

    // END STEP 3 //

    // BEGIN STEP 4 //

    std::vector<int> sorted;

    MPI_Scatterv(toSort, sizes, indexes, MPI_INT, sorted.data(), 0, MPI_INT, rank, MPI_COMM_WORLD);
    // END STEP 4 //

    // FINAL STEP //

    int length = sorted.end() - sorted.begin();
    
    MPI_Gather(&length, 1, MPI_INT, sizes, 1, MPI_INT, 0, MPI_COMM_WORLD);

    indexes[0] = 0;
    for(int i = 0; i < PROCESSOR - 1; i++)
    {
        indexes[i+1] = indexes[i] + sizes[0];
    }

    MPI_Gatherv(sorted.data(), length, MPI_INT, array.data(), sizes, indexes, MPI_INT, 0, MPI_COMM_WORLD);
        
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
	//Swap(&array[i + 1], &array[high]); 
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