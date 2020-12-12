#include "mpi.h"
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <chrono>  
#include <cmath>
#include <queue>
#include <ctime>

int N = 1000;
int PROCESSOR = 8;

std::vector<int> PSRS(std::vector<int> array, int rank);
std::vector<int> MergeSort(std::vector<int> nums, int rank);

std::vector<int> RandomVector(int size, int Range)
{
    std::vector<int> array(size);
    std::mt19937 rng;
    for(int i = 0; i < size; i++)
    {
        rng.seed(clock());
        std::generate(array.begin(),array.end(),[&](){return rng()%Range;});
    }
    return array;
}

std::vector<int> SortedVector(int size)
{
    std::vector<int> array(size);
    for(int i = 0; i < size; i++)
    {
        array[i] = i;
    }
    return array;
}

std::vector<int> BackwardsVector(int size)
{
    std::vector<int> array(size);
    for(int i = 0; i < size; i++)
    {
        array[i] = size-i;
    }
    return array;
}



int main(int argc, char *argv[])
{
    MPI_Init (&argc, &argv); // Initialize MPI envirnmnt
	int rank, namelen;
	char name[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_rank (MPI_COMM_WORLD, &rank); // ID of current process
	MPI_Get_processor_name (name, &namelen); // Hostname of node
	MPI_Comm_size (MPI_COMM_WORLD, &PROCESSOR); // Number of processes

    N = atoi(argv[1]);
    int order = atoi(argv[2]);
    
    std::vector<int> array(N);
    std::vector<int> mergeArray(N);


    if(rank == 0)
    {
        switch (order)
        {
            case 1: array = SortedVector(N);
            break;
            case 2: array = BackwardsVector(N);
            break;
            default: array = RandomVector(N,N*100);
            break;
        }
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    //MPI_Bcast(array.data(), N, MPI_INT, 0, MPI_COMM_WORLD);
    array = PSRS(array, rank);

    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-startTime).count();


    if(rank == 0)
    {
        
        for(int i = 1; i < N; i++)
        {
            if(array[i-1] > array[i])
            {
                std::cout<<" not ordered " <<std::endl; 
                break;
            }
            
        }
        std::cout<<"Averege elapsed time: "<< (time/1000000.0f)<< " ms" <<std::endl<<std::endl;
    }

    //std::cout<<"Averege elapsed time: "<< (time/100) <<std::endl;


    if(rank == 0)
    {
        switch (order)
        {
            case 1: mergeArray = SortedVector(N);
            break;
            case 2: mergeArray = BackwardsVector(N);
            break;
            default: mergeArray = RandomVector(N,N*100);
            break;
        }
    }
    //MPI_Bcast(mergeArray.data(), N, MPI_INT, 0, MPI_COMM_WORLD);

    startTime  = std::chrono::high_resolution_clock::now();
    mergeArray = MergeSort(mergeArray, rank);


    time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-startTime).count();

    if(rank == 0)
    {
        /*
        for(int i = 1; i < N; i++)
        {
            if(mergeArray[i-1] > mergeArray[i])
            {
                std::cout<<" not ordered " <<std::endl; 
                break;
            }
            
        }*/
        std::cout<<"Averege elapsed time: "<< (time/1000000.0f)<< "ms" <<std::endl<<std::endl;
    }

    int wait = 0;
    MPI_Bcast(&wait, 1, MPI_INT, 0, MPI_COMM_WORLD); 
    MPI_Finalize();

}


std::vector<int> PSRS(std::vector<int> array, int rank)
{
    int size = N/PROCESSOR;

    //int *toSort = new int[size]; // -> disjointed vector where the sorting will be performed
    std::vector<int> toSort(size);
    // BEGIN STEP 1 //  
    //Distribute array betwen processors
    MPI_Scatter(array.data(), size, MPI_INT, toSort.data(), size, MPI_INT, 0, MPI_COMM_WORLD);    
    //QuickSort(toSort, 0, size - 1); //-> apply quicksort in all processors
    std::sort(toSort.begin(), toSort.end());
    // END STEP 1 //

    // BEGIN STEP 2 //
    int processorPivots[PROCESSOR]; // -> pivots of each processor array
    //int candidatePivots[PROCESSOR*PROCESSOR]; // -> pivots of all processors
    std::vector<int> candidatePivots(PROCESSOR*PROCESSOR);
    int pivots[PROCESSOR - 1]; // -> selected pivots to implement sort

    // select pivots in each processor
    for(int i = 0; i < PROCESSOR; i++)
    {
        processorPivots[i] = toSort[i*size/PROCESSOR];
    }
    //rank 0 collect all the pivots into candidate pivots
    MPI_Gather(processorPivots, PROCESSOR, MPI_INT, candidatePivots.data(), PROCESSOR, MPI_INT, 0, MPI_COMM_WORLD);


    if(rank == 0)
    {        
        //order candiate pivots
        //QuickSort(candidatePivots, 0, PROCESSOR*PROCESSOR - 1);
        std::sort(candidatePivots.begin(),candidatePivots.end());
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
    int indexes[PROCESSOR];
    //store sizes of each rank data packs
    int sizes[PROCESSOR];

    int j = 0;// -> keeps track of actual pivot
    //search for partition using selected pivot
    for(int i = 0; i < size; i++)
    {
        if(toSort[i] < pivots[j]) continue;//-> partition only if number > pivot
        indexes[j] = lastIndex;//-> store prev partition index, first index must be 0
        lastIndex = i;// -> actualize last partition
        sizes[j] = lastIndex - indexes[j]; // -> data pack size goes from previous partition to current partition
        j++; //-> actualize pivot
        if(j >= PROCESSOR - 1) break;// -> if last pivot checked break;
    }
    indexes[j] = lastIndex; // -> set last value
    sizes[j] = size - indexes[j];// -> set last value

    //each rank inform of its partition data to the other ranksint *recvRankPartLen = new int[comm_sz];
    int *rankRcvLen = new int[PROCESSOR];
    MPI_Alltoall(sizes, 1, MPI_INT, rankRcvLen, 1, MPI_INT, MPI_COMM_WORLD);

    int rankRcvLength = 0;
    int *rankStartIndexes = new int[PROCESSOR];
    for(int i=0; i<PROCESSOR; i++) {
        rankStartIndexes[i] = rankRcvLength;
        rankRcvLength += rankRcvLen[i];
    }
   
    // END STEP 3 //
    // BEGIN STEP 4 //

    //int *sorted = new int[rankRcvLength];
    std::vector<int> sorted(rankRcvLength);

    MPI_Alltoallv(toSort.data(), sizes, indexes, MPI_INT, sorted.data(), rankRcvLen, rankStartIndexes, MPI_INT, MPI_COMM_WORLD);

    //QuickSort(sorted.data(), 0, rankRcvLength - 1);
    std::sort(sorted.begin(), sorted.end());
    
    // END STEP 4 //
    // FINAL STEP //

    int lengths[PROCESSOR];

    MPI_Gather(&rankRcvLength, 1, MPI_INT, lengths, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    indexes[0] = 0;
    for(int i = 0; i < PROCESSOR - 1; i++)
    {
        indexes[i+1] = indexes[i] + lengths[0];
    }

    //MPI_Gatherv(sorted.data(), rankRcvLength, MPI_INT, array.data(), lengths, indexes, MPI_INT, 0, MPI_COMM_WORLD);

    if(rank != 0)
    {
        MPI_Send(sorted.data(), rankRcvLength, MPI_INT, 0, rank, MPI_COMM_WORLD);
    }

    if(rank == 0)
    {
        std::vector<int> result;
	    MPI_Status status;
        result.insert(result.end(),sorted.begin(),sorted.end());

        for(int i = 0; i < PROCESSOR; i++)
        {
            if(i == 0) continue;
            MPI_Probe(i, i, MPI_COMM_WORLD, &status);

            int count;
            MPI_Get_count(&status, MPI_INT, &count);

            int *aux = new int[count];
            
		    MPI_Recv(aux, count, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            result.insert(result.end(),aux,aux+count); 
        }
        return result;
    }
        
    return array;
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
	}

    return nums;		
}