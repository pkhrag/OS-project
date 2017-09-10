// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"
#include "translate.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

void start_fork(int which){
	if(threadToBeDestroyed!=NULL) {
		delete threadToBeDestroyed;
		threadToBeDestroyed=NULL;
	}

#ifdef USER_PROGRAM
	if (currentThread->space != NULL){
		currentThread->RestoreUserState();
		currentThread->space->RestoreContextOnSwitch();
	}
#endif
	machine->Run();
}

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp, userVirtualAddress, i;
	unsigned int userVpgnumber, userOffset, userPPN, pageTableSize;
	bool userFlag;
	int startTick, numTicks, thisTick, lastTick;
	TranslationEntry *userEntry, *tlb, *KernelPageTable;
    unsigned printvalus;        // Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SysCall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SysCall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
	  writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
	     writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
	     writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintChar)) {
	writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } else if ((which == SyscallException) && (type == SysCall_GetReg)) {
		machine->WriteRegister(2, machine->ReadRegister(machine->ReadRegister(4)));
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	} else if ((which == SyscallException) && (type == SysCall_GetPA)) { 
		userVirtualAddress = machine->ReadRegister(4);
		pageTableSize = machine->pageTableSize;
		tlb = machine->tlb;
		KernelPageTable = machine->KernelPageTable;
		ASSERT((tlb == NULL) || (KernelPageTable == NULL))
		ASSERT((tlb != NULL) || (KernelPageTable != NULL))
		userVpgnumber = userVirtualAddress / PageSize;
		userOffset = userVirtualAddress % PageSize;

		if (userVpgnumber >= pageTableSize){
			userFlag = true;
			machine->WriteRegister(2, -1);
		} else {
			userEntry = NULL;				
			if (tlb == NULL){
				if(KernelPageTable[userVpgnumber].valid){
					userEntry = &KernelPageTable[userVpgnumber];
				}
			} else {
				for(i = 0; i <= TLBSize; i++){
					if((tlb[i].valid) && (tlb[i].virtualPage == userVpgnumber)) {
						userEntry = &tlb[i];
						userEntry->use = true;
						break;
					}
				}
			}
			if (userEntry != NULL) {
				userPPN = userEntry->physicalPage;
				if (userPPN >= NumPhysPages){
					userFlag = true;
					machine->WriteRegister(2, -1);
				}
			} else {
				machine->WriteRegister(2, -1);
			}
		}
		if (!userFlag){
			machine->WriteRegister(2, (userOffset +(userPPN*PageSize)));
		} 

	   // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
		
	} else if ((which == SyscallException) && (type == SysCall_GetPID)) {
		machine->WriteRegister(2, currentThread->getPID());
	   // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

	} else if ((which == SyscallException) && (type == SysCall_GetPPID)) {
		machine->WriteRegister(2, currentThread->getPPID());
	   // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

	} else if ((which == SyscallException) && (type == SysCall_Time)) {
		machine->WriteRegister(2, stats->getTotalTicks());
	   // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	} else if ((which == SyscallException) && (type == SysCall_Sleep)) {
		numTicks = (unsigned) machine->ReadRegister(4);
		if(numTicks == 0){
			currentThread->YieldCPU();
		} else {
			IntStatus old = interrupt->SetLevel(IntOff);
			scheduler->addToSleepThreads(currentThread, stats->getTotalTicks()+numTicks);
			currentThread->PutThreadToSleep();
			interrupt->SetLevel(old);
		}
		// Advance program counters.
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
		
	} else if ((which==SyscallException) && (type==SysCall_Yield)) {
		currentThread->YieldCPU();	
		// Advance program counters.
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
		
	} else if ((which == SyscallException) && (type == SysCall_Fork)){
		char* name = currentThread->getName();
		NachOSThread * forkThread = new NachOSThread(name);
		// Advance program counters.
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
		currentThread->SaveUserState();
		forkThread->setInstructionCount(currentThread->getIC());
		forkThread->setProcessSpace();
		machine->WriteRegister(2, 0);
		forkThread->SaveUserState();
		machine->WriteRegister(2, forkThread->getPID());
		currentThread->SaveUserState();
		forkThread->ThreadFork(&start_fork,0);
	} else if ((which==SyscallException) && (type=SysCall_NumInstr)) {
		machine->WriteRegister(2, stats->userTicks);
		// Advance program counters.
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	} else if ((which == SyscallException) && (type=SysCall_Join)) {
		int jpid = machine->ReadRegister(4);
		bool r = currentThread->getProcessStatus(jpid);	
		if (!r){
			machine->WriteRegister(2,-1);
		} else {	
			int exitStatus = currentThread->getExitStatus(jpid);
			if(exitStatus < 0) {
				scheduler->addExitListener(currentThread, jpid);
				IntStatus old = interrupt->SetLevel(IntOff);
				currentThread->PutThreadToSleep();
                (void) interrupt->SetLevel(old);
			}
			else
				machine->WriteRegister(2,exitStatus);
		}
		// Advance program counters
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	} 
	else if ((which == SyscallException) && (type=SysCall_Exit)) {
        int code = machine->ReadRegister(4);
        int me = currentThread->getPID();
        int baap = currentThread->getPPID();
        currentThread->setExitStatus(baap, me, code, FALSE);
        currentThread->FinishThread();
        scheduler->wakeAction(me, code);
    }
    else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
