#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include "my_mpi.h"

/* Node related arguments */
int numproc, np_length, nodefile_length, rank;
char *hostname, *nodefile;
bool read_flag;
char server_buffer[2097152];
/* Node Server file descriptor */
int sockfd;

/* Thread related intializations */
pthread_t thread_server;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


int MPI_Init(int *argc, char **argv[])
{
    /* Initialize variables from command line input */
    int i = 0;
    np_length = strlen((*argv)[1]);
    hostname = (char*)malloc(np_length * sizeof(char) + 1);
    memcpy(hostname, (*argv)[1], np_length);
    hostname[np_length]  = '\0';
    rank = atoi((*argv)[2]);
    numproc = atoi((*argv)[3]);
    nodefile_length = strlen((*argv)[4]) * sizeof(char);
    nodefile = (char*)malloc(nodefile_length + 1);
    memcpy(nodefile, (*argv)[4], nodefile_length);
    nodefile[nodefile_length] = '\0';

    /* Setup server */
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET,SO_REUSEPORT | SO_REUSEADDR, &enable, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORTNO);
    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 1);

    /* Call thread to start server */
    pthread_create(&thread_server, NULL, server, NULL);

    /* Wait for all nodes */
    MPI_Barrier(MPI_COMM_WORLD);
    return 0;
}

/* Initialize no. of processors */
int MPI_Comm_size(MPI_Comm temp, int *nproc)
{
    *nproc = numproc;
    return numproc;
}

/* Initialize rank of node */
int MPI_Comm_rank(MPI_Comm temp, int *rnk)
{
    *rnk = rank;
    return rank;
}

/* Initialize hostname of node */
int MPI_Get_processor_name(char *hstname, int *len)
{
    hstname = hostname;
    *len = np_length;
    return np_length;
}

/* Report socket error */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/* listen on server in background */
void *server(void *ptr)
{
    int newsockfd,n;
    char buffer[256];
    socklen_t clilen;
    struct sockaddr_in  cli_addr;
    while(1){
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,
                                (struct sockaddr *)&cli_addr,
                                &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        /* Read header message form client (SIZE|EXIT) */    
        int data_size;
        n = read(newsockfd, (char*)&data_size, sizeof(data_size));
        if (n < 0)
            error("ERROR reading from socket");    
        data_size = ntohl(data_size);
        
        /* Exit loop on EXIT Header */
        if (data_size  == INT_MAX)
            break;
        
        n = 0;
        /* Loop if data size is greater than tcp buffer */
        while( n < data_size)
        {
            n += read(newsockfd, server_buffer, data_size);
        }
        
        /* lock the mutex */
        pthread_mutex_lock(&mutex);
        /* set read flag as false */
        read_flag = true;
        /* signal main thread to wake up */
        pthread_cond_signal(&cond); 
        /* Unlock the mutex */
        pthread_mutex_unlock(&mutex);      
    }
    /* Close server */
    close(sockfd);
}

int MPI_Sendrecv(void *sendbuf, int sendcount, int send_elem_size, int reciever_rank,
            int send_tag, void * recvbuf, int recvcount, int recv_elem_size, int sender_rank,
            int recv_tag, MPI_Comm comm, int* status){

    int sockfd_client, total;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];
    char host_server[MPI_MAX_PROCESSOR_NAME];
    
    /* Calculate send and recive sizes */
    int send_size = sendcount * send_elem_size;
    int recv_size = recvcount * recv_elem_size;
    
    sockfd_client = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_client < 0)
        error("ERROR opening socket");
    
    /* Get server hostname based on rank */
    get_host_by_rank(reciever_rank,host_server); 

    server = gethostbyname(host_server);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(PORTNO);
    
    /* Try to connect to server */
    while (connect(sockfd_client, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        sleep(1);
    }
    
    total = 0;
    /* Send header to server (SIZE) */
    int datalen = htonl(send_size);
    total = write(sockfd_client,(char*)&datalen,sizeof(datalen));
    if(total < 0 ){
        error("Error writing header");
    }
    
    total = 0;
    /* Loop to send data to server */ 
   while(total < send_size)
   {
        total += write(sockfd_client,(char*)sendbuf,send_size);
    }

    /* lock the mutex */
    pthread_mutex_lock( &mutex );

    while(!read_flag)
    {
        /* block this thread until another thread signals cond.*/
        pthread_cond_wait( & cond, & mutex ); 
    }
    
    /* Copy from internal to recv buffer */
    memcpy(recvbuf,server_buffer,recv_size);
    /* Reset internal buffer */
    memset(server_buffer,0,sizeof(server_buffer));
    /* Reset read flag */
    read_flag = false;
    /* Unlock th emutex */
    pthread_mutex_unlock( & mutex );
    
    /* Close client */ 
    close(sockfd_client);    
    return 0;  
}

/* Barrier to synchronize nodes */
int MPI_Barrier(MPI_Comm comm)
{
    /* create a new server on rank 0 (root) */
    if (rank == 0)
    {   
        int sockfd_barrier;
        char buffer[256];
        int n, newsockfd[numproc - 1];
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        sockfd_barrier = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_barrier < 0)
            error("ERROR opening socket");
        int enable = 1;
        if (setsockopt(sockfd_barrier, SOL_SOCKET,SO_REUSEPORT | SO_REUSEADDR, &enable, sizeof(int)) < 0)
             error("setsockopt(SO_REUSEADDR) failed");
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(BARRIER_PORTNO);
        if (bind(sockfd_barrier, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) < 0)
            error("ERROR on binding");
        /* Server accepts connection from all other nodes */    
        listen(sockfd_barrier, numproc-1);

        /* Loop to read from all other nodes */
        for (int i = 0; i < numproc - 1; i++)
        {
            clilen = sizeof(cli_addr);
            newsockfd[i] = accept(sockfd_barrier,
                                  (struct sockaddr *)&cli_addr,
                                  &clilen);
            if (newsockfd[i] < 0)
                error("ERROR on accept");
            bzero(buffer, 256);
            /* read from each node */
            n = read(newsockfd[i], buffer, 255);
            if (n < 0)
                error("ERROR reading from socket");
            /* clear buffer */    
            buffer[0] = '\0';
        }
        /* Waits till all nodes have sent a message */
        sleep(1);
        /* Write back to each node */
        for (int i = 0; i < numproc - 1; i++)
        {
            /* Send message to each node */
            n = write(newsockfd[i], "GO", 2);
            if (n < 0)
                error("ERROR writing to socket");
            close(newsockfd[i]);
        }
        /* close server */
        close(sockfd_barrier);
    }
    /* If not root send message to root and wait to read message from root */
    else
    {
        int sockfd_client, n;
        struct sockaddr_in serv_addr;
        struct hostent *server;
        char root_server[MPI_MAX_PROCESSOR_NAME];    
        char buffer[256];
        
        sockfd_client = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_client < 0)
            error("ERROR opening socket");
        /* Get root hostname */    
        get_host_by_rank(0,root_server); 
        server = gethostbyname(root_server);
        if (server == NULL)
        {
            fprintf(stderr, "ERROR, no such host\n");
            exit(0);
        }
        
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr,
              (char *)&serv_addr.sin_addr.s_addr,
              server->h_length);
        serv_addr.sin_port = htons(BARRIER_PORTNO);
        
        while (connect(sockfd_client, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            sleep(1);
        }
        /* Initalize message */
        bzero(buffer, 256);
        strcpy(buffer, "Message from ");
        strcat(buffer,hostname);
        strcat(buffer,"\0");
        /* Send message to root */
        n = write(sockfd_client,buffer,strlen(buffer));
        if (n < 0)
            error("ERROR writing to socket");
        
        bzero(buffer, 256);
        /* Wait to read message from root */
        n = read(sockfd_client, buffer, 255);
        if (n < 0)
            error("ERROR reading from socket");
        /* close connection to root */
        close(sockfd_client);
    }
    return 0;
}

int MPI_Finalize()
{
    /* Wait for all nodes */
    MPI_Barrier(MPI_COMM_WORLD);
    sleep(2);
    /* Stop server on each node */
    stop_server();
    /* Wait for server thread */
    pthread_join(thread_server,NULL);
    free(hostname);
    free(nodefile);
    return 0;
}

/* Get hostname from nodefile based on rank */
void get_host_by_rank(int rank,char *host_server)
{
    FILE *fp;
    fp = fopen(nodefile, "r");
    if (fp == NULL)
    {
        exit(EXIT_FAILURE);
    }
    int count = 0;
    do{
        /* read a line from nodefile */
        fgets(host_server, MPI_MAX_PROCESSOR_NAME, fp);
        count++;
    }while(rank >= count);
    /* remove newline character fro mhostname */
    if(host_server[strlen(host_server) - 1] == '\n')
        host_server[strlen(host_server) - 1] = '\0';
    fclose(fp);    
}

/* Stop server on each node */
void stop_server(){
    int sockfd_client, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd_client = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_client < 0)
        error("ERROR opening socket");
    
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(PORTNO);
    
    while (connect(sockfd_client, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        sleep(1);
    }
    /* Send header message (EXIT) */
    int tmp = htonl(INT_MAX);
    n = write(sockfd_client,(char*)&tmp, sizeof(tmp));
    if (n < 0)
        error("ERROR writing to socket");
    /* Close connection */
    close(sockfd_client);
}