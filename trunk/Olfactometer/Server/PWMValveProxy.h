#ifndef PWMValveProxy_H
#define PWMValveProxy_H

#include "Olfactometer.h"
#include "RTLCoprocess.h"
#include <vector>
#include "ComediChan.h"
#include "StartStoppable.h"
#include "Controller.h"
#include "DAQTaskProxy.h"
#include "RTProxy.h"
#include "DataLogableProxy.h"
#include "rtl_coprocess/PWMVParams.h"

class PWMValveProxy : public Component, public StartStoppable, public Controller, virtual public RTProxy, virtual public DataLogableProxy
{
public:
  PWMValveProxy(Component *parent, const std::string &name, 
                RTLCoprocess *, 
                const std::string & daq_task_name, 
                const PWMVParams & params,
                const ComediChan &chan_template);
  ~PWMValveProxy();

  bool isOk() const { return ok; }

  PWMVParams getParams() const { return params; }
  bool setParams(const PWMVParams &);

  bool start();///< from start/stoppable
  bool stop();///<from start/stoppable
    
  std::vector<double> controlParams() const; ///< from Controller class
  unsigned numControlParams() const { return 2; } ///< from Controller class

protected:
  /// from Controller class, accepts a vector of size 2: dutyCycle[0-1], windowSizeSecs
  bool controller_SetControlParams(const std::vector<double> &params);
private:
  bool ok;
  PWMVParams params;
};

#endif
