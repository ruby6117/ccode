#include "PWM.h"
#include "Timer.h"

static inline int powi(int n, int p)
{
  int ret = 1;
  if (p >= 0) {
    while(p--) ret *= n;
  } else {    
    ret = 0; /* negative powers not yet supported.. */
  }
  return ret;
}


PWM::PWM()
  : highcb(0), lowcb(0), tscale(Milliseconds), _window(100)
{}

PWM::~PWM() 
{}

unsigned PWM::window() const { return _window; }

void PWM::setWindow(unsigned timeInScaleUnits) 
{ 
  mut.lock();
  _window = timeInScaleUnits; 
  needPeriodRecalc = true;
  mut.unlock();
}

unsigned PWM::dutyCycle() const { return duty; }

void PWM::setDutyCycle(unsigned d)  
{ 
  if (d > 100) d = 100;  
  mut.lock();
  duty = d; 
  mut.unlock();
}

long long PWM::windowToNS(void) const
{
  return _window * powi(10, tscale);  
}

void PWM::windowFromNS(long long ns)
{
  _window = ns / powi(10, tscale);
}

void PWM::setTimeScale(TimeScale t) 
{ 
  mut.lock();
  long long w = windowToNS();
  tscale = t;
  windowFromNS(w);
  mut.unlock();
}

PWM::TimeScale PWM::timeScale() const
{
  return tscale;
}

void PWM::start(Callback *h, Callback *l, unsigned n, Thread::Priority p)
{
  highcb = h;
  lowcb = l;
  if (!n) nCyclesLeft = -1;
  else nCyclesLeft = n;
  Thread::start(p);
}

void PWM::stop()
{
  mut.lock();
  pleaseStop = true;
  highcb = lowcb = 0;
  mut.unlock();
  Thread::stop();
}

void PWM::run()
{
  Timer timer;
  mut.lock();
  pleaseStop = false;
  needPeriodRecalc = true;
  while (!pleaseStop && nCyclesLeft) {
    if (needPeriodRecalc) 
      timer.setPeriod(windowToNS()), needPeriodRecalc = false;
    if (duty && highcb) (*highcb)(); // duty cycle on
    if (duty < 100 && lowcb) {
      unsigned saved_duty = duty;
      // duty cycle off if < 100 and lowcb
      // sleep from now until the duty cycle ends
      mut.unlock();
      Timer::nanoSleep(timer.lastWakeupTime() + (timer.period()/100 * saved_duty), 
                       Timer::Absolute);
      mut.lock(); // NB: possible priority inversion here but it's ok since this only happens on param change
      (*lowcb)(); // duty cycle off
    }
    mut.unlock();
    timer.waitNextPeriod();
    mut.lock();
  }
  mut.unlock();
}

// void PWM::run()
// {
//   Timer timer;
//   mut.lock();
//   unsigned saved_window = window(), saved_duty = dutyCycle();
//   TimeScale saved_tscale = timeScale();
//   timer.setPeriod(windowToNS() / 100);
//   pleaseStop = false;  
//   bool inDuty = false;
//   unsigned nDutyLeft = 0, nOffLeft = 0;
//   while (!pleaseStop && nCyclesLeft) {

//     // check for duty cycle change.. if changed, reset
//     if (saved_duty != dutyCycle()) {
//       saved_duty = dutyCycle();
//       inDuty = false;
//       nOffLeft = 0;
//     }

//     if (!inDuty && !nOffLeft) {
//       // start duty cycle
//       inDuty = true;
//       nDutyLeft = duty;
//       if (highcb) (*highcb)(); // call duty-on callback
//     }
//     if (inDuty && !nDutyLeft) {
//       inDuty = false;
//       nOffLeft = 100 - duty;
//       if (lowcb) (*lowcb)(); // call duty-off callback
//       if (nCyclesLeft > 0) --nCyclesLeft;
//     }
//     if (nDutyLeft) --nDutyLeft;
//     if (nOffLeft) --nOffLeft;

//     // recalc period if anything changed..
//     if (saved_window != window() || saved_tscale != timeScale()) {
//       saved_window = window(), saved_tscale = timeScale();      
//       timer.setPeriod(windowToNS() / 100);
//     }

//     mut.unlock();
//     timer.waitNextPeriod();
//     mut.lock();
//   }
//   mut.unlock();
// }
