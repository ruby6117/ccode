#ifndef PWM_H
#define PWM_H

#include "Thread.h"
#include "Mutex.h"

/** A PWM (pulse-width-modulation) controller.  
    
    The PWM controller can toggle a line 'high' for a percentage of time
    of something called a 'window size'.  It uses callbacks to be 
    application-neutral. The actual control routine runs inside a separate
    thread and calls your callbacks from there.  If you are using this 
    library in an RTOS, it will call your callback in RTOS context. */
class PWM : protected Thread
{
 public:

  enum TimeScale {
    Nanoseconds = 0,
    Microseconds = 3,
    Milliseconds = 6,
  };
  
  PWM();
  virtual ~PWM();

  /// Callback functor for PWM client code to use.
  struct Callback {
    /** Make compiler warnings shut up */
    virtual ~Callback() {}
    /** Called when functor is applied -- depending if it's a lowCallback
        or highCallback -- @see PWM::start(). */
    virtual void operator()() = 0;
  };

  /// returns the PWM window size in Scale units
  unsigned window() const;
  void setWindow(unsigned timeInScaleUnits);
  
  /// returns a number from 0 -> 100 indicating the percentage 
  /// time of the window that we are high
  unsigned dutyCycle() const;
  void setDutyCycle(unsigned d);

  void setTimeScale(TimeScale);
  TimeScale timeScale() const;

  /** @brief starts the PWM for nCycles (0 indicates indefinite).
      
      highCallback is called (operator() is applied) when the PWM wants
      a high state (duty on), lowCallback is called when the PWM wants a low
      state (duty off).
      start actually creates a new thread in the current environment, 
      so callbacks are called from within that new thread.
      NB: callbacks should *not* call back into the PWM class setter methods
      as a deadlock could occur!! */  
  void start(Callback *highCallback, Callback *lowCallback, unsigned nCycles = 0, Thread::Priority = Thread::NormalPriority);
  /// kills the PWM thread, stopping PWM.
  void stop();

protected:
  /// from parent Thread class, implements the thread
  void run();

private:
  long long windowToNS(void) const;
  void windowFromNS(long long ns);
  Callback *highcb, *lowcb;
  TimeScale tscale;
  unsigned _window, duty;
  int nCyclesLeft;
  Mutex mut;
  volatile bool pleaseStop;
  bool needPeriodRecalc;
};

#endif
