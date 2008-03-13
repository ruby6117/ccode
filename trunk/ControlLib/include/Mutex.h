#ifndef Mutex_H
#define Mutex_H

#include "SysDep.h"

/// A wrapper to the C system-dependent mutex type, typically pthread_mutex_t
class Mutex
{
  friend class Condition;
public:
  Mutex();
  ~Mutex();

  void lock();
  void unlock();
  bool locked() const;
  bool tryLock();
protected: // stuff used by friends.. made it protected for clarity
  Mut_t mut; 
private:
  volatile bool islocked;
};

/// A class that automatically locks a mutex on construction and automatically unlocks it on destruction -- useful as an automatic variable in a function or scope
class MutexLocker
{
public:
  explicit MutexLocker(Mutex &m) : mut(m) { mut.lock(); }
  ~MutexLocker() { mut.unlock(); }
private:
  Mutex &mut;
  /// prohibited operation
  MutexLocker(const MutexLocker &);
  /// prohibited operation
  MutexLocker &operator=(const MutexLocker &o);
};

#endif
