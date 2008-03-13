#include "Thread.h"
#include "Timer.h"
#include "SysDep.h"

Thread::Thread()
  : is_running(false), me(0)
{}

Thread::~Thread() 
{
  if (is_running) stop();
}

void * Thread::run_func_wrapper(void *a)
{
  Thread *self = reinterpret_cast<Thread *>(a);
  self->run();
  self->is_running = false;
  return 0;
}

void Thread::start(Priority p)
{
  int s = Default;
  if (p == IdlePriority) s = Other;
  if (p == InheritPriority) s = Inherit, p = IdlePriority;
  me = (Handle)::thread_create(p + thread_prio_min(s), s, &run_func_wrapper, static_cast<void *>(this));
  if (me) is_running = true;
}

void Thread::stop()
{
  if (running()) {
    is_running = false;
    ::thread_kill(me);
  }
}

//static
void Thread::exit()
{
  ::thread_exit(0);
}

/*static*/
Thread::Handle Thread::currentThread()
{
  return static_cast<Handle>(::thread_current());
}

/*static*/ 
void Thread::sleep(int secs)
{
  Timer::Time t = secs;
  Timer::nanoSleep(t*1000000000);
}

/*static*/ 
void Thread::msleep(int msecs)
{
  Timer::Time t = msecs;
  Timer::nanoSleep(t*1000000);  
}

/*static*/ 
void Thread::usleep(int usecs)
{
  Timer::Time t = usecs;
  Timer::nanoSleep(t*1000);  
}

/// much like pthread_setcancelstate -- enable/disable cancellation for the
/// *calling* thread
/*static*/ void Thread::setCancelState(bool enable)
{
  int arg = enable ? ::thread_CancelEnable : ::thread_CancelDisable;  
  ::thread_setcancelstate(arg, 0);                        
}

/*static*/ bool Thread::getCancelState()
{
  int st = ::thread_getcancelstate();                        
  return st == ::thread_CancelEnable;  
}

void Thread::join()
{
  if (running()) ::thread_join(me);  
}


struct FunctorThread : public Thread
{
  FunctorThread(Thread::Functor & f) : f(f) {}
protected:
  void run() { f(); }
private:
  Thread::Functor & f;
};

/* static */
void Thread::doFuncInRT(Functor & f)
{
  if (::thread_self_is_rt()) f();
  else {
    Thread *thr = new FunctorThread(f);
    thr->start();
    thr->join();
    delete thr;
  }
}

