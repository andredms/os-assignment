/****************************************
* AUTHOR: Andre de Moeller              
* DATE: 24.03.20                        
* PURPOSE: implements processes         
* LAST MODIFIED: 10.06.20
****************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/wait.h>

#include "liftsim.h"
#include "request.h"
#include "memory.h"

//global variables used so that processes know names of shared memory
Memory* myMemory;
Request* buffer;

//semaphore and shared memory names
const char* shm_name = "/SHAREDMEMORY";
const char* shm_nameBuffer = "/BUFFER";
const char* sem_full = "/SEMFULL";
const char* sem_empty = "/SEMEMPTY";
const char* sem_mutex = "/SEMMUTEX";

//user-defined variables
int TIME;
int BUFFER_SIZE;

int main(int argc, char* argv[])
{
    int error = 0, shm_fd, status = 0, lineCount = 0;
    pid_t pid[2], ids[3] = { 0 };
    sem_t *full, *empty, *mutex;

    printf("\n-------------------------------------------------\n");
    printf("            LIFT SIMULATOR (Processes)           \n");
    printf("-------------------------------------------------\n\n");

    if (argc != 3)  
    {
        printf("USAGE INFORMATION:\n");
        printf("Run via ./liftsim <buffer_size> <time>\n");
    }
    else 
    {
        lineCount = countLines();
        //error checking
        if (atoi(argv[1]) < 1) {
        
            printf("Error: buffer size must be >= 1\n");
            error++;
        }
        if (atoi(argv[2]) < 0) 
        {
            printf("Error: time must be >= 0\n");
            error++;
        }
        if (lineCount < 50 || lineCount > 100)      
        {
            printf("Error: sim_input needs 50 - 100 requests\n\n");
            error++;
        }
        if (error == 0) 
        {
            //removes past sim_out file
            remove("sim_out");

            BUFFER_SIZE = atoi(argv[1]);
            TIME = atoi(argv[2]);

            //initialise semaphores
            full = sem_open(sem_full, O_CREAT, 0644, 0);
            empty = sem_open(sem_empty, O_CREAT, 0644, BUFFER_SIZE);
            mutex = sem_open(sem_mutex, O_CREAT, 0644, 1);

            //close semaphores as not needed in parent process
            sem_close(full);
            sem_close(empty);
            sem_close(mutex);

            //opens the shared memory for creation, sets the size and maps it
            shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
            ftruncate(shm_fd, sizeof(Memory));
            myMemory = (Memory*)mmap(NULL, sizeof(Memory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

            //creates shared buffer
            buffer = (Request*)mmap(NULL, BUFFER_SIZE * sizeof(Request), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

            //initialises default values for shared memory
            myMemory->count = 0;
            myMemory->done = 0; 
            myMemory->totalRequests = 0;
            myMemory->totalMovements = 0;

            printf("Creating process...\n\n");

            //two processes total
            pid[0] = fork();

            //four processes total
            pid[1] = fork();

            //lift 1
            if (pid[0] == 0 && pid[1] == 0) 
            {
                printf("    Lift1 started!\n");
                //actual pid grabbed here for waitpid
                ids[0] = getpid();
                lift(1);
                exit(0);
            }
            //lift 2
            else if (pid[0] == 0 & pid[1] > 0) 
            {
                printf("    Lift2 started!\n");
                //actual pid grabbed here for waitpid
                ids[1] = getpid();
                lift(2);
                exit(0);
            }
            //lift 3
            else if (pid[0] > 0 && pid[1] == 0) 
            {
                printf("    Lift3 started!\n");
                //actual pid grabbed here for waitpid
                ids[2] = getpid();
                lift(3);
                exit(0);
            }
            //parent process
            else if (pid[0] > 0 && pid[1] > 0) 
            {
                printf("    LiftR started!\n");
                request();
                printf("\nWaiting for children to terminate...\n");

                //wait for all child processes to end
                for (int ii = 0; ii < 3; ii++) {
                    waitpid(ids[ii], &status, 0);
                }
                //add final information to file
                writeSummary(myMemory->totalMovements, myMemory->totalRequests);

                printf("\n");
                printf("-------------------------------------------------\n");
                printf("            File saved to: sim_out\n");
                printf("-------------------------------------------------\n");
                printf("\n");
            }
            else 
            {
                //if any process IDs were less than 0
                printf("Error: process could not be created\n");
            }

            //unlink semaphores
            sem_unlink(sem_mutex);
            sem_unlink(sem_full);
            sem_unlink(sem_empty);
            
            //close shared memory 
            close(shm_fd);

            //unmap shared memory
            shm_unlink(shm_name);

            //unmap shared memory
            munmap(myMemory, sizeof(myMemory));

            //unmap shared buffer
            munmap(buffer, BUFFER_SIZE * sizeof(Request));
        }
    }
    return 0;
}

/****************************************
* NAME: lift (consumer)                
* IMPORT: lift number                   
* EXPORT: none                          
* PURPOSE: performs lift operation      
****************************************/
void* lift(int num)
{
    Request request;
    int movement = 0, prev = 0, totalMovement = 0, reqNo = 0, complete = 0, shm_fd;
    sem_t *empty, *full, *mutex;

    //open shared memory in process
    shm_fd = shm_open(shm_name, O_RDWR, 0666);

    //open semaphores
    full = sem_open(sem_full, O_RDWR);
    empty = sem_open(sem_empty, O_RDWR);
    mutex = sem_open(sem_mutex, O_RDWR);

    while (complete == 0) 
    {
        sem_wait(full);
        sem_wait(mutex);

        if (myMemory->count > 0)
        {
            //grab request from buffer
            request = dequeue();

            //works out movement for this request
            movement = abs(prev - request.origin) + abs(request.origin - request.destination);

            //summation of all previous movements
            totalMovement += movement;

            //for final output
            myMemory->totalMovements += movement;
            myMemory->totalRequests++;

            //increase request number
            reqNo++;

            //append request information to file
            writeOutput(request, (int)num, movement, reqNo, totalMovement, prev);

            //set new previous floor to current destination
            prev = request.destination;
        }
        if (myMemory->done == 1 && myMemory->count == 0) 
        {
            complete = 1;

    	    //post full on exit
            sem_post(full);
        }
        sem_post(mutex);
        sem_post(empty);

        sleep(TIME);
    }

    //closes semaphores
    sem_close(empty);
    sem_close(full);
    sem_close(mutex);

    //closes shared memory
    close(shm_fd);

    return NULL;
}

/****************************************
* NAME: enqueue			               
* IMPORT: request                       
* EXPORT: none                         
* PURPOSE: adds request to buffer       
****************************************/
void enqueue(Request request)
{
    //put new request at whatever the count currently is
    buffer[myMemory->count] = request;

    //increase count
    myMemory->count++;
}

/****************************************
* NAME: dequeue			             
* IMPORT: none                          
* EXPORT: request struct                          
* PURPOSE: removes request from buffer   
****************************************/
Request dequeue()
{
    Request request = buffer[0];

    //shuffle items down
    for (int ii = 0; ii < myMemory->count - 1; ii++) 
    {
        buffer[ii] = buffer[ii + 1];
    }
    //decrement count
    myMemory->count--;

    return request;
}

/****************************************
* NAME: request (producer)             
* IMPORT: none                          
* EXPORT: none                       
* PURPOSE: reads requests in from file  
****************************************/
void* request()
{
    FILE* inputfile;
    int origin, destination, error = 0, shm_fd;
    Request request;
    sem_t *empty, *full, *mutex;

    //open shared memory in process
    shm_fd = shm_open(shm_name, O_RDWR, 0666);

    /*opens and reads sim_input as a file*/
    inputfile = fopen("sim_input", "r");

    /*checks if file eixsts*/
    if (inputfile != NULL) 
    {
        printf("\nReading and writing requests...\n\n");

        //open semaphores
        full = sem_open(sem_full, O_RDWR);
        empty = sem_open(sem_empty, O_RDWR);
        mutex = sem_open(sem_mutex, O_RDWR);

        do 
        {
            sem_wait(empty);
            sem_wait(mutex);

            /*scans a line in the file for a certain format*/
            fscanf(inputfile, "%d %d\n", &origin, &destination);

            //stores relevant information in request
            if (origin < 1 || destination < 1 || origin > 20 || destination > 20) 
            {
                printf("\nError: origin and destination must be between 1-20\n");
                printf("\nEnding prematurely...\n");
                myMemory->done = 1;
                sem_post(mutex);
                sem_post(full);
                error++;
            }
            else 
            {
                //stores information in a struct
                request.origin = origin;
                request.destination = destination;

                //queue request struct
                enqueue(request);
 
                writeBuffer(origin, destination);

                if (feof(inputfile)) 
                {
                    myMemory->done = 1;
                }
            }
            sem_post(mutex);
            sem_post(full);
        } while ((!feof(inputfile)) && error == 0);

        //closes the file
        fclose(inputfile);

        //closes semaphores
        sem_close(full);
        sem_close(empty);
        sem_close(mutex);
    }
    else 
    {
        //checks if file wasn't found
        perror("Error");
    }

    //closes shared memory
    close(shm_fd);

    return NULL;
}

/****************************************
* NAME: writeOutput                    
* IMPORT: relevant lift inf             
* EXPORT: none                         
* PURPOSE: writes operation to file     
****************************************/
void writeOutput(Request request, int num, int movement, int reqNo, int totalMovement, int prev)
{
    FILE* outputfile;

    //opens file for appending
    outputfile = fopen("sim_out", "a");

    fprintf(outputfile, "Lift-%d Operation\nPrevious Position: Floor %d\nRequest: Floor %d to Floor %d\nDetail operations:\n"
                        "Go from: Floor %d to Floor %d\n    Go from: Floor %d to Floor %d\n    #Movement for this request: %d\n"
                        "    #Request: %d\n    Total #movement: %d\nCurrent position: %d\n\n",
                        (int)num, prev, request.origin, request.destination, prev, request.origin, request.origin, request.destination,     
                        movement, reqNo, totalMovement, request.destination);

    fclose(outputfile);
}

/****************************************
* NAME: writeBuffer                    
* IMPORT: origin, destination           
* EXPORT: none                         
* PURPOSE: writes request to file       
****************************************/
void writeBuffer(int origin, int destination)
{
    FILE* outputfile;

    //opens file for appending
    outputfile = fopen("sim_out", "a");

    fprintf(outputfile, "-------------------------------------------------\n"
                        "New lift request from floor %d to floor %d\n"
                        "-------------------------------------------------\n\n",
                        origin, destination);

    fclose(outputfile);
}

/****************************************
* NAME: writeSummary                   
* IMPORT: totalMovements, totalRequests          
* EXPORT: none                         
* PURPOSE: writes end of file summary      
****************************************/
void writeSummary(int totalMovements, int totalRequests)
{
    FILE* outputfile;

    //opens file for appending
    outputfile = fopen("sim_out", "a");

    fprintf(outputfile, "\nTotal number of requests: %d\nTotal number of movements: %d\n", totalRequests, totalMovements);

    fclose(outputfile);
}

/****************************************
* NAME: countLines                 
* IMPORT: none          
* EXPORT: line count          
* PURPOSE: checks if file is valid    
****************************************/
int countLines()
{
    FILE* input;
    char ch;
    int count = 0;
    input = fopen("sim_input", "r");

    do
    {
        ch = fgetc(input);
        if(ch == '\n')
        {
            count++;
        }
    }while(!feof(input));
    fclose(input);
    return count;
}
