#ifndef Monitor_H
#define Monitor_H

#include "Olfactometer.h"
#include "Settings.h"
#include "Common.h"
#include <pthread.h>
#include <list>

class Monitor
{
public:
  Monitor();
  ~Monitor();

  bool start(const Settings & settings, Olfactometer *olf);
  void stop();

private: /* funcs */
  static void *threadFuncWrapper(void *);
  void threadFunc();

  void checkGasPanic();
  void doGasPanic();

private: /* data */
  Olfactometer *olf;
  bool running;
  pthread_t thr;
  int gas_panic_secs;
  int gas_panic_beeps;
  bool gas_panic_doBeep, gas_panic_doStop;

  struct GasPanicState {
    GasPanicState() :  panicking(false), count(0), alarm_count(0) {}
    std::list<FlowController *> offenders;
    bool panicking;
    int count;
    int alarm_count;
  } gasPanic;

};


#endif
