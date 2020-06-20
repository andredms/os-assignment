/****************************************
* AUTHOR: Andre de Moeller              
* DATE: 23.03.20                        
* PURPOSE: liftsim.c header file        
* LAST MODIFIED: 23.03.20               
****************************************/
#ifndef LIFTSIM_H
#define LIFTSIM_H

#include "request.h"

void* lift(int num);
void enqueue(Request request);
Request dequeue();
void* request();
void writeOutput(Request request, int num, int movement, int reqNo, int totalMovement, int prev);
void writeBuffer(int origin, int destination);
void writeSummary(int totalMovements, int totalRequests);
int countLines();

#endif
