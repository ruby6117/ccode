#include "Mutex.h"
#include "SysDep.h"

Mutex::Mutex()
  : islocked(false)
{
  mut = ::mut_create();
}

Mutex::~Mutex()
{
  ::mut_destroy(mut);
}


void Mutex::lock()
{
  
  ::mut_lock(mut);
  islocked = true;
}

void Mutex::unlock()
{
  ::mut_unlock(mut);
  islocked = false;
}

bool Mutex::locked() const
{
  return islocked;
}

bool Mutex::tryLock()
{
  int ret = ::mut_trylock(mut);
  if (ret) islocked = true;
  return !ret;
}


/* prohibited */MutexLocker::MutexLocker(const MutexLocker &m) : mut(m.mut) {}
/* prohibited */MutexLocker &MutexLocker::operator=(const MutexLocker &m) {(void)m; return *this; }

