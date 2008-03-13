#include "PIDFlowController.h"
#include "Conf.h"
#include "ConfParse.h"
#include <boost/regex.hpp>
#include "ProbeComedi.h"
#include "Log.h"
#include <comedilib.h>
#include "Common.h"
#include <sstream>
#include <algorithm>


PIDFlowController::
PIDFlowController(Settings & ini,
                  RTLCoprocess * coprocess, 
                  const ComediChan &rd, const ComediChan &wr, 
                  const ComediChan *he, const ComediChan *se,
                  Component *parent, const std::string &fcname)
  : FlowController(0, 0, parent, fcname), DOEnableable(this, he, se),
    valid(false), ini(ini)
    
{  
  this->coprocess = coprocess;
  params.dev_ai = rd.minor();
  params.subdev_ai = rd.subDev();
  params.chan_ai = rd.chan();
  params.dev_ao = wr.minor();
  params.subdev_ao = wr.subDev();
  params.chan_ao = wr.chan();
  params.flow_set = 0;
  params.flow_actual = 0;
  if (!Calib::load(ini, fcname)) {
      error_str = String("Configuration file error: could not load either curve fit coefficients or calibration table for ") + fcname + "!\n";    
      valid = false;
      return;
  } else if (coeffs.size() != 4) {
      error_str = String("Configuration file error: need precisely 4 curve fit coefficients for ") + fcname + "!\n";
      valid = false;
      return;
  }
  params.a = coeffs[0];
  params.b = coeffs[1];
  params.c = coeffs[2];
  params.d = coeffs[3];
  
  params.rate_hz = String(ini.get(fcname, Conf::Keys::update_rate_hz)).toUInt();
  if (params.rate_hz <= 0 || params.rate_hz > 1000) {
    error_str = String("Internal Error: Could not create PID Flow Controller ") + fcname + " since the update_rate_hz parameter of " + params.rate_hz + " is out of valid range.";
    valid = false;
    return;
  }
  std::string overrd = ini.get(fcname, Conf::Keys::read_range_override);
  static const boost::regex numRangeRE("^([[:digit:].-]+)-([[:digit:].-]+)$");
  if (overrd.length()) {
    boost::cmatch caps;
    if (!boost::regex_match(overrd.c_str(), caps, numRangeRE) ) {
      std::ostringstream os;
      os << "Configuration file error: " << fcname << " failed to read/parse " << Conf::Keys::read_range_override << "!\n";    
      error_str = os.str();
      valid = false;
      return;
    }
    params.vmin_ai = ToDouble(caps[1].str());
    params.vmax_ai = ToDouble(caps[2].str());
  } else {
    params.vmin_ai = rd.rangeMin();
    params.vmax_ai = rd.rangeMax();
  }
  if (params.vmax_ai - params.vmin_ai < 0.01) {
    std::ostringstream os;
    os << "Configuration file error: " << fcname << " has too small a " << Conf::Keys::read_range_override << " of " << overrd << "!\n";    
    error_str = os.str();
    valid = false;
    return;    
  }
  overrd = ini.get(fcname, Conf::Keys::write_range_override);
  if (overrd.length()) { 
    boost::cmatch caps;
    if (!boost::regex_match(overrd.c_str(), caps, numRangeRE) ) {
      std::ostringstream os;
      os << "Configuration file error: " << fcname << " failed to read/parse " << Conf::Keys::write_range_override << "!\n";    
      error_str = os.str();
      valid = false;
      return;
    }
    params.vmin_ao = ToDouble(caps[1].str());
    params.vmax_ao = ToDouble(caps[2].str());
  } else {
    params.vmin_ao = wr.rangeMin();
    params.vmax_ao = wr.rangeMax();
  }
  if (params.vmax_ao - params.vmin_ao < 0.01) {
    std::ostringstream os;
    os << "Configuration file error: " << fcname << " has too small a " << Conf::Keys::write_range_override << " of " << overrd << "!\n";    
    error_str = os.str();
    valid = false;
    return;    
  }
  std::string clip = ini.get(fcname, Conf::Keys::write_range_clip);
  if (clip.length()) {
    boost::cmatch caps;
    if (!boost::regex_match(clip.c_str(), caps, numRangeRE) ) {
      std::ostringstream os;
      os << "Configuration file error: " << fcname << " failed to read/parse " << Conf::Keys::write_range_clip << "!\n";    
      error_str = os.str();
      valid = false;
      return;
    }
    params.vclip_ao_min = ToDouble(caps[1].str());
    params.vclip_ao_max = ToDouble(caps[2].str());
  } else {
    params.vclip_ao_min = params.vmin_ao;
    params.vclip_ao_max = params.vmax_ao;
  }

  if (params.vclip_ao_max - params.vclip_ao_min < 0.01) {
    std::ostringstream os;
    os << "Configuration file error: " << fcname << " has too small a " << Conf::Keys::write_range_clip << " of " << clip << "!\n";    
    error_str = os.str();
    valid = false;
    return;    
  }

  params.controlParams.Kp = String(ini.get(fcname, Conf::Keys::Kp)).toDouble(&valid);
  if (!valid) {
    error_str = String("Configuration file error: ") + fcname + " missing valid " + Conf::Keys::Kp + "\n";    
    return;
  }
  params.controlParams.Ki = String(ini.get(fcname, Conf::Keys::Ki)).toDouble(&valid);
  if (!valid) {
    error_str = String("Configuration file error: ") + fcname + " missing valid " + Conf::Keys::Ki + "\n";
    return;
  }
  params.controlParams.Kd = String(ini.get(fcname, Conf::Keys::Kd)).toDouble(&valid);
  if (!valid) {
    error_str = String("Configuration file error: ") + fcname + " missing valid " + Conf::Keys::Kd + "\n";
    return;
  }
  params.controlParams.numIntgrlPts = String(ini.get(fcname, Conf::Keys::num_Ki_points)).toUInt(&valid);
  if (!valid) {
    error_str = String("Configuration file error: ") + fcname + " missing valid " + Conf::Keys::num_Ki_points + "\n";
    return;
  }
  if (!rd.getDAQ()) {
    error_str = String("Configuration file error: ") + fcname + " needs a rt_daq_task for its read channel specification!\n";
    return;
  }
  if (!wr.getDAQ()) {
    error_str = String("Configuration file error: ") + fcname + " needs a rt_daq_task for its write channel specification!\n";
    return;
  }
  std::string daq_ai_name = rd.getDAQ()->name();
  std::string daq_ao_name = wr.getDAQ()->name();

  if ( (handle = coprocess->createPID(id(), params, daq_ai_name, daq_ao_name)) == RTLCoprocess::Failure ) {
    error_str = String("Internal Error: Could not create PID Flow Controller ") + fcname + " as the realtime coprocess returned a failure status.\n";
    valid = false;
    return;
  }


  if (!start()) {
    error_str = String("Internal Error: Could not start PID Flow Controller ") + fcname + " as the realtime coprocess returned a failure status.\n";
    valid = false;
  }
  PIDFlowSensorProxy *sp;
  PIDFlowActuatorProxy *ap;
  sensor = sp = new PIDFlowSensorProxy(0, name() + "_Sensor", rd);
  actuator = ap = new PIDFlowActuatorProxy(0, name() + "_Actuator", wr);
  // this awkward reparenting here is necessary so that FlowMeter::childAdded() picks up this new sensor
  sensor->reparent(this);
  // ditto for FlowController::childAdded()
  actuator->reparent(this);
  sp->pfc = this;
  ap->pfc = this;
  valid = true;

  Log() << "PID FlowController " << name() << " at " << params.rate_hz << "Hz using " << daq_ai_name << ":" << rd.chan() << ", " << daq_ao_name << ":" << wr.chan() << "\n"; 

}

PIDFlowController::~PIDFlowController()
{
  stop();
  if (handle != RTLCoprocess::Failure) 
    coprocess->destroy(handle);
  handle = RTLCoprocess::Failure;  
}

bool PIDFlowController::start()
{
  setStarted(coprocess->start(handle));
  return isStarted();
}

bool PIDFlowController::stop()
{
  setStopped(coprocess->stop(handle));  
  return isStopped();
}

bool PIDFlowController::getControlParams(std::vector<double> & ret1, unsigned & ret2) const
{
  return handle != RTLCoprocess::Failure &&
    const_cast<RTLCoprocess *>(coprocess)->getControlParams(handle, ret1, ret2);
}

bool PIDFlowController::setControlParams(const std::vector<double> & kpkikd, unsigned n) 
{
  return handle != RTLCoprocess::Failure &&
    coprocess->setControlParams(handle, kpkikd, n);
}

PIDFlowSensorProxy::PIDFlowSensorProxy(PIDFlowController *parent,
                                       const std::string &name,
                                       const ComediChan & aio_params)
  : ComediSensor(parent, name,  aio_params)
{
  pfc = parent;
}

PIDFlowActuatorProxy::PIDFlowActuatorProxy(PIDFlowController *parent,
                                           const std::string &name,
                                           const ComediChan & aio_params)
  : ComediActuator(parent, name, aio_params)
{
  pfc = parent;
}


double PIDFlowSensorProxy::read() 
{ 
  return pfc ? pfc->coprocess->getFlow(pfc->handle) : 0.;
}

double PIDFlowSensorProxy::readRaw() 
{ 
  double ret = 0.;
  pfc && pfc->coprocess && pfc->coprocess->getLastInV(pfc->handle, ret);
  return ret;
}

bool PIDFlowActuatorProxy::write(double d) 
{ 
  bool ret = pfc && pfc->coprocess->setFlow(pfc->handle, d);
  if (ret) m_lastWritten = d;
  return ret;
}

bool PIDFlowActuatorProxy::writeRaw(double v) 
{ 
  bool ret = pfc && pfc->coprocess->setValveV(pfc->handle, v);
  if (ret) m_lastWrittenRaw = v;
  return ret;
}

double PIDFlowActuatorProxy::lastWrittenRaw() const
{
  double ret = 0.;
  pfc && pfc->coprocess && pfc->coprocess->getLastOutV(pfc->handle, ret);
  return ret;
}


/// implement pure virtual Calib::setCoeffs
bool PIDFlowController::setCoeffs(const Coeffs & c)
{
  if (c.size() != 4) return false;
  if (coprocess->setCoeffs(handle, c[0], c[1], c[2], c[3])) {
    coeffs = c;
    return true;
  }
  return false;
}

std::vector<double> PIDFlowController::controlParams() const
{
  std::vector<double> ret;
  unsigned npts;
  if (getControlParams(ret, npts)) ret.push_back(static_cast<double>(npts));
  else ret.resize(4);
  return ret;
}

bool PIDFlowController::controller_SetControlParams(const std::vector<double> & p)
{
  std::vector<double> dbls = p;
  unsigned npts = static_cast<unsigned>(dbls.back());
  dbls.pop_back();
  return setControlParams(dbls, npts);
}


/// saves the control params to the settings file
bool PIDFlowController::saveParams()
{
  ini.put(name(), Conf::Keys::Kp, String::Str(params.controlParams.Kp));
  ini.put(name(), Conf::Keys::Ki, String::Str(params.controlParams.Ki));
  ini.put(name(), Conf::Keys::Kd, String::Str(params.controlParams.Kd));
  return ini.save();
}

bool PIDFlowController::save()
{  
  return Calib::save(ini, name()) && saveParams();
}

/// from our own interface -- sets the output voltage clipping
bool PIDFlowController::setVClip(double min, double max)
{
  return coprocess->setVClip(handle, min, max);
}

/// from our own interface -- query the output voltage clipping
bool PIDFlowController::getVClip(double & min, double & max)
{
  return coprocess->getVClip(handle, min, max);
}
