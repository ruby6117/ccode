#ifndef Timer_H
#define Timer_H

#include "SysDep.h"

/** Class implementing a periodic timer.

    Set the timer's period with setPerdiod(), then call waitNextPeriod() to
    reliably sleep until the next task period.  Useful for realtime apps
    or soft realtime apps.  This beats calling usleep() yourself since
    managing the task period time is done for you in this class. 

    Typical usage:

    @code
    Timer t;

    t.setPeriod( 1000000000 / TASK_RATE_HZ );

    while(loopCondition) {
    
    // ... do stuff here ...

       t.waitNextPeriod(); 
    }   
    @endcode
*/
class Timer
{
public:

  Timer();
  ~Timer();

  /// the time is always expressed in nanoseconds
  typedef Time_t Time;
  
  enum SleepType {
    Relative,
    Absolute
  };
  
  /// return the absolute system time
  static Time absTime();

  /// sleep nanos ns, either relative to the current time or relative
  /// to the absolute system time.  If relative to system time, nanos is 
  /// an absolute time value in the future.  Use absTime() to obtain 
  /// the current system time.
  static void nanoSleep(Time nanos, SleepType = Relative);

  /// resets the cycle count (see cycleCount()
  /// and also the relative timer such that elapsed() will start from 0 
  /// and count upwards again
  void reset();

  /// time elapsed since class instantiation or since reset() called
  Time elapsed() const;
    
  static const Time DEFAULT_PERIOD;

  /// resets the periodic timer to start NOW, and also sets the period to 
  /// 'period'.  So the next task period is NOW + period.
  void setPeriod(Time period);  
  /// returns the task period.  Defaults to 1ms (1000000 ns) on a new instance.
  Time period() const;

  /// waits for the next period.  Note that if you call this function
  /// too late, (as in, after the next period) it may return right away!
  /// so as a rule, set your period once,  then call waitNextPeriod() the
  /// first time as soon as you can, so as to avoid missing the first periodic
  /// deadline.
  void waitNextPeriod();

  /// the number of periods that have elapsed, this basically is a count
  /// of waitNextPeriod() calls that we have made since the last reset()!
  unsigned long long cycleCount() const;  
  
  /// the time at which we will wake up if waitNextPeriod is called()
  Time nextWakeupTime() const;
  /// the time at which this cycle started
  Time lastWakeupTime() const;

private:
  unsigned long long cycle;
  Time t0, nextWakeup, per;
  
};

#endif
