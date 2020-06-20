/****************************************
* AUTHOR: Andre de Moeller              
* DATE: 24.03.20
* PURPOSE:                              
* LAST MODIFIED: 10.06.20
****************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "liftsim.h"
#include "request.h"

//global variables for shared memory
int BUFFER_SIZE;
int TIME;
int count = 0;
int done = 0;
int totalMovements = 0;
int totalRequests = 0;
Request* buffer;

//mutex lock and conds
pthread_mutex_t lock;
pthread_cond_t more = PTHREAD_COND_INITIALIZER;
pthread_cond_t less = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[])
{
    int error = 0, ii, lineCount = 0;
    pthread_t name[4];

    printf("\n\n-------------------------------------------------\n");
    printf("            LIFT SIMULATOR (Threads)           \n");
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
        if (atoi(argv[1]) < 1) 
        {
            printf("Error: buffer size must be >= 1\n");
            error++;
        }
        if (atoi(argv[2]) < 0) 
        {
            printf("Error: time must be >= 0\n");
            error++;
        }
        if(lineCount < 50 || lineCount > 100)
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

            //allocate memory for  buffer
            buffer = (Request*)malloc(BUFFER_SIZE * sizeof(Request));

            //initialise lock
            pthread_mutex_init(&lock, NULL);

            //create threads
            //liftR
            printf("Creating threads...\n\n");
            if (pthread_create(&(name[0]), NULL, request, NULL) != 0) 
            {
                fprintf(stderr, "Error: cannot create LiftR");
            }
            else 
            {
                //lift1-3
                for (ii = 1; ii < 4; ii++) 
                {
                    if (pthread_create(&(name[ii]), NULL, lift, (void*)0 + ii) != 0) 
                    {
                        fprintf(stderr, "Error: cannot create Lift%d\n", ii);
                    }
                }
            }

            //waits for threads to finish
            //liftR
            if (pthread_join(name[0], NULL) != 0) 
            {
                fprintf(stderr, "Error: cannot join LiftR\n");
            }
            else 
            {
                //lift1-3
                for (ii = 1; ii < 4; ii++) 
                {
                    if (pthread_join(name[ii], NULL) != 0) 
                    {
                        fprintf(stderr, "Error: cannot join Lift%d\n", ii);
                    }
                }
            }
            //add final information to file
            writeSummary(totalMovements, totalRequests);

            //free allocated memory
            free(buffer);

            //destory mutex
            pthread_mutex_destroy(&lock);

            printf("-------------------------------------------------\n");
            printf("	    File saved to: sim_out             \n");
            printf("-------------------------------------------------\n");
            printf("\n");
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
void* lift(void* num)
{
    Request request;
    int movement = 0, prev = 0, totalMovement = 0, reqNo = 0, complete = 0;

    while (complete == 0) 
    {
        //locked for shared memory (buffer and count)
        pthread_mutex_lock(&lock);
            
        //if no items are in the buffer
        while (count == 0 && done == 0) 
        {
            //put thread to sleep
            pthread_cond_wait(&more, &lock);
        }
        if (count > 0) 
        {
            //grab request from buffer
            request = dequeue();

            //works out movement for this request
            movement = abs(prev - request.origin) + abs(request.origin - request.destination);
    
            //summation of all previous movements
            totalMovement += movement;

            //for final output
            totalMovements += movement;
            totalRequests++;

            //increase request number
            reqNo++;

            //append request information to file
            writeOutput(request, (int)num, movement, reqNo, totalMovement, prev);

            //set new previous floor to current destination
            prev = request.destination;
        }
        if (done == 1 && count == 0) 
        {
            complete = 1;
        }

        //signals lift-r to read more requests into buffer since no longer full
        pthread_cond_broadcast(&less);

        //release lock
        pthread_mutex_unlock(&lock);

        //simulate time
        sleep(TIME);
    }
    return NULL;
}

/****************************************
* NAME: enqueue			               
* IMPORT: request                       
* EXPORT: none                          
* PURPOSE: adds reqiest to buffer       
****************************************/
void enqueue(Request request)
{
    //put new request at whatever the count currently is
    buffer[count] = request;

    //increase count
    count++;
}



/****************************************
* NAME: dequeue			                
* IMPORT: none                          
* EXPORT: request (at front of queue)  
* PURPOSE: removes request to buffer   
****************************************/
Request dequeue()
{
    Request request = buffer[0];

    //shuffle items down
    for (int ii = 0; ii < count - 1; ii++)
    {
        buffer[ii] = buffer[ii + 1];
    }
    //decrement count
    count--;

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
    int origin, destination;
    int error = 0;

    /*opens and reads sim_input as a file*/
    inputfile = fopen("sim_input", "r");

    /*checks if file eixsts*/
    if (inputfile != NULL) 
    {
        printf("Reading and writing requests...\n\n");
        do 
        {
            //creates new request
            Request request;

            /*scans a line in the file for a certain format*/
            fscanf(inputfile, "%d %d\n", &origin, &destination);

            //stores relevant information in request
            if (origin < 1 || destination < 1 || origin > 20 || destination > 20) 
            {
                printf("Error: origin and destination must be between 1-20\n");
                printf("\nEnding prematurely...\n\n");
                done = 1;
                pthread_mutex_unlock(&lock);
                error++;
            }
            else 
            {
                //lock enqueue function for count variable
                pthread_mutex_lock(&lock);

                //if the queue is full, put to sleep until avaliable spot
                while (count == BUFFER_SIZE) 
                {
                    pthread_cond_wait(&less, &lock);
                }

                //stores information in a struct
                request.origin = origin;
                request.destination = destination;

                //queue request struct
                enqueue(request);

                writeBuffer(origin, destination);

                if (feof(inputfile)) 
                {
                    done = 1;
                }
            
                //signal that a request has been read into the buffer for consumers
                pthread_cond_broadcast(&more);

                //release lock
                pthread_mutex_unlock(&lock);
            }
        } while ((!feof(inputfile) && error == 0));

        /*closes the file*/
        fclose(inputfile);
    }
    else 
    {
        /*checks if file is not found*/
        perror("Error");
    }
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
                        (int)num, prev, request.origin, request.destination,
                        prev, request.origin, request.origin, request.destination, movement, reqNo, totalMovement, request.destination);

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
