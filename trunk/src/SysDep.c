#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifdef _XOPEN_SOURCE
#  undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 600 /* for clock_nanosleep */
#include "SysDep.h"

#define BILLION 1000000000


#ifdef __KERNEL__

#include <linux/vmalloc.h>
#include <linux/string.h>
#include <asm/bitops.h>

void *Alloc_mem(unsigned long size)
{
  return vmalloc(size);
}

void Free_mem(void *mem)
{
  vfree(mem);
}

unsigned Strlen(const char *s)
{
  return strlen(s);
}

char * Strdup(const char *s)
{
  unsigned len = Strlen(s);
  char *b = (char *) Alloc_mem(len+1);
  strcpy(b, s);
  return b;
}

#  ifdef RTLINUX
#    include <rtl_time.h>
#    include <rtl_sched.h>
#    include <rtl_sema.h>
#    include <rtl_mutex.h>
#    include <rtl_fifo.h>
#    include <mbuff.h>
#    define pthread_attr_setinheritsched(a,i) do { (void)a; (void)i; } while (0)
#    define pthread_attr_setschedpolicy(a,p) do { (void)a; (void)p; } while (0)
#    define PTHREAD_INHERIT_SCHED 0
#    define PTHREAD_EXPLICIT_SCHED 0
#  elif defined(RTAI)
#    error RTAI not yet supported
#  else
#    error Define one of RTAI or RTLINUX
#  endif

#  ifdef RTLINUX
AbsTime_t abstime_get(void)
{
  return gethrtime();
}

AbsTime_t abstime_nanosleep(AbsTime_t abs)
{
  struct timespec ts = timespec_from_ns(abs), rem;
  clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, &rem);
  return timespec_to_ns(&rem);
}

int RTFOpen(int minor, int size)
{
  int err;
  if (minor < 0) {
    int i;
    for (i = 0; i < RTF_NO; ++i) 
      if (rtf_create(i, size) == 0) { minor = i; break; }
    if (minor < 0) return -1;
    return minor;
  }
  /* else minor >= 0.. */
  err = rtf_create(minor, size);
  if (err) return err;
  return minor;
}

void RTFClose(int descr)
{
  rtf_destroy(descr);
}

int RTFHandler(int descr, int (*func)(unsigned int))
{
  return rtf_create_handler(descr, func) == 0;
}

int RTFHandler_RT(int descr, int (*func)(unsigned int))
{
  return rtf_create_rt_handler(descr, func) == 0;
}

int RTFRead(int descr, void *buf, unsigned bytes)
{
  return rtf_get(descr, (char *)buf, bytes);
}

int RTFWrite(int descr, const void *buf, unsigned bytes)
{
  return rtf_put(descr, (char *)buf, bytes);
}

int RTFMakeUserPair(unsigned get_descr, unsigned put_descr)
{
  return rtf_make_user_pair(get_descr, put_descr) == 0;
}

int RTFReadBytesAvailable(unsigned int fifo)
{
  if (rtf_isused(fifo))
    return RTF_LEN(fifo);
  return -1;
}

int (*const RTPrint)(const char *fmt, ...) __attribute__((format (printf,1,2))) = &rtl_printf;

void *ShmAttach(const char *name, unsigned long size)
{
  return mbuff_attach(name, size);
}

void ShmDetach(const char *name, void *shm_ptr)
{
  mbuff_detach(name, shm_ptr);
}

int DetermineRTOS(void) { return RTLinux; }

#  endif

#  ifdef RTAI
/** Do something... */
int DetermineRTOS(void) { return RTAI; }
#  endif



#else /* !__KERNEL__ */

#  include <string.h>
#  include <pthread.h>
#  include <stdlib.h>

void *Alloc_mem(unsigned long s) {  return malloc(s); }
void Free_mem(void *mem) { return free(mem); }
unsigned Strlen(const char * s) {  return strlen(s); }
char * Strdup(const char *s) { return strdup(s); }

#  ifdef UNIX
#    include <time.h>
#    include <semaphore.h>

AbsTime_t abstime_get(void)
{
  struct timespec ts;
  AbsTime_t t = 0;
  clock_gettime(CLOCK_REALTIME, &ts);
  t = (AbsTime_t)(ts.tv_sec) * (AbsTime_t)(BILLION) + (AbsTime_t)ts.tv_nsec;
  return t;
}

AbsTime_t abstime_nanosleep(AbsTime_t abs)
{
  struct timespec ts, rem;
  ts.tv_sec = abs / BILLION;
  ts.tv_nsec = abs % BILLION;
  clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, &rem);
  abs = ts.tv_sec * BILLION + ts.tv_nsec;
  return abs;
}

#    include <string.h>
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <fcntl.h>
#    include <unistd.h>
#    include <sys/ioctl.h>
#    include <stdio.h>
#    ifndef RTF_NO
#      define RTF_NO 64
#    endif

static inline int RTFOpen_Noprobe(unsigned minor)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "/dev/rtf%u", minor);
  buf[sizeof(buf)-1] = 0;
  return open(buf, O_RDWR);
}

int RTFOpen(int minor, int size)
{
  int fd;
  (void) size; /* size param ignored in userspace.. */
  if (minor < 0) {
    int i;
    for (i = 0; i < RTF_NO; ++i) 
      if ( (fd = RTFOpen_Noprobe(i)) > -1 ) { 
        minor = i; 
        break; 
      }
    if (minor < 0) return -1;
  } else {
    fd = RTFOpen_Noprobe(minor);
  }
  return fd;
}

void RTFClose(int descr)
{
  close(descr);
}

int RTFHandler(int descr, int (*func)(unsigned int))
{
  (void)descr;
  (void)func;
  if (func) fprintf(stderr, "Warning: RTFHandler() unimplemented in userspace!\n");
  return 0;
}

int RTFHandler_RT(int descr, int (*func)(unsigned int))
{
  (void)descr;
  (void)func;
  if (func) fprintf(stderr, "Warning: RTFHandler_RT() unimplemented in userspace!\n");
  return 0;
}

int RTFRead(int descr, void *buf, unsigned bytes)
{
  return read(descr, buf, bytes);
}

int RTFWrite(int descr, const void *buf, unsigned bytes)
{
  return write(descr, buf, bytes);
}

int RTFMakeUserPair(unsigned get_descr, unsigned put_descr)
{
  (void)get_descr;
  (void)put_descr;
  fprintf(stderr, "Warning: RTFMakeUserPair() unimplemented in userspace!\n");
  return 0;
}

int RTFReadBytesAvailable(unsigned int fifo)
{
  int ret, bytes = -1;
  ret = ioctl(FIONREAD, (int)fifo, &bytes);
  if (ret >= 0) return bytes;
  return ret;
}

int (*const RTPrint)(const char *fmt, ...) __attribute__((format (printf,1,2))) = &printf;

/* For userspace muff_attach and rtai_shm_attach -- stolen from rtlab 
    project */
#    include "rtos_shared_memory.h"

int DetermineRTOS(void) 
{
  int rtosUsed = Unknown;
  struct stat statbuf;

  if (!stat("/proc/rtai", &statbuf)) rtosUsed = RTAI;
  else if(!stat("/proc/modules", &statbuf)) {
    FILE *proc_modules = fopen("/proc/modules", "r");
    static const int BUFSZ = 256;
    char modname_buf[BUFSZ];
    char FMT[32];
    char *lineptr = 0;
    size_t lnsz = 0;

    snprintf(FMT, sizeof(FMT)-1, "%%%ds", BUFSZ);
    FMT[sizeof(FMT)-1] = 0;
    while(rtosUsed == Unknown
          && getline(&lineptr, &lnsz, proc_modules) > 0) {
      // read in the modules list one at a time and see if 'rtl' is loaded
      if (sscanf(lineptr, FMT, modname_buf)) {
        modname_buf[BUFSZ-1] = 0;
        if (!strcmp("rtl", modname_buf))
          rtosUsed = RTLinux; // bingo!
      }
    }
    if (lineptr) free(lineptr); // clean up after getline
    fclose(proc_modules);
  }

  return rtosUsed; 
}

void *ShmAttach(const char *name, unsigned long size)
{
  switch(DetermineRTOS()) {
  case RTLinux:
    return mbuff_attach(name, size);
  case RTAI:
    return rtai_shm_attach(name, size);
  }
  return 0;
}

void ShmDetach(const char *name, void *shm_ptr)
{
  switch(DetermineRTOS()) {
  case RTLinux:
    mbuff_detach(name, shm_ptr);
    break;
  case RTAI:
    rtai_shm_detach(name, shm_ptr);
    break;
  }
}

#  else /* !UNIX */
#    error Currently, only UNIX is supported in SysDep.c!
#  endif /* defined UNIX */

#endif /* defined __KERNEL__ */

static struct timespec ns_to_timespec(long long ns)
{
  struct timespec ts;
  ts.tv_nsec = ns % (long long)BILLION;
  ts.tv_sec = ns / (long long)BILLION;
  return ts;
}

struct ThreadArgs
{
  ThreadFunc_t func;
  void *arg;
};

static void *thread_wrapper(void *arg)
{
  struct ThreadArgs *a = (struct ThreadArgs *)arg;
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  return a->func(a->arg);
}

static inline int scheduler_translate(int s)
{
  switch(s) {    
  case RR: s = SCHED_RR; break;
  case FIFO: s = SCHED_FIFO; break;
  case Other: 
  default:
    s = SCHED_OTHER; 
    break;
  }  
  return s;
}

Thread_t thread_create(int prio, int sched, ThreadFunc_t func, void *arg)
{
#define n_static 256
  static struct ThreadArgs args_hack[n_static];
  static unsigned hack_idx = 0;
  struct ThreadArgs *args = &args_hack[hack_idx++%n_static];  
#undef n_static
  pthread_t thr;
  pthread_attr_t attr;
#ifndef __KERNEL__
  if (sched == Default && geteuid() == 0) sched = RR;
  else if (sched == Default) sched = Other;
#else
  if (sched == Default) sched = RR;
#endif
  if (prio < thread_prio_min(sched)) prio = thread_prio_min(sched);
  if (prio > thread_prio_max(sched)) prio = thread_prio_max(sched);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  if (sched != Inherit) {
    struct sched_param sp;
    sched = scheduler_translate(sched);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, sched);
    sp.sched_priority = prio;
    pthread_attr_setschedparam(&attr, &sp);
  } else if (sched == Inherit) {
    pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
  }
#ifdef RTLINUX
  pthread_attr_setfp_np(&attr, 1);
#endif
  args->func = func;
  args->arg = arg;
  (void) prio; /* TODO: implement priorities.. */
  if (pthread_create(&thr, &attr, thread_wrapper, args)) 
    /* TODO handle errors here.. */
    return NULL;
  pthread_attr_destroy(&attr);
  return (Thread_t)thr;
}

void *thread_kill(Thread_t thr)
{
  pthread_t pthr = (pthread_t)thr;
  void *ret = 0;
  pthread_cancel(pthr);
  pthread_join(pthr, &ret);
  if (ret == PTHREAD_CANCELED) ret = 0;
  return ret;
}

void thread_exit(void *return_arg)
{
  pthread_exit(return_arg);
}

void *thread_join(Thread_t thr)
{
  pthread_t pthr = (pthread_t)thr;
  void *ret = 0;
  pthread_join(pthr, &ret);
  return ret;
}

Thread_t thread_current(void)
{
  return (Thread_t)pthread_self();
}

int thread_prio_min(int s)
{
  return sched_get_priority_min(scheduler_translate(s));
}

int thread_prio_max(int s)
{
  return sched_get_priority_max(scheduler_translate(s));
}

int thread_setcancelstate(int state, int *oldstate)
{
  int ret;
  state = state == thread_CancelDisable ? PTHREAD_CANCEL_DISABLE : PTHREAD_CANCEL_ENABLE;
  ret = pthread_setcancelstate( state, oldstate );
  if (oldstate) {
    *oldstate = *oldstate == PTHREAD_CANCEL_DISABLE ? thread_CancelDisable : thread_CancelEnable;
  }
  return ret;
}

int thread_getcancelstate(void)
{
  int old;
  thread_setcancelstate(thread_CancelDisable, &old);
  thread_setcancelstate(old, 0);
  return old;
}

int thread_self_is_rt(void)
{
#if defined(__KERNEL__) && defined(RTLINUX)
  return pthread_self() != pthread_linux();
#else
  return 0;
#endif
}


/*--------------------------------------------------------------------------
  Semaphore stuff.. 
--------------------------------------------------------------------------*/
/** Creates a new semaphore, initializing its value to value, 
    may allocate memory */
Sem_t Sem_create(unsigned int value)
{
  sem_t *sem = (sem_t *)Alloc_mem(sizeof(sem_t));
  if (sem) {
    int ret = sem_init(sem, 0, value);
    if (ret) {
      Free_mem(sem);
      sem = 0;
    }
  }
  return (Sem_t)sem;
}

/** Destroys the semaphore, freeing any resources it may have used */
int  Sem_destroy(Sem_t sem)
{
  int ret = 0;
  if (sem && !(ret = sem_destroy((sem_t *)sem)) )
    Free_mem(sem);
  return ret;
}

/** Just like unix sem_post */
void  Sem_post(Sem_t s)
{
  if (s) sem_post((sem_t *)s);  
}

/** Just like unix sem_wait */
void  Sem_wait(Sem_t s)
{
  if (s) sem_wait((sem_t *)s);
}

/** Just like unix sem_trywait.  Returns nonzero if could not decrease 
    count immediately. */
int  Sem_trywait(Sem_t s)
{
  int ret = 1;
  if (s)  ret = sem_trywait((sem_t *)s);
  return ret;
}

/** Retreives the count of semaphore. */
int Sem_getvalue(Sem_t s)
{
  int ret = 0;
  if (s) sem_getvalue((sem_t *)s, &ret);
  return ret;
}

int Sem_timedwait(Sem_t s, AbsTime_t abs)
{
  struct timespec ts = ns_to_timespec(abs);
  if (s)  return sem_timedwait((sem_t *)s, &ts);
  return 1;
}

/** Creates a new mutex, may allocate memory */
Mut_t mut_create(void)
{
  Mut_t mut = (Mut_t)Alloc_mem(sizeof(pthread_mutex_t));
  if (mut) pthread_mutex_init((pthread_mutex_t *)mut, 0);  
  return mut;
}

/** Destroys the mutex, freeing any memory it may have used. */
int  mut_destroy(Mut_t m)
{
  int ret = 0;
  if (m && !(ret = pthread_mutex_destroy((pthread_mutex_t *)m)) ) 
    Free_mem(m);
  return ret;
}

/** Just like pthread_mutex_lock */
void  mut_lock(Mut_t m)
{
  if (m) pthread_mutex_lock((pthread_mutex_t *)m);
}

/** Just like pthread_mutex_unlock */
void  mut_unlock(Mut_t m)
{
  if (m) pthread_mutex_unlock((pthread_mutex_t *)m);
}

/** Just like pthread_mutex_trylock.  Returns nonzero if could not acquire
    the lock immediately. */
int  mut_trylock(Mut_t m)
{
  if (m) return pthread_mutex_trylock((pthread_mutex_t *)m);
  return 1;
}

Cond_t cond_create(void)
{
  Cond_t c = (Cond_t)Alloc_mem(sizeof(pthread_cond_t));
  if (c) pthread_cond_init((pthread_cond_t *)c, 0);  
  return c;  
}

int   cond_destroy(Cond_t c)
{
  int ret = 0;
  if (c && !(ret =  pthread_cond_destroy((pthread_cond_t *)c)) )
    Free_mem(c);
  return ret;
}

void   cond_signal(Cond_t c)
{
  if (c) pthread_cond_signal((pthread_cond_t *)c);
}

void   cond_broadcast(Cond_t c)
{
  if (c) pthread_cond_broadcast((pthread_cond_t *)c);  
}

void   cond_wait(Cond_t c, Mut_t m)
{
  if (c && m) pthread_cond_wait((pthread_cond_t *)c, (pthread_mutex_t *)m);
}

int   cond_timedwait(Cond_t c, Mut_t m, AbsTime_t a)
{
  int ret = 0;
  if (c && m) {
    struct timespec ts = ns_to_timespec(a);
    ret = pthread_cond_timedwait((pthread_cond_t *)c, 
                                 (pthread_mutex_t *)m, 
                                 &ts);
  }
  return ret;
}

void Strfree(const char *s)
{
  Free_mem((void *)s);
}

long Ffs(unsigned long bits)
{
  if (!bits) return -1;
  return ffs(bits)-1;
}

long Ffz(unsigned long bits)
{
  return Ffs(~bits);
}

void Memcpy(void *dest, const void *src, unsigned long sz)
{
  memcpy(dest, src, sz);
}

void Memset(void *mem, char c, unsigned long num)
{
  memset(mem, c, num);
}

int  Strcmp(const char *s1, const char *s2) { return strcmp(s1,s2); }

/// Just like the C library memcmp
int Memcmp(const void *mem, const void *mem2, unsigned long sz)
{
  return memcmp(mem, mem2, sz);
}

char *Strncpy(char *d, const char *s, unsigned long sz)
{
  return strncpy(d, s, sz);
}

int (* const Snprintf)(char *b, unsigned size, const char *fmt, ...) __attribute__ ((format (printf, 3, 4))) = &snprintf;
