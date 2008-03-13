#include "Lockable.h"
#if defined(UNIX) || defined(LINUX)
#  include <pthread.h>
struct Lockable::Private
{
  pthread_mutex_t mut;

  Private() { pthread_mutex_init(&mut, 0); }
  ~Private() { pthread_mutex_destroy(&mut); }
  void lock() { pthread_mutex_lock(&mut); }
  void unlock() { pthread_mutex_unlock(&mut); }
};
#elif defined(WIN32)
#  include <windows.h>
struct Lockable::Private
{
  HANDLE mut;

  Private() { mut = CreateMutex(0, FALSE, 0); }
  ~Private() { CloseHandle(mut); }
  void lock() { WaitForSingleObject(mut, INFINITE); }
  void unlock() { ReleaseMutex(mut); }
};
#else
#  error "To use lockable, please define one of WIN32, UNIX, LINUX."
#endif

Lockable::Lockable() {  p = new Private; }

Lockable::~Lockable() { delete p; p = 0; }

void Lockable::lock()
{
  p->lock();
}

void Lockable::unlock()
{
  p->unlock();
}
