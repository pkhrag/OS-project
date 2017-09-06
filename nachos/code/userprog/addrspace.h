// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize		1024 	// increase this as necessary!
class ProcessAddressSpace {
  public:
    ProcessAddressSpace(unsigned int, TranslationEntry*);	// Create an address space,
    ProcessAddressSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"
	ProcessAddressSpace();
    ~ProcessAddressSpace();			// De-allocate an address space

    void InitUserModeCPURegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveContextOnSwitch();			// Save/restore address space-specific
    void RestoreContextOnSwitch();		// info on a context switch 
  private:
    TranslationEntry *KernelPageTable;	// Assume linear page table translation
					// for now!
    unsigned int numVirtualPages;		// Number of pages in the virtual 
					// address space
public:
	unsigned int getNumPages(){return numVirtualPages;}
	TranslationEntry* getPageTable(){return KernelPageTable;}
	int getMemory(){
		int min = MemorySize-1;
		for(int i=0;i<numVirtualPages;i++)
			if (KernelPageTable[i].physicalPage  < min)	min=KernelPageTable[i].physicalPage;
		return min*PageSize;
	};
};

#endif // ADDRSPACE_H
