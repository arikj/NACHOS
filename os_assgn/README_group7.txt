							NACHOS ASSIGNMENT 1
						
						Arihant Kumar Jain	11147
						Bhavishya Mittal	11198
						Javesh Garg		11334



1. SC_GetReg:- Returns the value stored in the register number passed as argument.

1) The argument is passed in the register 4.
2) Using the ReadRegister() from the machine class to read the register number(say, num) from register 4.
3) Again using ReadRegister to read the value stored in the register 'num'.
4) Finally, write the value of the register in the register 2 (the return register) using the WriteRegister() function.
5) Increment the PC(program counter) so as to read the next instruction.



2. SC_GetPA:- Returns the physical address corresponding to the virtual address passed as argument in register 4.

1) Find the virtual page number (vpn) and the offset for the given virtual address.
2) Next check for error if the calculated vpn is greater than the machine pageTableSize. If error is found, -1 is returned. 
3) Find the the page table entry for corresponding vpn and retrieve the desired physical page number(say, pageframe).
4) Check for error if the pageframe is greater than the maximum number of physical pages. If error is found, return -1.
5) Return ‘page frame * page size + offset’ as the physical address.
6) Increment the Program Counter.



3. SC_GetPID:- Returns the PID of the calling thread.

1) Pass the value of the Process ID in register 2.
2) The PID is a unique number assigned to every thread whenever it is created which is incremented using a global variable.
3) This is done in the constructor Thread::Thread(char* threadName).
4) The PID, being a private member of class Thread, is accessed by function call ‘call_pid()’ defined inside the class.
5) Increment the Program Counter.



4. SC_GetPPID:- Returns the PID of the parent of the calling thread.

1) Pass the value of the Process ID in register 2.
2) PPID is assigned to a thread when it is created in the constructor of the class Thread. It is assigned the PID of the creating thread. 
3) The PPID, being a private member of class Thread, is accessed by function call ‘call_ppid()’ defined inside the class.
4) Increment the Program Counter.



5. SC_Time:- Returns the total ticks i.e. current simulated time at present

1) Returns total clock ticks retrieved from an object of class Statistics using member variable totalTicks.
2) Increment the Program Counter.



6. SC_Sleep:- Puts the calling thread to sleep for the number of ticks passed as argument.

1) A queue called sleeping_queue is maintained, which stores all the threads currently sleeping on a timer interrupt.
2) The calling thread is inserted into the queue in sorted order with respect to the time(clock tick) to wake the thread (argument passed + current ticks), using SortedInsert function of ListElement class.
3) Interrupt is turned off before putting thread to sleep and after returning from sleep,set the interrupt to the previous level.
4) On every timer tick, TimerInterruptHandler() is called where the clock ticks for waking first thread on sleeping_queue is checked. If its ticks are over, remove it from sleeping_queue and put it on the ready queue. Continue this step till all the threads whose sleeping time has expired is removed from the queue.
5)Increment the PC when the thread returns fron the sleep.



7. SC_Yield:- The calling thread voluntarily gives up the CPU to the scheduler so that some other thread can now be scheduled. 

1) Turn off the interrupts and find the first thread waiting in ready_queue(say, nextThread) to run using FindNextToRun() function of class Scheduler.
2) The current thread is put in the ready queue using ReadyToRun(currentThread) and the next thread is run using Run(nextThread) of Scheduler class.
3) Set the interrupt level to the previous level and increment the PC.



8. SC_Exec:- Load the executable passed as argument in the current context of the calling thread

Constructor of AddrSpace is changed in such a way that every time, address space is allocated for new threads, they are provided separate physical addresses by maintaing a global variable 'physicalAddressPos' which is incremented when physical address is allocated to a thread.
  
1) Read the pointer to address of the executable file name from the register 4(argument).
2) Read the file path from the memory and store it in a variable "filename".
3) Open the file using the "fileSystem->Open()" function and create a new address space using "AddrSpace(executable)" constructor.
4) Delete the current space of the thread by calling destructor of the class AddrSpace and assign it the new space, allocated before.
5) Close the executable file and initialize the registers with the default values.
6) Load page table using the "space->RestoreState()" function.



9. SC_Join:- Allow a thread to join, i.e. wait for the exit of its children.

1) A new structure array child_list storing the pid and the exit code of all the child threads of the parent thread is maintained for each thread.  
	- Whenever a thread creates a child thread, pid and exit code(initialised as -1) is inserted into the child_list of parent thread.
	- A variable "num_child" is maintained which is incremented on creation of a new child to keep track of number of child threads.
	- This is done in the constructor of class Thread.
2) Search for the child thread with PID that is passed in register 4 as argument in the child_list of the currentThread. This is done by calling searchforchild() function defined in class Thread. It returns exit code if child exists, else returns -2.
	- If child does not exist, return -1;	
	- If child exists but has non-negative exit code, i.e. child has already exited, return exit code.
	- If child exists and the child has not exited, we put the thread in the join_list along with the child thread's pid and put it to       sleep till the child thread exits.
3) Increment the PC when the thread wakes up after the child thread exits.



10. SC_Exit:- Exit the current running thread and wakes up the parent thread if waiting for it to exit after Join()

1) A global variable 'running_threads' is maintained to keep track of all the live threads, i.e threads that have not yet exited. 
	- Every time a thread is created it is incremented by 1 in class Thread constructor. 
	- On every exit, decrement the "running_threads" by 1. If 'running_threads'  reduce to zero, execute the Halt() system call to end simulation.
2) If calling thread is not the last thread to exit, 
	- update the exit code in the child_list of parent thread using parentupdate() defined in class Thread.
	- check if the parent thread is waiting for the exit of this thread in join_list using wakeup_parent defined in class Scheduler.
	- If parent thread is found waiting for the current thread to exit, wake it up from sleep and put it in the ready queue and remove the entry from join_list.
3) Finally, call exitingthread() defined in class Thread where status is set to be BLOCKED and threadtobedestroyed is set to be the currentThread and Sleep() is called.
4) Whenever next thread is scheduled to run, the thread in threadtobedestroyed is deleted.



11. SC_Fork:- Create a new thread and duplicate the address space of the calling thread.
On calling Fork() following steps are taken:
1) Increase PC by 1
	This has to be done before step 4 or otherwise the child process will start from Fork() line and this will create an infinite loop of Fork() call.
2) Create a new thread.
	This is our child thread.
3) Copy the Address Space and physical memory of parent ie existing process to child's address space.
	This has to be done as the address space of parent is to be exactly copied in child's.
	For this, we created a new constructor AddrSpace() for AddrSpace class and in this constructor we set all corresponding values to the parent ie currentProcess values.
4) Save current thread context ie registers,PC etc in the child's thread.
	SaveUserState function of Thread class is used for this
5) Set return value in child's thread as 0 ie set Reg 2 to 0.
	For this we defined a new function set2Register() in Thread class which sets the data variable userRegisters[2]=0
	This is done so that when child process resumes , value 0 is returned by Fork().
6) Call Fork(func,arg) with the func defined.
	This is the default function in Thread class. This function  allocates the stack frame for the child process ant puts it in ready-to-run queue.This function requires a function as an argument.
	func:
		This function is executed for the first time this child process is put into ready list by the scheduler.
		The purpose of this function is to restore context.
7) Set return value in parent process as child pid
	This is done as in the parent process Fork() should return the pid of the child's process just created.


