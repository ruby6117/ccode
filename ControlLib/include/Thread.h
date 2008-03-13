#ifndef Thread_H
#define Thread_H

/// Encapsulates a thread on the target system.  Inherit from this to implement your own threads.
class Thread
{
public:
  enum Priority { IdlePriority=0, LowestPriority, LowPriority, NormalPriority, HighPriority, HighestPriority, InheritPriority = 0xffffffff };
  typedef void * Handle;

  Thread();
  virtual ~Thread();
  /// Starts the thread.  The new thread will begin executing the run() routine as soon as it is scheduled.
  virtual void start(Priority = NormalPriority);
  /// Stops the thread -- waiting for it to complete.  Depending on the system the cancel command sent to the thread may or may not work so you might want to reimplement this in your derived classes and use some sort of a boolean flag to implement stopping, then call this.
  virtual void stop();
  bool running() const { return is_running; }
  
  /// blocks until the thread completes.
  void join();

  /// much like pthread_setcancelstate -- enable/disable cancellation for the
  /// *calling* thread
  static void setCancelState(bool enabledisable);
  /// find out if this thread is cancellable
  static bool getCancelState();

  /// called by a thread to exit the current thread of execution
  static void exit();
  
  /// returns a handle to the caller's thread.
  static Handle currentThread();

  static void sleep(int secs);
  static void msleep(int msecs);
  static void usleep(int usecs);

  /// Functor object used with doFuncInRT method
  struct Functor 
  {
    virtual ~Functor() {}
    virtual void operator()() = 0; 
  };
  /** Call a function in RT.  Here are the rules:
      1. Iff we are currently in userspace, just call f().
      2. Iff we are in linux nonrealtime context, create a new
         realtime thread and the next time the thread is run, call f().  Wait
         for thread to exit before returning.
      3. Iff we are in linux realtime context, just call f() directly.
      Why?? This function is useful for calling code such as FP 
      code that only works in RT. */
  static void doFuncInRT(Functor & f);

protected:
  /// The thread function -- derived classes need to implement this.
  virtual void run() = 0;

private:
  /// no public copy c'tor 
  Thread(const Thread &t) { (void)t; }
  /// no public operator=
  Thread & operator=(const Thread &t) { (void)t; return *this; }

private:
  static void *run_func_wrapper(void *);

  bool is_running;
  Handle me;
};

#endif
