#include "mpi.h"
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <cmath>

const int N=7;
const int RANGE=100;

// mpirun -host localhost -np 4 ./mpi
int main (int argc, char *argv[]) {
	MPI_Init (&argc, &argv); // Initialize MPI envirnmnt
	int rank, size, namelen;
	char name[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_rank (MPI_COMM_WORLD, &rank); // ID of current process
	MPI_Get_processor_name (name, &namelen); // Hostname of node
	MPI_Comm_size (MPI_COMM_WORLD, &size); // Number of processes
	//std::cout<<"Hello World from rank "<<rank<<" running on"<<name<<"!\n";
	//if (rank == 0) std::cout<<"MPI World size = "<<size<<" processes\n";

	auto amountperprocess = (int)std::round(N/size); 

	//for clients (and master)
	//auto B= new int[N][N];
	auto Arow= new int[amountperprocess];
	std::vector<int> nums(N);
	
	if (rank == 0){
    	std::mt19937 rng; // default constructed, seeded with fixed seed
    	std::generate(nums.begin(),nums.end(),[&](){return rng()%RANGE;});


		/*for(int i=1;i<size;i++){
	    	MPI_Send(nums.data()+i*N/size, N/size, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		std::sort(nums.begin(),nums.begin()+N/size);
		for(int i=1;i<size;i++){
	    	MPI_Recv(nums.data()+i*N/size, N/size, MPI_INT, i, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);
		}


		for(int step=size;step>1;step/=2){
			for(int i=0;i<step;i+=2){
				std::inplace_merge(nums.begin()+i*N/step,nums.begin()+(i+1)*N/step,nums.begin()+(i+2)*N/step);
			}
		}*/

	}

	MPI_Scatter(nums.data(), amountperprocess, MPI_INT, Arow, amountperprocess, MPI_INT, 0, MPI_COMM_WORLD);

	std::sort(Arow, Arow + amountperprocess) ;
	MPI_Gather( Arow, amountperprocess, MPI_INT, nums.data(), amountperprocess, MPI_INT, 0, MPI_COMM_WORLD);

	if (rank == 0){

		for(int step=size;step>1;step/=2){
			for(int i=0;i<step;i+=2){
				std::inplace_merge(nums.begin()+i*N/step,nums.begin()+(i+1)*N/step,nums.begin()+(i+2)*N/step);
			}
		}

		/*if(amountperprocess % 2 != 0)
		{
			std::sort(nums.begin(), nums.end());
		}*/

		auto print = [](const int& n) { std::cout << " " << n; };
     	std::for_each(std::begin(nums), std::end(nums), print);
    	std::cout<<std::endl;

	}		

	MPI_Finalize (); // Terminate MPI environment
}
