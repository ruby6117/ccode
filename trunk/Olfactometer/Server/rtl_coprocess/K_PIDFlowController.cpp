#include "K_PIDFlowController.h"
#include "K_DAQTask.h"
#include "Module.h"
#include "kcomedilib.h"

namespace Kernel 
{

PIDFlowController::PIDFlowController(DataLogger *l, unsigned log_id, const PIDFCParams &p, DAQTask *dt_in, DAQTask *dt_out)
  :  DataLogable(l, log_id), ok(false), dev_ai(0), dev_ao(0), params(p)
{
  daq_ai = dt_in;
  daq_ao = dt_out;
  if (!initComedi()) { 
    uninitComedi();
    return;
  }
  if (!setRate(p.rate_hz)) {
    Error("PIDFlowController rate of %u is out of range.\n", p.rate_hz);
    uninitComedi();
    return;
  }
  /*  if (daq_ai && daq_ao) {
    Msg("PIDFlow using daq tasks %s, %s\n", daq_ai->name(), daq_ao->name());
    }*/
  ok = true;
}

PIDFlowController::~PIDFlowController()
{
  stop();
  uninitComedi();
}

bool PIDFlowController::initComedi()
{
  const PIDFCParams &p = params;

  char dev_comedi_string[] = "/dev/comedi0";
  int lastchar = Strlen(dev_comedi_string)-1;
  dev_comedi_string[lastchar] = '0' + p.dev_ai;
  dev_ai = ::comedi_open(dev_comedi_string);
  if (!dev_ai) {
    Error("PIDFlowController cannot open %s\n", dev_comedi_string);
    return false;
  }
  dev_comedi_string[lastchar] = '0' + p.dev_ao;
  dev_ao = ::comedi_open(dev_comedi_string);
  if (!dev_ao) {
    Error("PIDFlowController cannot open %s\n", dev_comedi_string);
    return false;
  }
  int sai = p.subdev_ai, sao = p.subdev_ao, cai = p.chan_ai, cao = p.chan_ao;
  if (sai >= ::comedi_get_n_subdevices(dev_ai))  return false;
  if (sao >= ::comedi_get_n_subdevices(dev_ao))  return false;
  if (cai >= ::comedi_get_n_channels(dev_ai, p.subdev_ai))  return false;
  if (cao >= ::comedi_get_n_channels(dev_ao, p.subdev_ao))  return false;
  
  max_ai = ::comedi_get_maxdata(dev_ai, p.subdev_ai, p.chan_ai);
  max_ao = ::comedi_get_maxdata(dev_ao, p.subdev_ao, p.chan_ao);
  
  setControlParams(p.controlParams);
  return true;
}

void PIDFlowController::uninitComedi()
{
  if (dev_ai) comedi_close(dev_ai);
  if (dev_ao) comedi_close(dev_ao);
  dev_ai = dev_ao = 0;
}

void PIDFlowController::start()
{
  PID::start(this, params.rate_hz, &params.controlParams);
}


double 
PIDFlowController::voltsToFlow(double v) const
{
  return params.a*(v*v*v) + params.b*(v*v) + params.c*(v) + params.d;
}

double
PIDFlowController::getE()
{
  mut.lock();
  params.last_v_in = readVolts(&ok);
  if (!ok) {
      mut.unlock();
      Error("AI read error\n");
      return 0.0;
  }
  params.flow_actual = voltsToFlow(params.last_v_in);
  double e = params.flow_actual - params.flow_set;  
  mut.unlock();
  logDatum(params.last_v_in, Raw, "vin");
  logDatum(e, Other, "e");
  logDatum(params.flow_actual, Cooked, "flow");
  return e;
}

bool PIDFlowController::writeVolts(double v)
{
  lsampl_t samp = aov2s(v);
  int ret;
  if (daq_ao) {
    ret = daq_ao->putSample(params.chan_ao, samp) ? 1 : 0;
  } else {
    ret = comedi_data_write(dev_ao, params.subdev_ao, params.chan_ao, 0, 0, samp);
  }
  return ret >= 1;
}

double PIDFlowController::readVolts(bool *ok) const
{ 
  lsampl_t samp = 0;
  if (ok) *ok = true;
  if (daq_ai) {
      samp = daq_ai->getSample(params.chan_ai);
  } else if ( comedi_data_read(dev_ai, params.subdev_ai, params.chan_ai, 0, 0, &samp) < 1 ) {
      if (ok) *ok = false;
      return 0.0;
  }
  return ais2v(samp); 
}

void PIDFlowController::setU(double u)
{
  logDatum(u, Other, "u");
  mut.lock();
  params.last_v_out += u;
  if (params.last_v_out > params.vclip_ao_max) params.last_v_out = params.vclip_ao_max;
  else if (params.last_v_out < params.vclip_ao_min) params.last_v_out = params.vclip_ao_min;
  bool ret = writeVolts(params.last_v_out);

  mut.unlock();
  if ( !ret ) {
    Error("AO write error\n");
    ok = false;
    return;
  }
  logDatum(params.last_v_out, Raw, "vout");
}

struct ReadVFunctor : public Thread::Functor
{
  PIDFlowController *p;
  ReadVFunctor(PIDFlowController *p) : p(p) {}
  void operator()() { 
    p->params.last_v_in = p->readVolts();
    p->params.flow_actual = p->voltsToFlow(p->params.last_v_in);
  }
};

PIDFCParams PIDFlowController::getParams() const
{
  PIDFCParams ret;
  mut.lock();
  if (!running()) { // make sure to physically read hardware if PID loop is not running
    ReadVFunctor f(const_cast<PIDFlowController *>(this));
    Thread::doFuncInRT(f); // need to call readVFunctor in RT since we are in linux context here and we will be using doubles which use FPU regs
  }
  ret = params;
  mut.unlock();
  return ret;
}

struct WriteVFunctor : public Thread::Functor
{
  PIDFlowController *p;
  WriteVFunctor(PIDFlowController *p) : p(p) {}
  void operator()() { 
    p->writeVolts(p->params.last_v_out); 
  }
};

bool PIDFlowController::setParams(const PIDFCParams & pin)
{
  mut.lock();
  if (running() && (params.dev_ai != pin.dev_ai || params.dev_ao != pin.dev_ao
                    || params.subdev_ai != pin.subdev_ai
                    || params.subdev_ao != pin.subdev_ao)) {
    mut.unlock();
    return false;
  }
  double flow_actual_backup, v_in_backup, v_out_backup; // don't overwrite our params for these 3 since our notion is more accurate 
  Cpy(flow_actual_backup, params.flow_actual);
  Cpy(v_in_backup, params.last_v_in);
  Cpy(v_out_backup, params.last_v_out);
  params = pin;
  Cpy(params.flow_actual, flow_actual_backup);
  Cpy(params.last_v_in, v_in_backup);
  if (!running()) {
    WriteVFunctor f(const_cast<PIDFlowController *>(this));
    Thread::doFuncInRT(f); // force user-set voltage if PID loop isn't running
  } else { // otherwise if PID loop is running don't tinker with voltage
    Cpy(params.last_v_out, v_out_backup);
  }
  setRate(params.rate_hz);
  setControlParams(params.controlParams);
  mut.unlock();
  return true;
}


static inline double sample2Volts(lsampl_t samp, lsampl_t maxdata, double min, double max)
{
  return static_cast<double>(samp)/static_cast<double>(maxdata) * (max-min) + min;
}


static inline lsampl_t volts2Sample(double v, lsampl_t maxdata, double min, double max)
{
  return static_cast<lsampl_t>( maxdata * (v-min)/(max-min) );
}

double PIDFlowController::ais2v(lsampl_t s) const
{
  return sample2Volts(s, max_ai, params.vmin_ai, params.vmax_ai);
}

double PIDFlowController::aos2v(lsampl_t s) const
{
  return sample2Volts(s, max_ao, params.vmin_ao, params.vmax_ao);
}
lsampl_t PIDFlowController::aiv2s(double v) const
{
  return volts2Sample(v, max_ai, params.vmin_ai, params.vmax_ai);
}
lsampl_t PIDFlowController::aov2s(double v) const
{
  return volts2Sample(v, max_ao, params.vmin_ao, params.vmax_ao);
}


} // end namespace Kernel
