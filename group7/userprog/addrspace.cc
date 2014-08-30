// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;

    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;
    
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    threadFile = new char[size];
    if(replaceAlgo==0) ASSERT(numPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;
	pageTable[i].physicalPage = 0;
    if(replaceAlgo == 0)
    {
        pageTable[i].valid = TRUE;
      	pageTable[i].physicalPage = i+numPagesAllocated;
    }
    else       
	pageTable[i].valid = FALSE;
	    
	pageTable[i].replace = FALSE;			
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].shared = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment

if(replaceAlgo == 0){
    bzero(&machine->mainMemory[numPagesAllocated*PageSize], size);
 
    numPagesAllocated += numPages;
}
// then, copy in the code and data segments into memory

    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        vpn = noffH.code.virtualAddr/PageSize;
        offset = noffH.code.virtualAddr%PageSize;
        entry = &pageTable[vpn];
        pageFrame = entry->physicalPage;
        if(replaceAlgo == 0)
        executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			noffH.code.size, noffH.code.inFileAddr);
        else
        executable->ReadAt(&(threadFile[vpn*PageSize + offset]),
                    noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        vpn = noffH.initData.virtualAddr/PageSize;
        offset = noffH.initData.virtualAddr%PageSize;
        entry = &pageTable[vpn];
        pageFrame = entry->physicalPage;
        if(replaceAlgo == 0)
        executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			noffH.initData.size, noffH.initData.inFileAddr);
        else
            executable->ReadAt(&(threadFile[vpn*PageSize + offset]),
                  noffH.initData.size, noffH.initData.inFileAddr);
    }

}


int 
AddrSpace::newPhysicalPage()
{
int i;

for(i=0;i<NumPhysPages;i++)
    {
     if(invertTable[i].free == FALSE)
            {
            return i;  
            }
    }
return -1;          
}


int 
AddrSpace::replacePhysPage()
{
unsigned int i,pageReplace;
    
if(replaceAlgo == RANDOM)
    {

    pageReplace = Random() % NumPhysPages;
    while( ((pageReplace == pageTable[(machine->ReadRegister(PCReg))/PageSize].physicalPage ) && (pageTable[(machine->ReadRegister(PCReg))/PageSize].valid == TRUE )) || (invertTable[pageReplace].shared==TRUE) || invertTable[pageReplace].thread->canReplace == FALSE)
		{
       			pageReplace = Random() % NumPhysPages;
		}
    }
  
else if(replaceAlgo == FIFO)
        {
        pageReplace = 0;
        for(i=0;i<NumPhysPages;i++)
                {
                 if(invertTable[i].shared == FALSE && invertTable[i].fifo <= invertTable[pageReplace].fifo && invertTable[pageReplace].thread->canReplace == TRUE)
                        pageReplace = i;
                        
                 }
         }

else if(replaceAlgo == LRU)
        {
        pageReplace = 0;
	 for(i=0;i<NumPhysPages;i++)
                {
                 if(invertTable[i].shared == FALSE && invertTable[i].lru <= invertTable[pageReplace].lru  && invertTable[pageReplace].thread->canReplace == TRUE)
                        pageReplace = i;
                        
                 }    
        }
        
else if(replaceAlgo == LRUCLOCK)
        {
            //pageReplace = 0;
	    i = lruClockPointer;
            while(invertTable[i].ref == TRUE || invertTable[i].thread->canReplace == FALSE || invertTable[i].shared == TRUE)
		{
			invertTable[i].ref = FALSE;
			i = ( i + 1) % NumPhysPages;
		}	
	pageReplace = i;
	lruClockPointer = (i);
	invertTable[pageReplace].ref = TRUE;
        }
if(invertTable[pageReplace].thread->space != NULL)
{
	TranslationEntry* newPageTable = invertTable[pageReplace].thread->space->GetPageTable();     
		for(i=0;i<PageSize;i++)
		{   
		    invertTable[pageReplace].thread->space->threadFile[(invertTable[pageReplace].vpn)*PageSize + i] = machine->mainMemory[pageReplace*PageSize + i];    
		}
	newPageTable[invertTable[pageReplace].vpn].valid = FALSE;   
}
return pageReplace;        
    
}   

void
AddrSpace::pageDemand(int virtAddr)
{
unsigned int vpn = (unsigned) virtAddr / PageSize;
unsigned int offset = (unsigned) virtAddr % PageSize;
int i, newPage;


newPage = newPhysicalPage();

if(newPage == -1)
{
    newPage = replacePhysPage();
}

pageTable[vpn].valid = TRUE;
pageTable[vpn].physicalPage = newPage;
   
invertTable[newPage].thread = currentThread;
invertTable[newPage].vpn = vpn;
invertTable[newPage].free = TRUE;
invertTable[newPage].fifo = stats->totalTicks;
invertTable[newPage].lru = stats->totalTicks;

bzero(&machine->mainMemory[newPage*PageSize], PageSize);



     for(i =0; i<PageSize; i++)
         machine->mainMemory[newPage*PageSize + i] = threadFile[vpn*PageSize + i];   
     		    
}



void
AddrSpace::threadExit()
{
int i;
for(i=0;i<numPages;i++)
    {
        if(invertTable[pageTable[i].physicalPage].shared == TRUE && invertTable[pageTable[i].physicalPage].numShared>0)
                {
                    invertTable[pageTable[i].physicalPage].numShared--;
                    if(invertTable[pageTable[i].physicalPage].numShared == 0)
                            invertTable[pageTable[i].physicalPage].free = FALSE;
                 }
         
        else if(pageTable[i].valid == TRUE)
                {         
                invertTable[pageTable[i].physicalPage].free = FALSE;
                
                }
        
    
    }
}

int
AddrSpace::newSpace(int sharedSize)
{
    int newPage;
	int oldnumPages = numPages;
	int rem = (sharedSize % PageSize)?1:0;
	int sharedPages = sharedSize/PageSize + rem;
	numPages = oldnumPages + sharedPages;
	unsigned i;
    stats->pageFaults += sharedPages;
	if(replaceAlgo==0) ASSERT(numPages+numPagesAllocated <= NumPhysPages); 

	TranslationEntry* oldPageTable = currentThread->space->GetPageTable();
	TranslationEntry* newPageTable = new TranslationEntry[numPages];

	for(i=oldnumPages;i<numPages;i++)
	{
		newPageTable[i].virtualPage = i;
        if(replaceAlgo !=0){
            newPage = newPhysicalPage();

            if(newPage == -1)
                newPage = replacePhysPage();
        }
            
            else 
                newPage = i+numPagesAllocated-oldnumPages;        

        	newPageTable[i].physicalPage = newPage;
            invertTable[newPage].thread = currentThread;
            invertTable[newPage].free = TRUE;
            invertTable[newPage].vpn = i;
            invertTable[newPage].shared = TRUE;
            invertTable[newPage].numShared ++;
            invertTable[newPage].fifo = stats->totalTicks;
            invertTable[newPage].lru = stats->totalTicks;
        	newPageTable[i].valid = TRUE;
        	newPageTable[i].use = FALSE;
        	newPageTable[i].dirty = FALSE;
        	newPageTable[i].readOnly =FALSE;
		    newPageTable[i].shared = TRUE;
	}


     for(i=0;i<oldnumPages;i++)
     {
         newPageTable[i].virtualPage = oldPageTable[i].virtualPage;
         newPageTable[i].physicalPage = oldPageTable[i].physicalPage;
         newPageTable[i].valid = oldPageTable[i].valid;
         newPageTable[i].use = oldPageTable[i].use;
         newPageTable[i].dirty = oldPageTable[i].dirty;
         newPageTable[i].readOnly = oldPageTable[i].readOnly;
         newPageTable[i].shared = oldPageTable[i].shared;

     }
	pageTable = newPageTable;
	numPagesAllocated+=sharedPages;
	delete oldPageTable;
	RestoreState();

	return oldnumPages*PageSize;
}
//----------------------------------------------------------------------
// AddrSpace::AddrSpace (AddrSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(AddrSpace *parentSpace)
{



    numPages = parentSpace->GetNumPages();
    unsigned i, size = numPages * PageSize;
    if(replaceAlgo == 0)
        ASSERT(numPages+numPagesAllocated <= NumPhysPages);                // check we're not trying
    int j, newPage;                                                                            // to run anything too big --
                                                                                // at least until we have
    threadFile= new char[size];                                                                            // virtual memory
    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                                        numPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
pageTable = new TranslationEntry[numPages];
         

    if(replaceAlgo == 0)
    {
        for (i = 0; i < numPages; i++) {
            pageTable[i].virtualPage = i;
            if(parentPageTable[i].shared == FALSE)
                pageTable[i].physicalPage = i+numPagesAllocated;

            else
                pageTable[i].physicalPage = parentPageTable[i].physicalPage;    
            pageTable[i].valid = parentPageTable[i].valid;
            pageTable[i].use = parentPageTable[i].use;
            pageTable[i].dirty = parentPageTable[i].dirty;
            pageTable[i].readOnly = parentPageTable[i].readOnly;      // if the code segment was entirely on
            pageTable[i].shared = parentPageTable[i].shared;
            // a separate page, we could set its
            // pages to be read-only
        }

        // Copy the contents
        unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
        unsigned startAddrChild = numPagesAllocated*PageSize;
        for (i=0; i<size; i++) {
            machine->mainMemory[startAddrChild+i] = machine->mainMemory[startAddrParent+i];
        }

        numPagesAllocated += numPages;   
    }

    else{    
	currentThread->canReplace = FALSE;
	child->canReplace = FALSE;
        for (i = 0; i < numPages; i++) {

            if(parentPageTable[i].shared==FALSE && parentPageTable[i].valid == TRUE)
            {

                newPage = newPhysicalPage();
                if(newPage == -1)
                     newPage = replacePhysPage();
                pageTable[i].physicalPage = newPage;
                
                invertTable[newPage].thread = child; 
                invertTable[newPage].free = TRUE;
                invertTable[newPage].vpn = i;
                invertTable[newPage].fifo = stats->totalTicks;
                invertTable[newPage].lru = stats->totalTicks;
		invertTable[newPage].fork = TRUE;
                                                     
                for(j=0; j<PageSize;j++)
                    machine->mainMemory[((newPage)*PageSize) + j] = machine->mainMemory[((parentPageTable[i].physicalPage)*PageSize)+j];

                pageTable[i].valid = TRUE;
            }
            else if(parentPageTable[i].shared == FALSE && parentPageTable[i].valid == FALSE)
            {
                pageTable[i].physicalPage = 0;
                pageTable[i].valid = FALSE;
            }

            else if(parentPageTable[i].shared == TRUE)
            {
                pageTable[i].physicalPage = parentPageTable[i].physicalPage;
                pageTable[i].valid = TRUE;    
                invertTable[parentPageTable[i].physicalPage].numShared ++;
            }

            pageTable[i].use = parentPageTable[i].use;
            pageTable[i].dirty = FALSE;
            pageTable[i].shared = parentPageTable[i].shared;
            pageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
            // a separate page, we could set its
            // pages to be read-only
        }


        // Copy the contents
        //  unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
        //  unsigned startAddrChild = numPagesAllocated*PageSize;

        for (i=0; i<size; i++) {
            // machine->mainMemory[startAddrChild+i] = machine->mainMemory[startAddrParent+i];
            threadFile[i] = currentThread->space->threadFile[i];    
        }

	currentThread->canReplace = TRUE;
	child->canReplace = TRUE;
    }
    //numPagesAllocated += numPages;
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

unsigned
AddrSpace::GetNumPages()
{
   return numPages;
}

TranslationEntry*
AddrSpace::GetPageTable()
{
   return pageTable;
}
