#include "Timer.h"
#include "SysDep.h"

const Timer::Time Timer::DEFAULT_PERIOD = 1000000; ///< default to 1ms

Timer::Timer()
{
  reset();
  per = DEFAULT_PERIOD;
}

Timer::~Timer() {}

void Timer::reset()
{
  cycle = 0;
  t0 = absTime();  
}

void Timer::waitNextPeriod() 
{
  nanoSleep(nextWakeup, Absolute);
  ++cycle;
  nextWakeup += per;
}

void Timer::setPeriod(Time p)
{
  per = p;
  nextWakeup = absTime() + per;
}


Timer::Time Timer::period() const {  return per; }
unsigned long long Timer::cycleCount() const {  return cycle; }
Timer::Time Timer::elapsed() const { return absTime() - t0; }

Timer::Time Timer::nextWakeupTime() const { return nextWakeup; }
/// the time at which this cycle started
Timer::Time Timer::lastWakeupTime() const { return nextWakeup-per; }

// static
Timer::Time Timer::absTime() 
{
  return ::abstime_get();
}

// static
void Timer::nanoSleep(Time nanos, SleepType s)
{
  if (s != Absolute) 
    nanos += absTime();
  
  ::abstime_nanosleep(nanos);
}
