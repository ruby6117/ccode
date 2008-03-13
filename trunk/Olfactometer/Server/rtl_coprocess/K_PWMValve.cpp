#include "K_PWMValve.h"
#include "Module.h"
#include "kcomedilib.h"
#include "K_DAQTask.h"

namespace Kernel
{

PWMValve::PWMValve(DataLogger *l, unsigned lid, const PWMVParams &p, DAQTask *daq)
  : DataLogable(l, lid), hcb(this, true), lcb(this, false), dev(0), ok(true), daq(daq)
{
  setParams(p);
  
  if (!daq) {
    char dev_comedi_string[] = "/dev/comedi0";
    int lastchar = Strlen(dev_comedi_string)-1;
    dev_comedi_string[lastchar] = '0' + p.dev;
    dev = ::comedi_open(dev_comedi_string);
    if (!dev) {
      ok = false;
      Error("Could not create PWM object due to comedi_open error\n");
      return;
    }  
  }
  
}

PWMValve::~PWMValve()
{
  stop();
  if (dev) comedi_close(dev), dev = 0;
}

void PWMValve::Callback::operator()()
{
  int bit = ishigh ? 1 : 0;
  parent->logDatum(double(bit), Cooked);
  if (parent->daq) {
    parent->daq->putSample(parent->params.chan, bit);
  } else {
    comedi_dio_write(parent->dev, parent->params.subdev, parent->params.chan, bit);
  }
}

void PWMValve::start()
{
  PWM::start(&hcb, &lcb, 0, Thread::HighPriority);
}

bool PWMValve::setParams(const PWMVParams &p)
{
  if (p.dutyCycle > 100) return false;
  unsigned t1 = p.windowSizeMicros/100*p.dutyCycle, // time from cycle start to duty off
    t2 = p.windowSizeMicros/100*(100-p.dutyCycle), // time from duty off to cycle start
    tmin = (daq && daq->rateHz()) ? 1000000/daq->rateHz() : 40; // smallest possible time that has meaning, either 40us or sampling rate period
  
  // check for windowsize too small!
  if (p.dutyCycle && t1 < tmin) return false;
  if (p.dutyCycle < 100 && t2 < tmin) return false;  
  
  params = p;
  setTimeScale(Microseconds);
  setWindow(p.windowSizeMicros);
  setDutyCycle(p.dutyCycle);
  return true;
}

}
