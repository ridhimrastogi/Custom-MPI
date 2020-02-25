#include <string.h>

#define MPI_MAX_PROCESSOR_NAME 255
#define MPI_COMM_WORLD 0
#define PORTNO 3000
#define BARRIER_PORTNO 3001
#define MPI_SIGNED_CHAR 1

typedef int MPI_Status;
typedef int MPI_Comm;

int MPI_Init(int *, char ***);
int MPI_Comm_size(MPI_Comm, int *);
int MPI_Comm_rank(MPI_Comm, int *);
int MPI_Get_processor_name(char *, int *);
int MPI_Sendrecv(void *, int, int, int, int, void *, int, int, int, int,
                 MPI_Comm, int*);
int MPI_Finalize();
int MPI_Barrier(MPI_Comm);
void error(const char*);
void* server(void *);
void stop_server();
void get_host_by_rank(int rank,char *host_server);