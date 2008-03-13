#ifndef Semaphore_H
#define Semaphore_H

#include "SysDep.h"

/** Implements a semaphore.  This is pretty much a POSIX-style semaphores 
    workalike for C++. */
class Semaphore
{
public:
  Semaphore(unsigned int value);
  ~Semaphore();
  
  /// waits for semaphore to be nonzero, then decreses count
  void wait(); 
  /// returns immediately with false if sem count is 0, otherwise decreases count, returns true
  bool tryWait(); 
  /** wait with a timeout.  Returns true if we acquired semaphore before 
      timeout expired, or false otherwise */
  bool timedWait(AbsTime_t);
  /// atomically increases count, wakes sleepers
  void post();
  /// returns the current semaphore's value
  int value() const;

private:
  mutable Sem_t s;
};

#endif
