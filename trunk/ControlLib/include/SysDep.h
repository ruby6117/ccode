#ifndef SysDep_H
#define SysDep_H


#ifdef __cplusplus
extern "C" {
#endif

  /*--------------------------------------------------------------------------
    Time stuff.. 
   --------------------------------------------------------------------------*/
  typedef long long AbsTime_t;
  typedef long long Time_t;
  extern AbsTime_t abstime_get(void); /**< Returns abs. time in ns */
  extern AbsTime_t abstime_nanosleep(AbsTime_t abstime);


  /*--------------------------------------------------------------------------
    Thread stuff.. 
   --------------------------------------------------------------------------*/
  typedef void * Thread_t;
  typedef void *(*ThreadFunc_t)(void *);

  enum scheduler { Default = 0, Other = 1, RR = 2, FIFO = 3, Inherit = 4 };
  extern int thread_prio_min(int sched);
  extern int thread_prio_max(int sched);

  /** prio is a number, the higher the more priority */
  extern Thread_t thread_create(int prio,  /**< in the range thread_prio_min() 
                                              and thread_prio_max() */
                                int sched, /**< one of enum Scheduler */
                                ThreadFunc_t func, void *funcArg);
  /** Attempts to kill the thead.  For zombie threads, immediately returns
      their exit arg.  Returns NULL otherwise (even if thread could not be 
      killed!) */  
  extern void *thread_kill(Thread_t);
  /** Exits the current thread returning return_arg.. */
  extern void thread_exit(void *return_arg);
  /** Returns handle to the calling thread. */
  extern Thread_t thread_current(void);
  /** Returns blocks until the thread exits. */
  extern void *thread_join(Thread_t);

  enum { thread_CancelDisable = 0, thread_CancelEnable };
  extern int thread_setcancelstate(int state, int *oldstate);
  extern int thread_getcancelstate(void);

  /** Returns true iff the current thread is in realtime context */
  extern int thread_self_is_rt(void); 
  
  /*--------------------------------------------------------------------------
    Semaphore stuff.. 
   --------------------------------------------------------------------------*/
  typedef void * Sem_t;
  /** Creates a new semaphore, initializing its value to value, 
      may allocate memory */
  extern Sem_t Sem_create(unsigned int value);
  /** Destroys the semaphore, freeing any resources it may have used.
      Returns nonzero if sem could not be freed because some thread is
      blocking on it! */
  extern int  Sem_destroy(Sem_t);
  /** Just like unix sem_post */
  extern void  Sem_post(Sem_t);
  /** Just like unix sem_wait */
  extern void  Sem_wait(Sem_t);
  /** Just like unix sem_trywait.  Returns nonzero if could not decrease 
      count immediately. */
  extern int  Sem_trywait(Sem_t);
  /** Retreives the count of a semaphore. */
  extern int Sem_getvalue(Sem_t);
  /** Waits on a semaphore until abstime is reached.  Returns zero if the
      semaphore was acquired before the timeout, or nonzero if it wasn't 
      and the timeout expired. */
  extern int Sem_timedwait(Sem_t, AbsTime_t);

  /*--------------------------------------------------------------------------
    Mutex stuff.. 
   --------------------------------------------------------------------------*/
  typedef void * Mut_t;
  /** Creates a new mutex, may allocate memory */
  extern Mut_t mut_create(void);
  /** Destroys the mutex, freeing any memory it may have used.  Returns nonzero
      if the mutex was locked by some thread when.  In that case it is not 
      actually destroyed.*/
  extern int  mut_destroy(Mut_t);
  /** Just like pthread_mutex_lock */
  extern void  mut_lock(Mut_t);
  /** Just like pthread_mutex_unlock */
  extern void  mut_unlock(Mut_t);
  /** Just like pthread_mutex_trylock.  Returns nonzero if could not acquire
      the lock immediately. */
  extern int  mut_trylock(Mut_t);

  /*--------------------------------------------------------------------------
    Cond stuff.. 
   --------------------------------------------------------------------------*/
  typedef void * Cond_t;

  /** Allocated memory for the Cond_t, and initializes it. */
  extern Cond_t cond_create(void);
  /** Destroyes the Cond_t, freeing memory.  May return nonzero if the 
      cond had threads blocking on it.  In which case it is not freed. */
  extern int    cond_destroy(Cond_t);
  extern void   cond_signal(Cond_t);
  extern void   cond_broadcast(Cond_t);
  extern void   cond_wait(Cond_t, Mut_t);
  /** Returns nonzero if the timeout expired without the cond. being 
      signalled. */
  extern int    cond_timedwait(Cond_t, Mut_t, AbsTime_t);

  /*--------------------------------------------------------------------------
    Memory stuff.. 
   --------------------------------------------------------------------------*/
  extern void *Alloc_mem(unsigned long size);
  extern void Free_mem(void *mem);

  /*--------------------------------------------------------------------------
    FIFO stuff.. 
   --------------------------------------------------------------------------*/
  /** returns non-negative descriptor on success, -1 on failure.  
      if minor is negative probes for a free minor.  size should be positive.  
      If the fifo is opened, and we are in kernel context.  In kernel context
      a minor number is returned, in userspace an fd is returned.  */
  extern int RTFOpen(int minor, int size);
  extern void RTFClose(int descr);

  /** Returns the number of bytes available in fifo for reading, 
      0 if empty, or negative on error. */
  extern int RTFReadBytesAvailable(unsigned int fifo);

  /// NB: for now, only implemented in kernel
  extern int RTFMakeUserPair(unsigned get_descr, unsigned put_descr);
  /// NB: for now, RTFHandler only is implemented in the kernel
  extern int RTFHandler(int descr, int (*func)(unsigned int));
  /// NB: for now, RTFHandler_RT only is implemented in the kernel
  extern int RTFHandler_RT(int descr, int (*func)(unsigned int));

  extern int RTFRead(int descr, void *buf, unsigned bytes);
  extern int RTFWrite(int descr, const void *buf, unsigned bytes);

  /*--------------------------------------------------------------------------
    SHM Stuff..
  --------------------------------------------------------------------------*/
  extern void *ShmAttach(const char *name, unsigned long size);
  extern void ShmDetach(const char *name, void *shm_ptr);


  /*--------------------------------------------------------------------------
    Printf-like function
   --------------------------------------------------------------------------*/
  /// Generic interface to either printf, or rtl_printf
  extern int (* const RTPrint)(const char *fmt, ...) __attribute__((format (printf,1,2)));

  /*--------------------------------------------------------------------------
    Misc. utility
   --------------------------------------------------------------------------*/
  extern unsigned Strlen(const char *);
  extern char *Strdup(const char *);
  extern void Strfree(const char *);
  extern int  Strcmp(const char *, const char *);
  extern char *Strncpy(char *, const char *, unsigned long);
  /** Find first set bit. 0 is the first bit, 31 is normally the last.  
      Note that if no bits are set, results are -1. */
  extern long Ffs(unsigned long bits);
  /** Find first zero bit. 0 is the first bit, 31 is normally the last.  
      Note that if all bits are set, results are -1. */
  extern long Ffz(unsigned long bits);
  /// Just like the C library memcpy
  extern void Memcpy(void *dest, const void *src, unsigned long sz);
  /// Just like the C library memset
  extern void Memset(void *mem, char c, unsigned long num);
  /// Just like the C library memcmp
  extern int Memcmp(const void *mem, const void *mem2, unsigned long sz);
  
  /// just like snprintf in C library
  extern int (* const Snprintf)(char *b, unsigned size, const char *fmt, ...) 
       __attribute__ ((format (printf, 3, 4)));

  enum { Unknown, RTLinux, RTAI };
  /** Returns one of Unknown, RTLinux, RTAI.  In kernel mode this is basically
      a noop.  In userspace this actually tries to figure out your rtos
      based on the kernel modules loaded. */
  extern int DetermineRTOS(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
  template <typename T> void Cpy(T & dest, const T & src) { Memcpy(&dest, &src, sizeof(T)); }
  template <typename T> void Clr(T & thing) { Memset(&thing, 0, sizeof(T)); }
#endif


#endif
