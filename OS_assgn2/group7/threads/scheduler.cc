// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

#define FCFS 1 
#define SJF 2
#define RR 3
#define Unix 7

void Scheduler::setpreempt(){
	preempt = true;
}
//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    updateList = new List;	
    readyList = new List; 
    preempt = false;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete updateList;	
    delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread %s on ready list. pid : %d \n", thread->getName(), thread->GetPID());
    thread->setStatus(READY);
    if(schedalgo == SJF)
	readyList->SortedInsert((void *)thread, thread->predictBurst);
    else if(schedalgo >= UP1)
	readyList->SortedInsert((void *) thread, thread->currPriority);	
    else		
    	readyList->Append((void *)thread);
}


void
Scheduler::MoveToReady(Thread *thread)
{
	readyList->SortedInsert((void *) thread, thread->currPriority);
}


void Scheduler::ReadyToUpdate(Thread *thread)
{
	updateList->SortedInsert((void *) thread, thread->currPriority);
}
//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    return (Thread *)readyList->Remove();
}


Thread *
Scheduler::FindNextToUpdate()
{
    return (Thread *)updateList->Remove();
}

void Scheduler::topInsert(Thread *thread)
{
	readyList->Prepend((void *) thread);
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

	//Print();

   /*  if(schedalgo == SJF)
	{	
	oldThread->setActualFinal();
	//nextThread->setActualInitial();
	}


	stats->cpuUsageTime= stats->cpuUsageTime + stats->totalTicks;
	nextThread->waitingTime += stats->totalTicks;	
	DEBUG('c',"Final ticks = %d cpu = %d\n", stats->totalTicks, stats->cpuUsageTime);
*/	
	_SWITCH(oldThread, nextThread);
	
	//nextThread->setActualInitial();
  //  	stats->cpuUsageTime= stats->cpuUsageTime - stats->totalTicks;
	DEBUG('c',"Initial ticks = %d cpu = %d\n", stats->totalTicks, stats->cpuUsageTime);
/*	if(schedalgo == RR)
		nextQuanta = stats->totalTicks + quantaTicks;
	
	else if(schedalgo == SJF)
		nextThread->setActualInitial();
*/
    DEBUG('t', "Now in thread \"%s\" pid: %d \n", currentThread->getName(), currentThread->GetPID());
    	

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreState();
    }
#endif

}

//----------------------------------------------------------------------
// Scheduler::Tail
//      This is the portion of Scheduler::Run after _SWITCH(). This needs
//      to be executed in the startup function used in fork().
//----------------------------------------------------------------------

void
Scheduler::Tail ()
{
    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!


    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
        threadToBeDestroyed = NULL;
    }

#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {         // if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    readyList->Mapcar((VoidFunctionPtr) ThreadPrint);
}
