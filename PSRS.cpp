#include "mpi.h"
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <chrono>  
#include <cmath>
#include <queue>

const int N = 100;
const int RANGE = 100000;
const int PROCESSOR = 4;

void Swap(int* a, int* b);
int Partition (int *array, int low, int high) ;
void QuickSort(int *array, int low, int high);
std::vector<int> PSRS(std::vector<int> array, int rank);
std::vector<int> MergeSort(std::vector<int> nums, int rank);



int main(int argc, char *argv[])
{
    MPI_Init (&argc, &argv); // Initialize MPI envirnmnt
	int rank, size, namelen;
	char name[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_rank (MPI_COMM_WORLD, &rank); // ID of current process
	MPI_Get_processor_name (name, &namelen); // Hostname of node
	MPI_Comm_size (MPI_COMM_WORLD, &size); // Number of processes
    
    std::vector<int> array(N);
    std::vector<int> mergeArray(N);

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



    if(rank == 0)
    {
        //Initialization of vector
        std::mt19937 rng;
        for(int i = 0; i < N; i++)
        {
            rng.seed(i);
            std::generate(mergeArray.begin(),mergeArray.end(),[&](){return rng()%RANGE;});
        }
    }
    MPI_Bcast(mergeArray.data(), N, MPI_INT, 0, MPI_COMM_WORLD);
    MergeSort(mergeArray, rank);

    MPI_Bcast(mergeArray.data(), N, MPI_INT, 0, MPI_COMM_WORLD);

}


std::vector<int> PSRS(std::vector<int> array, int rank)
{
    int size = N/PROCESSOR;

    int *toSort = new int[size]; // -> disjointed vector where the sorting will be performed
    // BEGIN STEP 1 //
    
    //Distribute array betwen processors
    MPI_Scatter(array.data(), size, MPI_INT, toSort, size, MPI_INT, 0, MPI_COMM_WORLD);
    
    QuickSort(toSort, 0, size - 1); //-> apply quicksort in all processors

    // END STEP 1 //

    // BEGIN STEP 2 //

    
    int processorPivots[PROCESSOR]; // -> pivots of each processor array
    int candidatePivots[PROCESSOR*PROCESSOR]; // -> pivots of all processors
    int pivots[PROCESSOR - 1]; // -> selected pivots to implement sort

    // select pivots in each processor
    for(int i = 0; i < PROCESSOR; i++)
    {
        processorPivots[i] = toSort[i*size/PROCESSOR];
    }
    //rank 0 collect all the pivots into candidate pivots
    MPI_Gather(processorPivots, PROCESSOR, MPI_INT, candidatePivots, PROCESSOR, MPI_INT, 0, MPI_COMM_WORLD);


    if(rank == 0)
    {        
        //order candiate pivots
        QuickSort(candidatePivots, 0, PROCESSOR*PROCESSOR - 1);
        //select pivots to use
        for(int i = 0; i < PROCESSOR - 1; i++)
        {
            pivots[i] = candidatePivots[(i+1)*PROCESSOR]; // -> (i+1)*PROCESSOR = (i+1)*(PROCESSOR*PROCESSOR/PROCESSOR)
        }
    }
    //Inform pivots to all processors
    MPI_Bcast(pivots, PROCESSOR-1, MPI_INT, 0, MPI_COMM_WORLD); 
    // END STEP 2 //

    // BEGIN STEP 3 //

    //calcular largo de datos a enviar y luego enviar con scatter y scatterV

    int lastIndex = 0;//-> save last partition index
    //store indexes at wich each rank must implement partition
    int indexes[PROCESSOR*PROCESSOR];
    //store sizes of each rank data packs
    int sizes[PROCESSOR*PROCESSOR];

    int j = 0;// -> keeps track of actual pivot
    //search for partition using selected pivot
    for(int i = 0; i < N/PROCESSOR; i++)
    {
        if(toSort[i] < pivots[j]) continue;//-> partition only if number > pivot
        indexes[rank*PROCESSOR + j] = lastIndex;//-> store prev partition index, first index must be 0
        lastIndex = i;// -> actualize last partition
        sizes[rank*PROCESSOR + j] = lastIndex - indexes[rank*PROCESSOR + j]; // -> data pack size goes from previous partition to current partition
        j++; //-> actualize pivot
        if(j >= PROCESSOR - 1) break;// -> if last pivot checked break;
    }
    indexes[rank*PROCESSOR + j] = lastIndex; // -> set last value
    sizes[rank*PROCESSOR + j] = N/PROCESSOR - indexes[rank*PROCESSOR + j];// -> set last value

    //each rank inform of its partition data to the other ranks
    for(int i = 0; i < PROCESSOR; i++)
    {
        MPI_Bcast(&indexes[i*PROCESSOR], PROCESSOR, MPI_INT, i, MPI_COMM_WORLD);
        MPI_Bcast(&sizes[i*PROCESSOR], PROCESSOR, MPI_INT, i, MPI_COMM_WORLD);  
    }
   
    // END STEP 3 //

    // BEGIN STEP 4 //

    //Calculate receive buffer size from data pack sizes send to current rank
    int length = 0;
    for(int i = 0; i < PROCESSOR; i++)
    {
        length += sizes[i*PROCESSOR + rank];
    }

    int auxSizes[PROCESSOR];
    int auxIndexes[PROCESSOR];
    for(int i = 0; i < PROCESSOR; i++)
    {
        auxIndexes[i] = indexes[rank*PROCESSOR + i];
        auxSizes[i] = sizes[rank*PROCESSOR + i];
        
    }

    std::vector<int> sorted(length);

    int strides[PROCESSOR];
    for(int i = 0; i < PROCESSOR; i++)
    {
        if(i == 0)
        {  
            strides[0] = 0;
        }
        else
        {
            strides[i] = sizes[(i-1)*PROCESSOR + rank] + strides[i-1];
        }        
    }

    for(int i = 0; i < PROCESSOR; i++)
    {
        if(i == rank)
        {
            sorted.insert(sorted.end(), toSort[auxIndexes[i]], toSort[auxIndexes[i] + auxSizes[i]]);
        }
        else
        {
            MPI_Scatterv(toSort, auxSizes, auxIndexes, MPI_INT, sorted.data(), strides[i], MPI_INT, rank, MPI_COMM_WORLD);
            if(rank == 2)std::cout<<i<<std::endl;
        }
    }
    
    /*
    for(int i = 0; i < PROCESSOR; i++)
    {
        MPI_Send(&toSort[auxIndexes[i]], auxSizes[i], MPI_INT, i, i, MPI_COMM_WORLD);
    }

    for(int i = 0; i < PROCESSOR; i++)
    {
        MPI_Probe(i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int arr[sizes[i*PROCESSOR + rank]];
        MPI_Recv(arr, sizes[i*PROCESSOR + rank], MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        sorted.insert(sorted.end(), arr, arr + sizes[i*PROCESSOR + rank]);
    }*/


    // END STEP 4 //

    // FINAL STEP //

    int lengths[PROCESSOR];

    MPI_Gather(&length, 1, MPI_INT, lengths, 1, MPI_INT, 0, MPI_COMM_WORLD);
    /*if(rank == 1)
    {
        std::cout<<lengths[rank]<<" - "<<std::endl;
        for(int i = 0; i < lengths[rank]; i++)
        {
            std::cout<<sorted[i]<<" - ";
        }
    }*/
    
    
    indexes[0] = 0;
    for(int i = 0; i < PROCESSOR - 1; i++)
    {
        indexes[i+1] = indexes[i] + lengths[0];
    }

    MPI_Gatherv(sorted.data(), length, MPI_INT, array.data(), lengths, indexes, MPI_INT, 0, MPI_COMM_WORLD);
        
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


std::vector<int> MergeSort (std::vector<int> nums, int rank) 
{
	auto amountperprocess = (int)std::round(N/PROCESSOR); 

	auto Arow = new int[amountperprocess];

	MPI_Scatter(nums.data(), amountperprocess, MPI_INT, Arow, amountperprocess, MPI_INT, 0, MPI_COMM_WORLD);

	std::sort(Arow, Arow + amountperprocess) ;
	MPI_Gather( Arow, amountperprocess, MPI_INT, nums.data(), amountperprocess, MPI_INT, 0, MPI_COMM_WORLD);

	if (rank == 0){

		for(int step=PROCESSOR;step>1;step/=2){
			for(int i=0;i<step;i+=2){
				std::inplace_merge(nums.begin()+i*N/step,nums.begin()+(i+1)*N/step,nums.begin()+(i+2)*N/step);
			}
		}

    	std::cout<<std::endl<<std::endl;
		auto print = [](const int& n) { std::cout << " - " << n; };
     	std::for_each(std::begin(nums), std::end(nums), print);
    	std::cout<<std::endl;
	}

    return nums;		
}