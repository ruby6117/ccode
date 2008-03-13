#include "PWM.h"
#include "Timer.h"
extern "C" {
#include <linux/comedilib.h>
}
#include "test.h"
struct CB : public PWM::Callback
{
  CB(bool highlow)
    : hl(highlow), dev(0), s(0), c(0)
  {
    t0 = Timer::absTime();
  }
  void operator()() 
  {
    //Timer::Time t = Timer::absTime() - t0;
    if (hl) {
      ::comedi_data_write(dev, s, c, 0, 0, ::comedi_get_maxdata(dev, s, c));
    }  else {
      ::comedi_data_write(dev, s, c, 0, 0, 0);
    }
  }

  Timer::Time t0;
  bool hl;
  comedi_t *dev;
  unsigned s;
  unsigned c;
};


PWM *p = 0;
CB *h = 0, *l = 0;
extern "C" int doIt(comedi_t *dev, unsigned sdev, unsigned chan, unsigned w_us, unsigned dc, unsigned n)
{  
  p = new PWM;
  h = new CB(true);
  l = new CB(false);
  l->dev = h->dev = dev;
  h->s = l->s = sdev;
  h->c = l->c = chan;
  p->setTimeScale(PWM::Microseconds);
  p->setWindow(w_us);
  p->setDutyCycle(dc);
  p->start(h, l, n);
  return 0;
}

extern "C" void stopIt(void)
{
  p->stop();
  delete p;
  delete h;
  delete l;
}
