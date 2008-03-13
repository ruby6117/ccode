#ifndef Condition_H
#define Condition_H

#include "SysDep.h"
#include "Mutex.h"


/// A class implementing a condition variable.  Typically this is implemented using the pthread_cond_* functions, and always has their semantics.
class Condition
{
public:
  Condition();
  ~Condition();
  
  void signal();
  void broadcast();
  void wait(Mutex & mutex);
  void timedWait(Mutex & mutex, AbsTime_t absTimeout);  
private:
  Cond_t cond;
};

#endif
