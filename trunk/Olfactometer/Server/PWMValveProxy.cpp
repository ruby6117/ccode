#include "ConsoleUI.h"
#include "PWMValveProxy.h"
#include "ComediOlfactometer.h"

PWMValveProxy::PWMValveProxy(Component *parent, const std::string &name, 
                             RTLCoprocess * copr, 
                             const std::string & daq_task_name, 
                             const PWMVParams & params,
                             const ComediChan & ch)
  : Component(parent, name), params(params)
{
  coprocess = copr;
  handle = coprocess->createPWM(id(), params, daq_task_name);
  ok = handle != RTLCoprocess::Failure;  
  if (ok)  {
    ComediActuator *a = new ComediActuator(this, name + "_Actuator", ch);  
    a->setUnit("B");
  }
}

PWMValveProxy::~PWMValveProxy()
{
  if (handle != RTLCoprocess::Failure) coprocess->destroy(handle);
  handle = RTLCoprocess::Failure;
  ok = false;
}

bool PWMValveProxy::setParams(const PWMVParams &p)
{
  if (!isOk()) return false;
  if (coprocess->setParams(handle, p)) {
    params = p;
    return true;
  }
  return false;
}

/// from Controller class, accepts a vector of size 2
bool PWMValveProxy::controller_SetControlParams(const std::vector<double> &params)
{
  PWMVParams p = this->params;
  p.dutyCycle = static_cast<unsigned>(params[0] * 100.0);
  p.windowSizeMicros = static_cast<unsigned>(params[1] * 1e6);
  return setParams(p);
}

std::vector<double> PWMValveProxy::controlParams() const
{
  std::vector<double> ret(2);
  ret[0] = params.dutyCycle / 100.0;
  ret[1] = params.windowSizeMicros / 1e6;
  return ret;
}

bool PWMValveProxy::start() 
{ 
  if (isStarted()) return false;
  bool ret = coprocess->start(handle);
  setStarted(ret);
  return ret;
}

bool PWMValveProxy::stop() 
{ 
  if (isStopped()) return false;
  bool ret = coprocess->stop(handle);
  setStopped(ret);
  return ret;
}
