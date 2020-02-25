CC=mpicc

all : my_mpi

my_mpi:
	$(CC) -lm -lpthread -O3 -o my_rtt my_rtt.c my_mpi.c my_mpi.h

clean:
	rm my_rtt
