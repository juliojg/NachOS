/// Routines for synchronizing threads.
///
/// Three kinds of synchronization routines are defined here: semaphores,
/// locks and condition variables (the implementation of the last two are
/// left to the reader).
///
/// Any implementation of a synchronization routine needs some primitive
/// atomic operation.  We assume Nachos is running on a uniprocessor, and
/// thus atomicity can be provided by turning off interrupts.  While
/// interrupts are disabled, no context switch can occur, and thus the
/// current thread is guaranteed to hold the CPU throughout, until interrupts
/// are reenabled.
///
/// Because some of these routines might be called with interrupts already
/// disabled (`Semaphore::V` for one), instead of turning on interrupts at
/// the end of the atomic operation, we always simply re-set the interrupt
/// state back to its original value (whether that be disabled or enabled).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2017 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch.hh"
#include "system.hh"


/// Initialize a semaphore, so that it can be used for synchronization.
///
/// * `debugName` is an arbitrary name, useful for debugging.
/// * `initialValue` is the initial value of the semaphore.
Semaphore::Semaphore(const char *debugName, int initialValue)
{
    name  = debugName;
    value = initialValue;
    queue = new List<Thread *>;
}

/// De-allocate semaphore, when no longer needed.
///
/// Assume no one is still waiting on the semaphore!
Semaphore::~Semaphore()
{
    delete queue;
}


/// Wait until semaphore `value > 0`, then decrement.
///
/// Checking the value and decrementing must be done atomically, so we need
/// to disable interrupts before checking the value.
///
/// Note that `Thread::Sleep` assumes that interrupts are disabled when it is
/// called.
void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
      // Disable interrupts.

    while (value == 0) {  // Semaphore not available.
        queue->Append(currentThread);  // So go to sleep.
        currentThread->Sleep();
    }
    value--;  // Semaphore available, consume its value.

    interrupt->SetLevel(oldLevel);  // Re-enable interrupts.
}

/// Increment semaphore value, waking up a waiter if necessary.
///
/// As with `P`, this operation must be atomic, so we need to disable
/// interrupts.  `Scheduler::ReadyToRun` assumes that threads are disabled
/// when it is called.
void
Semaphore::V()
{
    Thread   *thread;
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);

    thread = queue->Remove();
    if (thread != NULL)  // Make thread ready, consuming the `V` immediately.
        scheduler->ReadyToRun(thread);
    value++;
    interrupt->SetLevel(oldLevel);
}

/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Lock::Lock(const char *debugName)
{
  sem = new Semaphore(debugName,1);
  name = debugName;
  hold_name = NULL;
}

Lock::~Lock()
{
  delete sem;
}

bool
Lock::IsHeldByCurrentThread()
{
  return hold_name == currentThread;
}

void
Lock::Acquire()
{
  ASSERT(!(IsHeldByCurrentThread()));
  if (hold_name != NULL){
    int HolderPriority = hold_name -> ActualPriority;
    if (HolderPriority > currentThread->ActualPriority){
      DEBUG('s',"Realice inversion de prioridades con %s y %s\n",hold_name->getName(),currentThread->getName());
      hold_name -> ActualPriority = currentThread->ActualPriority;
      hold_name -> OldPriority = HolderPriority;
    }
  }
        
  sem -> P(); 
  hold_name = currentThread;
}

void
Lock::Release()
{
  ASSERT(IsHeldByCurrentThread());
  
  hold_name -> ActualPriority = hold_name -> OldPriority;

  sem -> V();
  hold_name = NULL;
}


Condition::Condition(const char *debugName, Lock *conditionLock)
{
  name = debugName;
  lock = conditionLock;
  sleep_queue = new List<Semaphore*>();

}

Condition::~Condition()
{
  delete sleep_queue;
}

void
Condition::Wait()
{
    ASSERT(lock->IsHeldByCurrentThread());
    Semaphore *s = new Semaphore("SemaphoreForSleep",0);
    sleep_queue->Append(s);

    int HolderPriority = (lock->hold_name)->ActualPriority;
    if( HolderPriority < currentThread->ActualPriority){
      (lock->hold_name)->ActualPriority = currentThread->ActualPriority;
      (lock->hold_name)->OldPriority = HolderPriority;
    }


    lock->Release();
    s->P();
    lock->Acquire();
}

void
Condition::Signal()
{
    Semaphore *s;
    ASSERT(lock->IsHeldByCurrentThread());
    
    (lock->hold_name)->ActualPriority = (lock->hold_name)->OldPriority;
    
    if(!sleep_queue->IsEmpty()){
      s = sleep_queue->Remove();
      s->V();
    }
}

void
Condition::Broadcast()
{
  Semaphore *s;
  while(!sleep_queue->IsEmpty()){
    s = sleep_queue->Remove();
    s->V();
  }
}

Puerto::Puerto(const char* debugName){
  name = debugName;
  lock = new Lock("PuertoLock");
  full = new Condition("PuertoConditionLleno",lock);
  empty = new Condition("PuertoConditionVacio",lock);
  IsEmpty = true;
}

Puerto::~Puerto(){
  delete lock;
  delete full;
  delete empty;
}
void
Puerto::Send(int mensaje){
  lock -> Acquire();
  
  while(!IsEmpty)
    empty -> Wait();

  buff = mensaje;
  IsEmpty = false;
  full -> Signal();
  lock -> Release();
}

void
Puerto::Receive(int *mensaje){
  lock -> Acquire();
  while(IsEmpty)
    full-> Wait();  

  *mensaje = buff;
  IsEmpty = true;
  empty -> Signal();
  lock -> Release();
}

