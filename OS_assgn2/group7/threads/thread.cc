// thread.cc 
//	Routines to manage threads.  There are four main operations:
//
//	Fork -- create a thread to run a procedure concurrently
//		with the caller (this is done in two steps -- first
//		allocate the Thread object, then call Fork on it)
//	Finish -- called when the forked procedure finishes, to clean up
//	Yield -- relinquish control over the CPU to another ready thread
//	Sleep -- relinquish control over the CPU, but thread is now blocked.
//		In other words, it will not run again, until explicitly 
//		put back on the ready queue.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "thread.h"
#include "switch.h"
#include "synch.h"
#include "system.h"

#define STACK_FENCEPOST 0xdeadbeef	// this is put at the top of the
					// execution stack, for detecting 
					// stack overflows
//----------------------------------------------------------------------
// Thread::Thread
// 	Initialize a thread control block, so that we can then call
//	Thread::Fork.
//
//	"threadName" is an arbitrary string, useful for debugging.
//----------------------------------------------------------------------

Thread::Thread(char* threadName)
{
    int i;

    name = threadName;
    stackTop = NULL;
    stack = NULL;
    status = JUST_CREATED;
    waitingTime = 0;	
#ifdef USER_PROGRAM
    space = NULL;
#endif
    
    startTime = stats->totalTicks; 
    predictBurst = 0;
    lastBurst = 0;
    unixCpu = 0;
    lastWait = 0;
    cpuTime = 0;
    basePriority = 0;
    currPriority = 0;
    threadArray[thread_index] = this;
    pid = thread_index;
    thread_index++;
    DEBUG('n',"thread_index=%d\n",thread_index);
    ASSERT(thread_index < MAX_THREAD_COUNT);
    if (currentThread != NULL) {
       ppid = currentThread->GetPID();
       currentThread->RegisterNewChild (pid);
    }
    else ppid = -1;

    childcount = 0;
    waitchild_id = -1;

    for (i=0; i<MAX_CHILD_COUNT; i++) exitedChild[i] = false;
}

//----------------------------------------------------------------------
// Thread::~Thread
// 	De-allocate a thread.
//
// 	NOTE: the current thread *cannot* delete itself directly,
//	since it is still running on the stack that we need to delete.
//
//      NOTE: if this is the main thread, we can't delete the stack
//      because we didn't allocate it -- we got it automatically
//      as part of starting up Nachos.
//----------------------------------------------------------------------

Thread::~Thread()
{
    DEBUG('t', "Deleting thread \"%s\"\n", name);

    ASSERT(this != currentThread);
    if (stack != NULL)
	DeallocBoundedArray((char *) stack, StackSize * sizeof(int));
}


//----------------------------------------------------------------------
// Thread::Fork
// 	Invoke (*func)(arg), allowing caller and callee to execute 
//	concurrently.
//
//	NOTE: although our definition allows only a single integer argument
//	to be passed to the procedure, it is possible to pass multiple
//	arguments by making them fields of a structure, and passing a pointer
//	to the structure as "arg".
//
// 	Implemented as the following steps:
//		1. Allocate a stack
//		2. Initialize the stack so that a call to SWITCH will
//		cause it to run the procedure
//		3. Put the thread on the ready queue
// 	
//	"func" is the procedure to run concurrently.
//	"arg" is a single argument to be passed to the procedure.
//----------------------------------------------------------------------

/*void
Thread::setActualInitial()
{
	actual = stats->totalTicks;
	//DEBUG('z',"Setting actual initial for %s: %u\n", name, actual);
	DEBUG('z',"\t\tSetting for %d: actual initial = %u\t\n", pid, actual);
}


void
Thread::setActualFinal()
{
	actual = stats->totalTicks-actual;
	predict = .5*actual+.5*predict;
	DEBUG('z',"\t\tSetting for %d: actual final = %u\t predict =%u\n", pid, actual, predict);
}
*/

void 
Thread::Fork(VoidFunctionPtr func, int arg)
{
    DEBUG('t', "Forking thread \"%s\" with func = 0x%x, arg = %d\n",
	  name, (int) func, arg);
    
    StackAllocate(func, arg);

    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts 
					// are disabled!
    (void) interrupt->SetLevel(oldLevel);
}    

//----------------------------------------------------------------------
// Thread::CheckOverflow
// 	Check a thread's stack to see if it has overrun the space
//	that has been allocated for it.  If we had a smarter compiler,
//	we wouldn't need to worry about this, but we don't.
//
// 	NOTE: Nachos will not catch all stack overflow conditions.
//	In other words, your program may still crash because of an overflow.
//
// 	If you get bizarre results (such as seg faults where there is no code)
// 	then you *may* need to increase the stack size.  You can avoid stack
// 	overflows by not putting large data structures on the stack.
// 	Don't do this: void foo() { int bigArray[10000]; ... }
//----------------------------------------------------------------------

void
Thread::CheckOverflow()
{
    if (stack != NULL)
#ifdef HOST_SNAKE			// Stacks grow upward on the Snakes
	ASSERT(stack[StackSize - 1] == STACK_FENCEPOST);
#else
	ASSERT(*stack == STACK_FENCEPOST);
#endif
}

//----------------------------------------------------------------------
// Thread::Finish
// 	Called by ThreadRoot when a thread is done executing the 
//	forked procedure.
//
// 	NOTE: we don't immediately de-allocate the thread data structure 
//	or the execution stack, because we're still running in the thread 
//	and we're still on the stack!  Instead, we set "threadToBeDestroyed", 
//	so that Scheduler::Run() will call the destructor, once we're
//	running in the context of a different thread.
//
// 	NOTE: we disable interrupts, so that we don't get a time slice 
//	between setting threadToBeDestroyed, and going to sleep.
//----------------------------------------------------------------------

//
void
Thread::Finish ()
{
    (void) interrupt->SetLevel(IntOff);		
    ASSERT(this == currentThread);
    
    DEBUG('t', "Finishing thread \"%s\"\n", getName());
    
    threadToBeDestroyed = currentThread;
    Sleep();					// invokes SWITCH
    // not reached
}

//----------------------------------------------------------------------
// Thread::SetChildExitCode
//      Called by an exiting thread on parent's thread object.
//----------------------------------------------------------------------

void
Thread::SetChildExitCode (int childpid, int ecode)
{
   unsigned i;

   // Find out which child
   for (i=0; i<childcount; i++) {
      if (childpid == childpidArray[i]) break;
   }

   ASSERT(i < childcount);
   childexitcode[i] = ecode;
   exitedChild[i] = true;

   if (waitchild_id == (int)i) {
      waitchild_id = -1;
      // I will wake myself up
      IntStatus oldLevel = interrupt->SetLevel(IntOff);
      scheduler->ReadyToRun(this);
      (void) interrupt->SetLevel(oldLevel);
   }
}

//----------------------------------------------------------------------
// Thread::Exit
//      Called by ExceptionHandler when a thread calls Exit.
//      The argument specifies if all threads have called Exit, in which
//      case, the simulation should be terminated.
//----------------------------------------------------------------------

void
Thread::Exit (bool terminateSim, int exitcode)
{
    (void) interrupt->SetLevel(IntOff);
    ASSERT(this == currentThread);

    DEBUG('t', "Finishing thread \"%s\"\n", getName());

    	
    threadToBeDestroyed = currentThread;

    Thread *nextThread;

    setStatus(BLOCKED);
    endTime = stats->totalTicks;
    stats->threadTime[(stats->numThreads)++] = endTime;
    
    printf("Waiting time: %d   CPU usage Time: %d\n",waitingTime, cpuTime);
    // Set exit code in parent's structure provided the parent hasn't exited
    if (ppid != -1) {
       ASSERT(threadArray[ppid] != NULL);
       if (!exitThreadArray[ppid]) {
          threadArray[ppid]->SetChildExitCode (pid, exitcode);
       }
    }

    while ((nextThread = scheduler->FindNextToRun()) == NULL) {
        if (terminateSim) {
           DEBUG('i', "Machine idle.  No interrupts to do.\n");
           printf("\nNo threads ready or runnable, and no pending interrupts.\n");
           printf("Assuming all programs completed.\n");
           interrupt->Halt();
        }
        else interrupt->Idle();      // no one to run, wait for an interrupt
    }
    scheduler->Run(nextThread); // returns when we've been signalled
}

//----------------------------------------------------------------------
// Thread::Yield
// 	Relinquish the CPU if any other thread is ready to run.
//	If so, put the thread on the end of the ready list, so that
//	it will eventually be re-scheduled.
//
//	NOTE: returns immediately if no other thread on the ready queue.
//	Otherwise returns when the thread eventually works its way
//	to the front of the ready list and gets re-scheduled.
//
//	NOTE: we disable interrupts, so that looking at the thread
//	on the front of the ready list, and switching to it, can be done
//	atomically.  On return, we re-set the interrupt level to its
//	original state, in case we are called with interrupts disabled. 
//
// 	Similar to Thread::Sleep(), but a little different.
//----------------------------------------------------------------------

void
Thread::Yield ()
{
    Thread *nextThread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    
    ASSERT(this == currentThread);
    
    DEBUG('t', "Yielding thread \"%s\"\n", getName());
    if(schedalgo >= UP1)
	{
	setStatus(READY);
	nextThread = scheduler->FindNextToRun();
	if(nextThread == NULL)
		setStatus(RUNNING);

	else
		{

		if(currPriority < nextThread->currPriority)
			{
			setStatus(RUNNING);
			scheduler->topInsert(nextThread);
			}

		else
			{
			scheduler->ReadyToRun(this);
			scheduler->Run(nextThread);
			}
		}
	}
	
    else
	{
	    nextThread = scheduler->FindNextToRun();
	    if (nextThread != NULL) {
		scheduler->ReadyToRun(this);
		scheduler->Run(nextThread);
	    }
	}
  (void) interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// Thread::Sleep
// 	Relinquish the CPU, because the current thread is blocked
//	waiting on a synchronization variable (Semaphore, Lock, or Condition).
//	Eventually, some thread will wake this thread up, and put it
//	back on the ready queue, so that it can be re-scheduled.
//
//	NOTE: if there are no threads on the ready queue, that means
//	we have no thread to run.  "Interrupt::Idle" is called
//	to signify that we should idle the CPU until the next I/O interrupt
//	occurs (the only thing that could cause a thread to become
//	ready to run).
//
//	NOTE: we assume interrupts are already disabled, because it
//	is called from the synchronization routines which must
//	disable interrupts for atomicity.   We need interrupts off 
//	so that there can't be a time slice between pulling the first thread
//	off the ready list, and switching to it.
//----------------------------------------------------------------------
void
Thread::Sleep ()
{
    Thread *nextThread;
    
    ASSERT(this == currentThread);
    ASSERT(interrupt->getLevel() == IntOff);
    
    DEBUG('t', "Sleeping thread \"%s\"\n", getName());

    setStatus(BLOCKED);
    while ((nextThread = scheduler->FindNextToRun()) == NULL) {
	interrupt->Idle();	// no one to run, wait for an interrupt
    }
        
    scheduler->Run(nextThread); // returns when we've been signalled
}

//----------------------------------------------------------------------
// ThreadFinish, InterruptEnable, ThreadPrint
//	Dummy functions because C++ does not allow a pointer to a member
//	function.  So in order to do this, we create a dummy C function
//	(which we can pass a pointer to), that then simply calls the 
//	member function.
//----------------------------------------------------------------------

static void ThreadFinish()    { currentThread->Finish(); }
static void InterruptEnable() { interrupt->Enable(); }
void ThreadPrint(int arg){ Thread *t = (Thread *)arg; t->Print(); }

//----------------------------------------------------------------------
// Thread::StackAllocate
//	Allocate and initialize an execution stack.  The stack is
//	initialized with an initial stack frame for ThreadRoot, which:
//		enables interrupts
//		calls (*func)(arg)
//		calls Thread::Finish
//
//	"func" is the procedure to be forked
//	"arg" is the parameter to be passed to the procedure
//----------------------------------------------------------------------

void
Thread::StackAllocate (VoidFunctionPtr func, int arg)
{
    stack = (int *) AllocBoundedArray(StackSize * sizeof(int));

#ifdef HOST_SNAKE
    // HP stack works from low addresses to high addresses
    stackTop = stack + 16;	// HP requires 64-byte frame marker
    stack[StackSize - 1] = STACK_FENCEPOST;
#else
    // i386 & MIPS & SPARC stack works from high addresses to low addresses
#ifdef HOST_SPARC
    // SPARC stack must contains at least 1 activation record to start with.
    stackTop = stack + StackSize - 96;
#else  // HOST_MIPS  || HOST_i386
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
#ifdef HOST_i386
    // the 80386 passes the return address on the stack.  In order for
    // SWITCH() to go to ThreadRoot when we switch to this thread, the
    // return addres used in SWITCH() must be the starting address of
    // ThreadRoot.
    *(--stackTop) = (int)_ThreadRoot;
#endif
#endif  // HOST_SPARC
    *stack = STACK_FENCEPOST;
#endif  // HOST_SNAKE
    
    machineState[PCState] = (int) _ThreadRoot;
    machineState[StartupPCState] = (int) InterruptEnable;
    machineState[InitialPCState] = (int) func;
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = (int) ThreadFinish;
}

#ifdef USER_PROGRAM
#include "machine.h"

//----------------------------------------------------------------------
// Thread::SaveUserState
//	Save the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine saves the former.
//----------------------------------------------------------------------

void
Thread::SaveUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	userRegisters[i] = machine->ReadRegister(i);
}

//----------------------------------------------------------------------
// Thread::RestoreUserState
//	Restore the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine restores the former.
//----------------------------------------------------------------------

void
Thread::RestoreUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, userRegisters[i]);
}



void 
Thread::setStatus(ThreadStatus st) 
{
	if(status == RUNNING && st!=RUNNING)
		{
		lastBurst = stats->totalTicks - lastBurst;
		stats->cpuUsageTime +=lastBurst;
		cpuTime +=lastBurst;
		DEBUG('h',"CPU Time: %d\n",stats->cpuUsageTime);

		if(lastBurst!=0)
			{
			stats->numBurst +=1;
			if(stats->minCpuBurst == 0)
				stats->minCpuBurst = lastBurst;
			else
				stats->minCpuBurst = (stats->minCpuBurst > lastBurst) ? lastBurst : stats->minCpuBurst;
			
			stats->maxCpuBurst = (stats->maxCpuBurst < lastBurst) ? lastBurst : stats->maxCpuBurst;
			}

		if(schedalgo == SJF && lastBurst!=0)
			{
			DEBUG('b',"Predict: %d\t Actual: %d\n",predictBurst, lastBurst);
			if(predictBurst > lastBurst)
				stats->cpuErrorEst += (predictBurst - lastBurst);
			else
				stats->cpuErrorEst += (lastBurst - predictBurst);
 
			predictBurst = 0.5*lastBurst + 0.5*predictBurst;
			}
			

		if(schedalgo >= UP1 && lastBurst!=0)
			{
			unixCpu = (unixCpu + lastBurst)/2;
			currPriority = basePriority + unixCpu/2;
				
			Thread *updateThread;
			while((updateThread = scheduler->FindNextToRun()) != NULL)
				{
				updateThread->unixCpu = (updateThread->unixCpu)/2;
				updateThread->currPriority = updateThread->basePriority + (updateThread->unixCpu)/2;
			        scheduler->ReadyToUpdate(updateThread);	
				}

			while((updateThread = scheduler->FindNextToUpdate()) != NULL)
				scheduler->MoveToReady(updateThread);

			TimeSortedWaitQueue *sleepThread;
                        sleepThread = sleepQueueHead;
                        while (sleepThread != NULL)
                               	{
                                sleepThread->GetThread()->unixCpu /= 2;
                                sleepThread->GetThread()->currPriority = sleepThread->GetThread()->basePriority + (sleepThread->GetThread()->unixCpu)/2;
                                sleepThread = sleepThread->GetNext();
                                }
                        	
				
			} 
		}

	else if(st == RUNNING)
		{
		lastBurst = stats->totalTicks;
		nextQuanta = stats->totalTicks + quantaTicks; 		
		} 

	if(st == READY){
		lastWait =  stats->totalTicks;		
		}

	else if(st == RUNNING && status == READY)
		{
		lastWait = stats->totalTicks - lastWait;
		waitingTime  = waitingTime + lastWait;
		stats->avgWaitTime = stats->avgWaitTime + lastWait; 	
		}
	
	status = st; 
}

//----------------------------------------------------------------------
// Thread::CheckIfChild
//      Checks if the passed pid belongs to a child of mine.
//      Returns child id if all is fine; otherwise returns -1.
//----------------------------------------------------------------------

int
Thread::CheckIfChild (int childpid)
{
   unsigned i;

   // Find out which child
   for (i=0; i<childcount; i++) {
      if (childpid == childpidArray[i]) break;
   }

   if (i == childcount) return -1;
   return i;
}

//----------------------------------------------------------------------
// Thread::JoinWithChild
//      Called by a thread as a result of SC_Join.
//      Returns the exit code of the child being joined with.
//----------------------------------------------------------------------

int
Thread::JoinWithChild (int whichchild)
{
   // Has the child exited?
   if (!exitedChild[whichchild]) {
      // Put myself to sleep
      waitchild_id = whichchild;
      IntStatus oldLevel = interrupt->SetLevel(IntOff);
      printf("[pid %d] Before sleep in JoinWithChild.\n", pid);
      Sleep();
      printf("[pid %d] After sleep in JoinWithChild.\n", pid);
      (void) interrupt->SetLevel(oldLevel);
   }
   return childexitcode[whichchild];
}

//----------------------------------------------------------------------
// Thread::ResetReturnValue
//      Sets the syscall return value to zero. Used to set the return
//      value of SC_Fork in the created child.
//----------------------------------------------------------------------

void
Thread::ResetReturnValue ()
{
   userRegisters[2] = 0;
}

//----------------------------------------------------------------------
// Thread::Schedule
//      Enqueues the thread in the ready queue.
//----------------------------------------------------------------------

void
Thread::Schedule()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);        // ReadyToRun assumes that interrupts
                                        // are disabled!
    (void) interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// Thread::Startup
//      Part of the scheduling code needed to cleanly start a forked child.
//----------------------------------------------------------------------

void
Thread::Startup()
{
   scheduler->Tail();
}

//----------------------------------------------------------------------
// Thread::SortedInsertInWaitQueue
//      Called by SC_Sleep before putting the caller thread to sleep
//----------------------------------------------------------------------

void
Thread::SortedInsertInWaitQueue (unsigned when)
{
   TimeSortedWaitQueue *ptr, *prev, *temp;

   if (sleepQueueHead == NULL) {
      sleepQueueHead = new TimeSortedWaitQueue (this, when);
      ASSERT(sleepQueueHead != NULL);
   }
   else {
      ptr = sleepQueueHead;
      prev = NULL;
      while ((ptr != NULL) && (ptr->GetWhen() <= when)) {
         prev = ptr;
         ptr = ptr->GetNext();
      }
      if (ptr == NULL) {  // Insert at tail
         ptr = new TimeSortedWaitQueue (this, when);
         ASSERT(ptr != NULL);
         ASSERT(prev->GetNext() == NULL);
         prev->SetNext(ptr);
      }
      else if (prev == NULL) {  // Insert at head
         ptr = new TimeSortedWaitQueue (this, when);
         ASSERT(ptr != NULL);
         ptr->SetNext(sleepQueueHead);
         sleepQueueHead = ptr;
      }
      else {
         temp = new TimeSortedWaitQueue (this, when);
         ASSERT(temp != NULL);
         temp->SetNext(ptr);
         prev->SetNext(temp);
      }
   }

   IntStatus oldLevel = interrupt->SetLevel(IntOff);
   //printf("[pid %d] Going to sleep at %d.\n", pid, stats->totalTicks);
   Sleep();
   //printf("[pid %d] Returned from sleep at %d.\n", pid, stats->totalTicks);
   (void) interrupt->SetLevel(oldLevel);
}
#endif
