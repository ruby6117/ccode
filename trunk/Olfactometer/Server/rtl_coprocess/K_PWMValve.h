#ifndef PWMValve_H
#define PWMValve_H

#include "PWMVParams.h"

#ifdef __KERNEL__
#  include "kcomedilib.h"
#  include "PWM.h"
#  include "K_DataLogable.h"
namespace Kernel
{

class DAQTask;

class PWMValve : protected PWM, public Kernel::DataLogable
{
public:
  PWMValve(DataLogger *l, unsigned log_id, const PWMVParams &, DAQTask *daq = 0);
  ~PWMValve();
  PWMVParams getParams() const { return params; }
  bool setParams(const PWMVParams &);
  bool isOk() const { return ok; }
  void start();

  using PWM::stop; // pull function to public

private:
  class Callback : public PWM::Callback {
  public:
    Callback(PWMValve *parent, bool b) : parent(parent), ishigh(b){}
    void operator()();
  private:
    PWMValve *parent;
    bool ishigh;
  };
  Kernel::PWMValve::Callback hcb, lcb;  
  friend class Kernel::PWMValve::Callback;
  comedi_t *dev;
  PWMVParams params;
  bool ok;
  DAQTask *daq;
};

}

#else
#  error Kernel::PWMValve is a kernel-only class for now!
#endif /* __KERNEL__ */

#endif
