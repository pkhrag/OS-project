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
//	end up calling SelectNextReadyThread(), and that would put us in an 
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

//----------------------------------------------------------------------
// ProcessScheduler::ProcessScheduler/ 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

ProcessScheduler::ProcessScheduler()
{ 
    listOfReadyThreads = new List;
	maxvalue = 0;
	sleepThreads = new(ThreadNode*[MaxThreads]);
} 

//----------------------------------------------------------------------
// ProcessScheduler::~ProcessScheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

ProcessScheduler::~ProcessScheduler()
{ 
    delete listOfReadyThreads; 
} 

//----------------------------------------------------------------------
// ProcessScheduler::MoveThreadToReadyQueue
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
ProcessScheduler::MoveThreadToReadyQueue (NachOSThread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());

    thread->setStatus(READY);
    listOfReadyThreads->Append((void *)thread);
}

//----------------------------------------------------------------------
// ProcessScheduler::SelectNextReadyThread
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

NachOSThread *
ProcessScheduler::SelectNextReadyThread ()
{
    return (NachOSThread *)listOfReadyThreads->Remove();
}

//----------------------------------------------------------------------
// ProcessScheduler::ScheduleThread
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
ProcessScheduler::ScheduleThread (NachOSThread *nextThread)
{
    NachOSThread *oldThread = currentThread;
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveContextOnSwitch();
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

    _SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreContextOnSwitch();
    }
#endif
}

//----------------------------------------------------------------------
// ProcessScheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
ProcessScheduler::Print()
{
    printf("Ready list contents:\n");
    listOfReadyThreads->Mapcar((VoidFunctionPtr) ThreadPrint);
}
// code for implementing the ThreadNode in a heap structure
void ProcessScheduler::addToSleepThreads(NachOSThread* thread, int waketick){
	ThreadNode* noden = new ThreadNode;
	noden->thread = thread;
	noden->waketick = waketick;
	sleepThreads[maxvalue] = noden;
	int i = maxvalue;
	maxvalue++;
	ThreadNode *tmp;
	while((sleepThreads[i]->waketick < sleepThreads[i/2]->waketick) && (i > 0)){
		tmp = sleepThreads[i/2];
		sleepThreads[i/2] = sleepThreads[i];
		sleepThreads[i] = tmp;
	}
}
// code getting minimum wake time thread from the min heap
int ProcessScheduler::getMinWakeTick(){
	if (sleepThreads[0] != NULL) return sleepThreads[0]->waketick;
	return (stats->getTotalTicks() + 1);
}
NachOSThread* ProcessScheduler::removeSleepThread(){
	NachOSThread* minThread = sleepThreads[0]->thread;
	delete sleepThreads[0];
	sleepThreads[0] = sleepThreads[maxvalue--];
	int i=0;
	while((i < maxvalue) && (sleepThreads[i]->waketick >sleepThreads[2*(i+1)]->waketick || sleepThreads[i]->waketick > sleepThreads[2*(i+1) - 1]->waketick )){
		int j = 2*(i+1);
		int k = 2*(i+1) - 1;
		ThreadNode* tmp;
		if (sleepThreads[k]->waketick < sleepThreads[j]->waketick){
			tmp = sleepThreads[k];
			sleepThreads[k] = sleepThreads[j];
			sleepThreads[j] = tmp;
		}
		tmp = sleepThreads[i];
		sleepThreads[i] = sleepThreads[j];
		sleepThreads[j] = tmp;
	}
	return minThread;
}
// code to add thread to wait list of another thread
// implemented using an array of objects of type ThreadPointer

void 
ProcessScheduler::addExitListener(NachOSThread* thread, int pid){
	ThreadPointer * n= new ThreadPointer;
	n->thread = thread;
	n->pid = pid;
    ListElement *temp = new ListElement((void *)n, 0);
    temp->next = joinThreads;
    joinThreads = temp;
}
void ProcessScheduler::wakeAction(int pid, int code){
    ListElement *t = joinThreads;
    ThreadPointer *l; 
	while(t!=NULL){
        l = (ThreadPointer*)t->item; 
        if (l->pid == pid) {
           NachOSThread *baap = l->thread;
#ifdef USER_PROGRAM
           baap->SetRegister(2, code); 
#endif
           scheduler->MoveThreadToReadyQueue(baap); 
        }
        t = t->next;
    }
}
