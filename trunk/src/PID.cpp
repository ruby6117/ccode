#include "PID.h"
#include "Timer.h"
#include "Thread.h"
#include "Mutex.h"


unsigned PID::MAX_RATE = 50000;  /**< limit rate to 50kHz since most 
                                      platforms can't schedule loops faster 
                                      than that anyway */

PID::PID()
  : cb(0), rateHz(1), pleaseStop(false), currRate(0)
{
  Clr(period);
}

PID::~PID() 
{}

unsigned PID::rate() const { return rateHz; }

bool PID::setRate(unsigned r) 
{ 
  if (r > 0 && r < MAX_RATE) {
    mut.lock();
    rateHz = r;
    mut.unlock();
    return true;
  }
  return false;
}

PID::ControlParams PID::controlParams() const 
{
  ControlParams ret;
  mut.lock();
  ret = *const_cast<ControlParams *>(&cp);
  mut.unlock();
  return ret; 
}

void PID::setControlParams(const ControlParams & c)  
{ 
  mut.lock();
  *const_cast<ControlParams *>(&cp) = c;
  mut.unlock();
}


void PID::start(Callback *c, unsigned r, const ControlParams *p, Thread::Priority prio)
{
  if (!c) return;
  if (running()) stop();
  if (r) setRate(r);
  if (p) setControlParams(*p);
  mut.lock();
  cb = c;
  pleaseStop = false;
  mut.unlock();
  Thread::start(prio);
}

void PID::stop()
{
  mut.lock();
  pleaseStop = true;
  cb = 0;
  mut.unlock();
  Thread::stop(); 
}

void PID::recomputePeriod()
{
  if (currRate != rateHz && rateHz) {
    currRate = rateHz;
    timer.setPeriod(1000000000 / currRate);
    period = timer.period();
  }
}

void PID::run()
{
  setCancelState(false); // make sure we don't get cancelled while holding a mutex, etc
  timer.reset();
  mut.lock();
  state.reset();
  recomputePeriod(); // calls timer.setPeriod, sets some class member vars
  pleaseStop = false;  

  while (!pleaseStop) {
    double e, u = 0;

    mut.unlock();  // release the lock
    e = cb->getE();    // obtain e from client code without the lock
    mut.lock(); // obtain the lock again

    // do stuff, compute u based on e and cp params
    u = doPID(e, period/static_cast<double>(1e6) /* timestep is in millis */);

    recomputePeriod(); // only does something if rate changed..

    mut.unlock();

    cb->setU(u); // apply 'u' computed above with lock released

    timer.waitNextPeriod(); // wait for next period with lock released

    mut.lock(); // acquire the lock again because we are checking pleaseStop
  }

  mut.unlock(); // at this point lock is always held, release it
}


#define ABS(x) ( (x) < 0 ? -(x) : (x) )
double PID::doPID(double e, double timestep)
{
  double iTerm = 0.0, dTerm = 0.0;

  if (cp.numIntgrlPts) {
    while (state.num_pts >= cp.numIntgrlPts) {
      // we reached max integral points, so delete the area of the first (oldest) point in our list
      state.i -=  state.pts.front();
      state.pts.pop_front();
      --state.num_pts;
    }
    double thisPt = e/++state.num_pts;
    state.i += thisPt;
    state.pts.push_back(thisPt);
    iTerm = state.i;
  }
 
  // calculate the derivative term
  if (state.d_undef || ABS(timestep - 0.0) < 0.000001) {
    // d is undefined.. just say slope is 0
    dTerm = 0.0;
    state.d_undef = false;
  } else {
    // d is defined, calculate derivative
    dTerm = (e - state.d) / timestep;
  }
  // save previous e value for state.d
  state.d = e;

  // u = Kp * e + Ki * (integral of e) + Kd * (derivative of e)

  return cp.Kp*e + cp.Ki*iTerm + cp.Kd*dTerm;
}
