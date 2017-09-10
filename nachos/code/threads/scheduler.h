// scheduler.h 
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "copyright.h"
#include "list.h"
#include "thread.h"

// The following class defines the scheduler/dispatcher abstraction -- 
// the data structures and operations needed to keep track of which 
// thread is running, and which threads are ready but not running.
struct ThreadNode {
	NachOSThread *  thread;
	int waketick;
};

struct ThreadPointer {
	NachOSThread* thread;
    int pid;
};

class ProcessScheduler {
	ThreadNode** sleepThreads;
	ListElement *joinThreads;
	int maxvalue;
  public:
    ProcessScheduler();	// Initialize list of ready threads 
    ~ProcessScheduler();// De-allocate ready list
	void addExitListener(NachOSThread*, int);
	void wakeAction(int, int);
	void addToSleepThreads(NachOSThread* thread, int waketick);
	int getMinWakeTick();
	NachOSThread* removeSleepThread();
    void MoveThreadToReadyQueue(NachOSThread* thread);	// Thread can be dispatched.
    NachOSThread* SelectNextReadyThread();		// Dequeue first thread on the ready 
					// list, if any, and return thread.
    void ScheduleThread (NachOSThread* nextThread);	// Cause nextThread to start running
    void Print();			// Print contents of ready list
    
  private:
    List *listOfReadyThreads;  		// queue of threads that are ready to run,
				// but not running
};

#endif // SCHEDULER_H
