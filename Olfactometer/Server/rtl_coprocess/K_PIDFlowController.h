#ifndef PIDFlowController_H
#define PIDFlowController_H

#include "PIDFCParams.h"

#ifdef __KERNEL__
#include "PID.h"
#include "kcomedilib.h"
#include "Mutex.h"
#include "K_DAQTask.h"
#include "K_DataLogable.h"

namespace Kernel
{
  class PIDFlowController : public PID::Callback, protected PID, public Kernel::DataLogable
  {
  public:
    PIDFlowController(DataLogger *logger,
                      unsigned log_id,
                      const PIDFCParams &params, 
                      DAQTask *daq_task_in = 0,
                      DAQTask *daq_task_out = 0);
    ~PIDFlowController();
    void start();
    void stop() { PID::stop(); }
    bool setParams(const PIDFCParams & params);
    PIDFCParams getParams() const;
    
    // nb: interited from parent: void stop();
    double getE();
    void setU(double);
        
    bool isOk() const { return ok; }

  private:
    bool initComedi();
    void uninitComedi();
    double voltsToFlow(double v) const;
    double ais2v(lsampl_t samp) const;
    double aos2v(lsampl_t samp) const;
    lsampl_t aiv2s(double v) const;
    lsampl_t aov2s(double v) const;
    bool writeVolts(double v); ///< write volts v to actual hardware
    double readVolts(bool *ok = 0) const; ///< read volts from actual hardware
    bool ok;
    comedi_t *dev_ai, *dev_ao;
    lsampl_t max_ai, max_ao;
    DAQTask *daq_ai, *daq_ao;
    // NB don't access these in non-realtime kernel thread! Use Cpy() ot Clr() to assign or clear
    PIDFCParams params;

    mutable Mutex mut;
    friend class WriteVFunctor;
    friend class ReadVFunctor;
  };

}
#else
#  error The Kernel::PIDFlowController is a kernel-only class for now!
#endif /* __KERNEL__ */

#endif
