#include "RTLCoprocess.h"
#include "Common.h"
#include <stdlib.h>
#include <sys/utsname.h>
#include <fstream>
#include <stdio.h>
#include "Log.h"
#include "Mutex.h"
#include "rtl_coprocess/DataEvent.h"

RTLCoprocess::RTLCoprocess(const char *m)
  : modname(m)
{}

bool RTLCoprocess::reload()
{
  if (modLoaded() && !unload()) return false;    
  return load();  
}

bool RTLCoprocess::load()
{ 
  String mod(modname), cmd = "/sbin/modprobe";
  if (!modLoaded() && ::system(cmd + " " + mod)) { // try modprobe first
    // that failed.. next, try insmod

    // determine kernel version for .ko or .o suffix..
    struct utsname u;
    ::uname(&u);
    double v = String(u.release).toDouble();
    if (v > 2.5) mod += ".ko";
    else mod += ".o";
    cmd = "/sbin/insmod";
    if (::system(cmd + " " + mod) && ::system(cmd + " ./" + mod)) 
      return false;   
  }
  if ( !shm.attach(OlfCoprocessShm_NAME) ) 
      return false;
  else if ( shm->magic != OlfCoprocessShm_MAGIC ) {
    shm.detach();
    return false;
  } else if ( !fifo.open(shm->cmd_fifo) || !fifo_datalog.open(shm->datalog_fifo) ) {
    shm.detach();
    return false;
  }
  data_events.clear();
  num_data_events = 0;
  stopDataEventGrabberThread = false;
  Thread::start();
  return true;
}

bool RTLCoprocess::unload()
{
  stopDataEventGrabberThread = true;
  shm.detach();
  fifo.close();
  fifo_datalog.close();
  Thread::stop();
  return ::system(String("/sbin/rmmod ") + modname); 
}

bool RTLCoprocess::modLoaded() const
{
  bool ret = false;
  std::ifstream ifs;
  ifs.open("/proc/modules", std::ios::in);
  if (!ifs.is_open()) return false;
  while (!ifs.eof()) {
    char buf[65536];
    ifs.getline(buf, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;
    StringList fields = String(buf).split("\\s+");
    if (!fields.size()) continue;
    const String & mod = fields.front();
    if (mod == modname) { ret = true; break; }
  }
  ifs.close();
  return ret;
}

RTLCoprocess::~RTLCoprocess()
{
  unload();
}

//static
const RTLCoprocess::Handle RTLCoprocess::Failure = ~0UL;

RTLCoprocess::Handle RTLCoprocess::createPID(unsigned dlid, const PIDFCParams &params, const std::string & daq_ai_name, const std::string & daq_ao_name)
{
  Cmd c;
  c.cmd = Cmd::Create;
  c.object = Cmd::PID;
  c.pidParams = params;
  c.datalog.id = dlid;
  ::snprintf(c.daqname_in, sizeof(c.daqname_in), "%s", daq_ai_name.c_str());
  ::snprintf(c.daqname_out, sizeof(c.daqname_out), "%s", daq_ao_name.c_str());
  c.daqname_in[sizeof(c.daqname_in)-1] = 0;
  c.daqname_out[sizeof(c.daqname_out)-1] = 0;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  // write the command to kernel and read the response
  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok)
    return c.handle;
  return Failure;
}

RTLCoprocess::Handle RTLCoprocess::createPWM(unsigned dlid, const PWMVParams &params, const std::string &daq)
{
  Cmd c;
  c.cmd = Cmd::Create;
  c.object = Cmd::PWM;
  c.pwmParams = params;
  c.datalog.id = dlid;
  ::snprintf(c.daqname_out, sizeof(c.daqname_out), "%s", daq.c_str());
  c.daqname_out[sizeof(c.daqname_out)-1] = 0;
  
  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  // write the command to kernel and read the response
  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok)
    return h2pwm(c.handle);
  return Failure;
}

bool RTLCoprocess::destroy(Handle h)
{
  Cmd c;
  c.cmd = Cmd::Destroy;
  if (ispwmh(h)) {
    c.object = Cmd::PWM;
    c.handle = pwm2h(h);
  } else if (isdaqh(h)) {
    c.object = Cmd::DAQ;
    c.handle = daq2h(h);
  } else if (ispidh(h)) {
    c.object = Cmd::PID;
    c.handle = pid2h(h);
  }

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  // write the command to kernel and read the response
  return c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status != Cmd::Error;
}



double RTLCoprocess::getFlow(Handle h, bool *ok)
{
  if (!ispidh(h)) { if (ok) *ok = false; return 0.; } // error handle is wrong

  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok ) {
    if (ok) *ok = true;
    return c.pidParams.flow_actual;  
  }
  if (ok) *ok = false;
  return 0.0;
}

bool RTLCoprocess::setFlow(Handle h, double flow)
{
  if (!ispidh(h)) { return false; } // error handle is wrong
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) 
    return false;
  c.cmd = Cmd::Modify;
  c.pidParams.flow_set = flow;
  return  c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok;
}

bool RTLCoprocess::setValveV(Handle h, double v)
{
  if (!ispidh(h)) { return false; } // error handle is wrong
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) 
    return false;
  c.cmd = Cmd::Modify;
  c.pidParams.last_v_out = v;
  return  c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok;
}

bool RTLCoprocess::getControlParams(Handle h,
                                    std::vector<double> & kpkikd_out,
                                    unsigned & num_ctrl_pts_out)
{
  if (!ispidh(h)) { // error handle is wrong
    Error() << "Internal error in RTLCoprocess::getControlParams() -- handle " << h << " appears to be invalid!\n";
    return false; 
  } 
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) {
    Error() << "Internal error in RTLCoprocess::getControlParams() could not query controlParams for " << h << "\n";    
    return false;
  }
  kpkikd_out.resize(3);
  kpkikd_out[0] = c.pidParams.controlParams.Kp;
  kpkikd_out[1] = c.pidParams.controlParams.Ki;
  kpkikd_out[2] = c.pidParams.controlParams.Kd;
  num_ctrl_pts_out = c.pidParams.controlParams.numIntgrlPts;
  return true;
}
bool RTLCoprocess::setControlParams(Handle h,
                                    const std::vector<double> & kpkikd,
                                    unsigned num_ctrl_pts)
{
  if (!ispidh(h)) { 
    Error() << "Internal error in RTLCoprocess::setControlParams() -- handle " << h << " appears to be invalid!\n";
    return false; 
  } // error handle is wrong
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) {
    Error() << "Internal error in RTLCoprocess::setControlParams() could not query controlParams for " << h << "\n";    
    return false;
  }
  c.cmd = Cmd::Modify;
  c.pidParams.controlParams.Kp = kpkikd[0];
  c.pidParams.controlParams.Ki = kpkikd[1];
  c.pidParams.controlParams.Kd = kpkikd[2];
  c.pidParams.controlParams.numIntgrlPts = num_ctrl_pts;
  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) {
    Error() << "Internal error in RTLCoprocess::setControlParams() could not modify controlParams for " << h << "\n";    
    return false;
  }
  return true;
}

bool RTLCoprocess::start(Handle h)
{
  Cmd c;
  c.cmd = Cmd::Start;
  if (ispwmh(h)) {
    c.object = Cmd::PWM;
    c.handle = pwm2h(h);
  } else if (isdaqh(h)) {
    c.object = Cmd::DAQ;
    c.handle = daq2h(h);
  } else {
    c.object = Cmd::PID;
    c.handle = h;
  }

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  return c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok;
}


bool RTLCoprocess::stop(Handle h)
{
  Cmd c;
  c.cmd = Cmd::Stop;
  if (ispwmh(h)) {
    c.object = Cmd::PWM;
    c.handle = pwm2h(h);
  } else if (isdaqh(h)) {
    c.object = Cmd::DAQ;
    c.handle = daq2h(h);
  } else {
    c.object = Cmd::PID;
    c.handle = h;
  }

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  return c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok;
}


bool RTLCoprocess::getLastOutV(Handle h, double & out)
{
  if (!ispidh(h)) return false;
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) 
    return false;
  out = c.pidParams.last_v_out;
  return true;
}
bool RTLCoprocess::getLastInV(Handle h, double & out)
{
  if (!ispidh(h)) return false;
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !c.writeFifo(&fifo) || !c.readFifo(&fifo) || c.status != Cmd::Ok ) 
    return false;
  out = c.pidParams.last_v_in;
  return true;
}
bool RTLCoprocess::setCoeffs(Handle h, double a, double b, double c, double d)
{
  if (!ispidh(h)) return false;
  Cmd cmd;
  cmd.cmd = Cmd::Query;
  cmd.object = Cmd::PID;
  cmd.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !cmd.writeFifo(&fifo) || !cmd.readFifo(&fifo) || cmd.status != Cmd::Ok ) 
    return false;
  cmd.cmd = Cmd::Modify;
  cmd.pidParams.a = a;
  cmd.pidParams.b = b;
  cmd.pidParams.c = c;
  cmd.pidParams.d = d;
  if ( !cmd.writeFifo(&fifo) || !cmd.readFifo(&fifo) || cmd.status != Cmd::Ok ) 
    return false;
  return true;
}

bool RTLCoprocess::setVClip(Handle h, double min, double max)
{
  if (!ispidh(h)) return false;
  Cmd cmd;
  cmd.cmd = Cmd::Query;
  cmd.object = Cmd::PID;
  cmd.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !cmd.writeFifo(&fifo) || !cmd.readFifo(&fifo) || cmd.status != Cmd::Ok
       || min < cmd.pidParams.vmin_ao || max > cmd.pidParams.vmax_ao ) 
    return false;
  cmd.cmd = Cmd::Modify;
  cmd.pidParams.vclip_ao_min = min;
  cmd.pidParams.vclip_ao_max = max;
  if ( !cmd.writeFifo(&fifo) || !cmd.readFifo(&fifo) || cmd.status != Cmd::Ok ) 
    return false;
  return true;
}

bool RTLCoprocess::getVClip(Handle h, double & min, double & max)
{
  if (!ispidh(h)) return false;
  Cmd cmd;
  cmd.cmd = Cmd::Query;
  cmd.object = Cmd::PID;
  cmd.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if ( !cmd.writeFifo(&fifo) || !cmd.readFifo(&fifo) || cmd.status != Cmd::Ok )
    return false;
  min = cmd.pidParams.vclip_ao_min;
  max = cmd.pidParams.vclip_ao_max;
  return true;
}


RTLCoprocess::Handle RTLCoprocess::createDAQ(const std::string & name, unsigned rate, unsigned minor, unsigned sdev, unsigned chan_mask, unsigned range, unsigned aref, bool dio_out, const double *rmin, const double *rmax)
{
  Cmd c;
  c.cmd = Cmd::Create;
  c.object = Cmd::DAQ;
  ::snprintf(c.daqname_in, sizeof(c.daqname_in), "%s", name.c_str());
  c.daqname_in[sizeof(c.daqname_in)-1] = 0;
  c.daqParams.minor = minor;
  c.daqParams.subdev = sdev;
  c.daqParams.chanmask = chan_mask;
  c.daqParams.range = range;
  c.daqParams.aref = aref;
  c.daqParams.rate_hz = rate;
  c.daqParams.dioMode = dio_out ? Cmd::Output : Cmd::Input;
  if (rmin && rmax) {
    c.daqParams.use_override = 1;
    c.daqParams.override_min = static_cast<int>(*rmin * 1e6);
    c.daqParams.override_max = static_cast<int>(*rmax * 1e6);
  } else
    c.daqParams.use_override = 0;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  // write the command to kernel and read the response
  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok)
    return h2daq(c.handle);
  return Failure;  
}

bool RTLCoprocess::getSample(Handle h, unsigned chan, lsampl_t & samp)
{
  if (!isdaqh(h)) return false;
  samp = 0;
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::DAQ;
  c.handle = daq2h(h);
  c.daqGetPut.chan = chan;
  c.daqGetPut.doit = true;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) {
    samp = c.daqGetPut.sample;
    return true;
  }
  return false;
}

bool RTLCoprocess::putSample(Handle h, unsigned chan, lsampl_t samp)
{
  if (!isdaqh(h)) return false;
  Cmd c;
  c.cmd = Cmd::Modify;
  c.object = Cmd::DAQ;
  c.handle = daq2h(h);
  c.daqGetPut.chan = chan;
  c.daqGetPut.sample = samp;
  c.daqGetPut.doit = true;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) 
    return true;
  return false;
}


bool RTLCoprocess::setParams(Handle h, const PWMVParams &p)
{
  if (!ispwmh(h)) return false;
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PWM;
  c.handle = pwm2h(h);

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) {
    c.pwmParams = p;
    c.cmd = Cmd::Modify;
    if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok)       
      return true;
  }
  return false;  
}

bool RTLCoprocess::getParams(Handle h, PWMVParams &out)
{
  if (!ispwmh(h)) return false;
  Cmd c;
  c.cmd = Cmd::Query;
  c.object = Cmd::PWM;
  c.handle = pwm2h(h);

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) {
    out = c.pwmParams;    
    return true;
  }
  return false;    
}

bool RTLCoprocess::setLogging(Handle h, bool b, int t)
{
  Cmd c;
  c.cmd = Cmd::Query;
  if (ispwmh(h)) h = pwm2h(h), c.object = Cmd::PWM;
  else if (isdaqh(h)) return false; // need to use 4 param version of this func
  else if (ispidh(h)) h = pid2h(h), c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) {
    c.cmd = Cmd::Modify;
    if (b) 
      c.datalog.mask |= t;
    else
      c.datalog.mask &= ~t;
    return c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok;
  }
  return false;
}

bool RTLCoprocess::setLogging(Handle h, bool b, unsigned chan, int id)
{
  Cmd c;
  c.cmd = Cmd::Query;
  if (ispwmh(h)) return false;
  else if (ispidh(h)) return false;
  else if (isdaqh(h)) h = daq2h(h), c.object = Cmd::DAQ;
  c.handle = h;
  c.daqGetPut.doit = false;
  if (chan >= 32) return false;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) {
    c.cmd = Cmd::Modify;
    if (b) 
      c.datalog.mask |= 1<<chan;
    else
      c.datalog.mask &= ~(1<<chan), id = -1;
    c.datalog.ids[chan] = id;
    return c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok;
  }
  return false;
}

bool RTLCoprocess::getLogging(Handle h, int t)
{
  Cmd c;
  c.cmd = Cmd::Query;
  if (ispwmh(h)) h = pwm2h(h), c.object = Cmd::PWM;
  else if (isdaqh(h)) h = daq2h(h), c.object = Cmd::DAQ, c.daqGetPut.doit = false, t = 1<<t;
  else if (ispidh(h)) h = pid2h(h), c.object = Cmd::PID;
  c.handle = h;

  // NB: there was a bug where multiple threads would write to the kernel
  // fifo and thus hang the system...
  MutexLocker locker (fifo_mut);

  if (c.writeFifo(&fifo) && c.readFifo(&fifo) && c.status == Cmd::Ok) {
    return c.datalog.mask & t;
  }
  return false;
}

void RTLCoprocess::run()
{
  while (!stopDataEventGrabberThread) {
    DataEvent e;
    fifo_datalog.read(&e, sizeof(e));
    if (stopDataEventGrabberThread) break; // may have gotten a cancellation here
    data_mut.lock();
    data_events.push_back(e);
    if (++num_data_events > max_data_events) 
      data_events.pop_front(), --num_data_events;
    data_mut.unlock();    
  }
}

// from datalog
unsigned RTLCoprocess::numEvents() const
{
  return num_data_events;
}

// from datalog
unsigned RTLCoprocess::getEvents(std::list<DataEvent> & ret, unsigned start, unsigned num, bool erase) 
{
  unsigned real_num = 0;
  ret.clear();
  if (start >= numEvents()) start = 0;
  if (num+start > numEvents()) num = numEvents() - start;
  MutexLocker locker(data_mut);
  std::list<DataEvent>::iterator end = data_events.end(), it = data_events.begin(), first = end;

  for (unsigned i = 0; it != end && i < start+num ; ++i, ++it) 
    if (i >= start) {
      if (first == end) first = it;
      ++real_num;
      ret.push_back(*it);
    }
  end = it;
  if (erase) data_events.erase(first, end), num_data_events -= real_num;
  return real_num;
}

void RTLCoprocess::clearEvents() 
{
  MutexLocker l(data_mut);
  data_events.clear();
  num_data_events = 0;
}

