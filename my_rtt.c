/******************************************************************************
 * FILE: mpi_hello.c
 * DESCRIPTION:
 *   MPI tutorial example code: Simple hello world program
 * AUTHOR: Blaise Barney
 * LAST REVISED: 08/19/12
 * REVISED BY: Christopher Mauney
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <sys/time.h>
#include "my_mpi.h"

/* This is the root process */
#define  ROOT       0
/* Control number of iterations */
#define  NUM_ITER   12
/* Control number of iterations to skip */
#define  SKIP_ITER  2

int main (int argc, char *argv[])
{
    /* process information */
    int   numproc, rank, len;

    /* current process hostname */
    char  hostname[MPI_MAX_PROCESSOR_NAME];

    /* initialize MPI */
    MPI_Init(&argc, &argv);

    // /* get the number of procs in the comm */
    MPI_Comm_size(MPI_COMM_WORLD, &numproc);

    // /* get my rank in the comm */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // /* get some information about the host I'm running on */
    MPI_Get_processor_name(hostname, &len);


    /* buffers to send and recieve messages */
    char *sendmsg[17];
    char *recvmsg[17];

    /* structure to calculate time */
    struct timeval start,end;

    /* loop control variables */
    int i = 0;
    int j = 0;
    int size = 0;

    /* Iterate over each message size*/
    for(; i<= 16; i++){
        double avg_rtt = 0;
        double rtt[NUM_ITER];
        double stddev = 0;
        /* Calculate message size */
        size = 32 * (int)(pow(2,i) + 0.5);
        /* Allocate send buffer */
        sendmsg[i] = (char*)malloc(sizeof(char) * size);
        /* Initialize send buffer */
        memset(sendmsg[i],'z',size);
        /* Repeat sendrecv for each mesage size */
        for(j = 0; j < NUM_ITER; j++){
            /* MPI status variable */
            MPI_Status status;
            /* Allocate recieve buffer */
            recvmsg[i] = (char*)malloc(sizeof(char) * size);
            /* Get start time */
            gettimeofday(&start, NULL);
            if( rank%2 == 0){
                /* Send and Recieve message to rank+1 node for even ranked nodes */    
                MPI_Sendrecv(sendmsg[i],size,MPI_SIGNED_CHAR,rank+1,rank,
                        recvmsg[i],size,MPI_SIGNED_CHAR,rank+1,rank+1,
                        MPI_COMM_WORLD,&status);
            } else if(rank%2 != 0) {
                /* Get start time */
                gettimeofday(&start, NULL);
                /* Send and Recieve message to rank-1 node for odd ranked nodes */
                MPI_Sendrecv(sendmsg[i],size,MPI_SIGNED_CHAR,rank-1,rank,
                        recvmsg[i],size,MPI_SIGNED_CHAR,rank-1,rank-1,
                        MPI_COMM_WORLD,&status);
                        
            }
            /* Get end time */
            gettimeofday(&end, NULL);
            /* Calculate elapsed time */
            rtt[j] = (end.tv_sec - start.tv_sec);               // secs
            rtt[j] += (end.tv_usec - start.tv_usec)/1000000.0;   // us to secs
            /* Free recieve buffer */
            free(recvmsg[i]);
        }
        /* Calculate average rtt for NUM_ITER - SKIP_ITER iterations */
        for(int k = SKIP_ITER; k < NUM_ITER; k++){
            //printf("%d %d %f\n",rank,size,rtt[k]);
            avg_rtt += rtt[k];
        }
        avg_rtt = avg_rtt/(NUM_ITER - SKIP_ITER);

        /* Calculate std deviation for NUM_ITER - SKIP_ITER iterations */
        for(int k = SKIP_ITER; k < NUM_ITER; k++){
            stddev += pow((avg_rtt - rtt[k]),2);
        }
        stddev = stddev/(NUM_ITER - SKIP_ITER);
        stddev = sqrt(stddev);
        /* Free send buffer */
        free(sendmsg[i]);
        printf("%d %f %f\n",size,avg_rtt,stddev);

    }
    /* graceful exit */
    MPI_Finalize();
}
