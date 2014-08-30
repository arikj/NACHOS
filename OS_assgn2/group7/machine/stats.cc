// stats.h 
//	Routines for managing statistics about Nachos performance.
//
// DO NOT CHANGE -- these stats are maintained by the machine emulation.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "stats.h"
//----------------------------------------------------------------------
// Statistics::Statistics
// 	Initialize performance metrics to zero, at system startup.
//----------------------------------------------------------------------

Statistics::Statistics()
{
    TimerTicks = 100;	
    avgWaitTime = totalTicks = idleTicks = systemTicks = userTicks = cpuUsageTime = 0;
    minCpuBurst = maxCpuBurst = numBurst = numThreads = numDiskReads = numDiskWrites = 0;
    algoUsed = cpuErrorEst = numConsoleCharsRead = numConsoleCharsWritten = 0;
    numPageFaults = numPacketsSent = numPacketsRecvd = 0;
}

//----------------------------------------------------------------------
// Statistics::Print
// 	Print performance metrics, when we've finished everything
//	at system shutdown.
//----------------------------------------------------------------------

void
Statistics::Print()
{
    printf("Total files: %d\n", numThreads);	
    printf("Ticks: total %d, idle %d, system %d, user %d\n", totalTicks, 
	idleTicks, systemTicks, userTicks);
    printf("Disk I/O: reads %d, writes %d\n", numDiskReads, numDiskWrites);
    printf("Console I/O: reads %d, writes %d\n", numConsoleCharsRead, 
	numConsoleCharsWritten);
    printf("Paging: faults %d\n", numPageFaults);
    printf("Network I/O: packets received %d, sent %d\n", numPacketsRecvd, 
	numPacketsSent);
    printf("CPU usage Time: %d\n",cpuUsageTime);
    printf("Total simulation time: %d\n",totalTicks);		
    printf("CPU utilisation: %0.2f%\n", ((float)cpuUsageTime/totalTicks)*100);
    printf("Average waiting time in ready queue: %d\n", avgWaitTime/numThreads);	
    printf("Minimun CPU burst: %d\n", minCpuBurst);
    printf("Maximum CPU burst: %d\n", maxCpuBurst);
    printf("Average CPU burst: %d\n", cpuUsageTime/numBurst);	
    printf("Number of Non-Zero CPU bursts: %d\n",numBurst);
    
    int minThreadTime = threadTime[0];
    int maxThreadTime = threadTime[0];
    int totalThreadTime = threadTime[0];
	
    for(int i=1;i<numThreads;i++)
	{
	if(minThreadTime > threadTime[i])
		minThreadTime = threadTime[i];

	if(maxThreadTime < threadTime[i])
                maxThreadTime = threadTime[i];

	totalThreadTime +=threadTime[i];	
	}

    int mean = totalThreadTime/numThreads;
    int var = 0;

    for(int i=0;i<numThreads;i++)
	var += ((threadTime[i] - mean)*(threadTime[i] - mean))/numThreads;	
			
    printf("Minimum thread completion time: %d\n", minThreadTime);
    printf("Maximum thread completion time: %d\n", maxThreadTime);
    printf("Average thread completion time: %d\n", mean);
    printf("Variance of thread completion time: %d\n",var);	
    if(algoUsed == 2)
	{
        printf("Error in CPU burst estimation: %d\n", cpuErrorEst);
	printf("Ratio of Error in CPU burst estimation to total CPU bursts: %0.2f\n",((float)cpuErrorEst/cpuUsageTime));
	}

}
