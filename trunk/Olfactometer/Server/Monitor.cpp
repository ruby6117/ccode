#include "Monitor.h"
#include "Log.h"
#include "Conf.h"
#include <time.h>

static inline bool RoughlyEqual(double a, double b, double thresh = 0.1)
{
  if (b > a) { double c = a; a = b; b = c; }
  if (a-b <= thresh) return true;
  return false;
}

Monitor::Monitor()
  : olf(0), running(false)
{}

Monitor::~Monitor() { stop(); }

void Monitor::stop()
{
  if (running) {
    ::pthread_cancel(thr);
    ::pthread_join(thr, 0);
    running = false;
  }
}

bool Monitor::start(const Settings &settings, Olfactometer * o)
{
  if (running) {
    Error() << "Monitor already running!\n";
    return false;
  }
  if (!o) {
    Error() << "Monitor thread was passed a null olfactometer!\n";
    return false;
  }
  olf = o;
  bool ok;
  gas_panic_secs = String::toInt(settings.get(Conf::Sections::Monitor, Conf::Keys::gas_panic_secs), &ok);
  if (!ok) gas_panic_secs = 1;
  gas_panic_beeps = String::toInt(settings.get(Conf::Sections::Monitor, Conf::Keys::gas_panic_beeps), &ok);
  if (!ok) gas_panic_beeps = 1;

  String gas_panic_actions = settings.get(Conf::Sections::Monitor, Conf::Keys::gas_panic_actions);

  // now parse gas panic actions
  gas_panic_doBeep = false;
  gas_panic_doStop = false;
  StringList actions = gas_panic_actions.split("[[:space:],]+");
  StringList::iterator it;
  for (it = actions.begin(); it != actions.end(); ++it) {
    String item = *it;
    item.trimWS();
    if (item == "beep") { gas_panic_doBeep = true;  }
    else if (item == "stopflow") { gas_panic_doStop = true; }
  }

  running = true;
  if ( ::pthread_create(&thr, 0, threadFuncWrapper, (void *)this) ) {
    Error() << "Monitor thread creation error .. not enough system resources?\n";
    running = false;
    return false;
  }
  return true;
}

void *Monitor::threadFuncWrapper(void * arg)
{
  Monitor *self = (Monitor *)arg;
  self->threadFunc();
  return 0;
}

void Monitor::threadFunc()
{
  for (;;) { // loop infinitely.. note we do get cancelled in d'tor or stop()
    ::pthread_testcancel();
    
    checkGasPanic();

    // we sleep and poll every second..
    static const struct timespec oneSec = { tv_sec : 1, tv_nsec : 0 };
    ::clock_nanosleep(CLOCK_REALTIME, 0, &oneSec, 0);
  }
}

void Monitor::checkGasPanic()
{
  gasPanic.offenders.clear();
  FlowController * fc = 0;
  bool havePanic = false;

  olf->lock();
  std::list<Component *> comps = olf->children(true);
  std::list<Component *>::iterator it;
  for (it = comps.begin(); it != comps.end(); ++it) {
    fc = dynamic_cast<FlowController *>(*it);
    if (fc 
        && !RoughlyEqual(fc->commandedFlow(), 0, 0.01)
        && RoughlyEqual(fc->flow(), 0, fc->errorMagnitude()/2) ) {
      // error condition! commanded flow is not 0 but actual is 0!
      havePanic = true;
      gasPanic.offenders.push_back(fc);
    }
  }
  olf->unlock();

  if (havePanic) {
    if (!gasPanic.panicking) gasPanic.count++;
  } else {
    gasPanic.count = 0;
    gasPanic.panicking = false;
  }
  
  if (gasPanic.count > gas_panic_secs) {
    
    if (!gasPanic.panicking && havePanic) {       // this is true when panic is first detected
      gasPanic.panicking = true;
      std::list<FlowController *>::iterator fcit;
      for (fcit = gasPanic.offenders.begin(); fcit != gasPanic.offenders.end(); ++fcit)
        Error() << "Flow controller " << (*fcit)->name() << " appears to have no gas input supply! Please fix!\n";
    }
        
    doGasPanic(); 
  }
}

static void beep(int hz, int tenth_secs);


void Monitor::doGasPanic()
{
  if (gas_panic_doBeep && gasPanic.alarm_count < gas_panic_beeps) 
  {
    for (int freq = 220; freq < 220*2; ++freq) {
        beep(freq, 1);
        static const struct timespec ts = { tv_sec  : 0, 
                                            tv_nsec : 1000000000/100 };
        ::nanosleep(&ts, 0);
    }
    ++gasPanic.alarm_count;
  } 
  /* test if we already beeped enough times, and if so, stop the offending 
     flow controller (provided specified action is stopflow) */
  else if (gas_panic_doStop
           && ((gas_panic_doBeep && gasPanic.alarm_count >= gas_panic_beeps)
               || !gas_panic_doBeep) )
  {
    std::list<FlowController *>::iterator it;
    olf->lock();
    for (it = gasPanic.offenders.begin(); it != gasPanic.offenders.end(); ++it)
      (*it)->setFlow(0);
    olf->unlock();
    gasPanic.alarm_count = 0; // reset alarm count
  }
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <unistd.h>


void beep(int hz, int tenth_secs)
{
#define THEDEV "/dev/console"
        static const int freq_8254 = 1193180; /* 8254 freq is 1.19 MHz */
        int fd = open(THEDEV, O_SYNC);
        if (fd < 0) {
          //perror("open "THEDEV);
          //abort();
          return;
        }
        // NOTE: assuption is that linux HZ is 100
#define HZ 100
        if ( ioctl(fd, KDMKTONE, ((freq_8254/hz) & 0xffff) | ((tenth_secs*(HZ/10))<<16)) ) {
          //perror("ioctl");
          //    abort();
        }

        close(fd);
}

