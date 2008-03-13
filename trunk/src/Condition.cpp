#include "Condition.h"
#include "SysDep.h"
#include "Mutex.h"


Condition::Condition()
{
  cond = ::cond_create();
}

Condition::~Condition()
{  
  ::cond_destroy(cond);
}

void Condition::signal()
{
  ::cond_signal(cond);
}

void Condition::broadcast()
{
  ::cond_broadcast(cond);
}

void Condition::wait(Mutex & mut)
{
  ::cond_wait(cond, mut.mut);
}

void Condition::timedWait(Mutex & mut, AbsTime_t abs)
{
  ::cond_timedwait(cond, mut.mut, abs);
}
