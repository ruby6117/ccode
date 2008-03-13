#include "Semaphore.h"
#include "SysDep.h"

Semaphore::Semaphore(unsigned int val)
{
  s = ::Sem_create(val); 
}

Semaphore::~Semaphore()
{
  ::Sem_destroy(s);
}

void Semaphore::wait()
{
  ::Sem_wait(s);
}

bool Semaphore::tryWait()
{
  return !::Sem_trywait(s);
}

void Semaphore::post()
{
  ::Sem_post(s);
}

int Semaphore::value() const
{
  return ::Sem_getvalue(s);
}

bool Semaphore::timedWait(AbsTime_t abs)
{
  return ::Sem_timedwait(s, abs) == 0;
}
