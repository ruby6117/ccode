#ifndef RTLCoprocess_H
#define RTLCoprocess_H

#include <vector>
#include <string>

#include "Common.h"

#include "RTShm.h"

/* For param structures.. */
#include "rtl_coprocess/PIDFCParams.h"
#include "rtl_coprocess/PWMVParams.h"
#include "rtl_coprocess/Cmd.h"
#include "rtl_coprocess/Shm.h"
#include "rtl_coprocess/DataEvent.h"

#include "Thread.h"
#include "Mutex.h"
#include "DataLog.h"
#include <list>

class RTLCoprocess : protected Thread, public DataLog
{
public:
  RTLCoprocess(const char *modname);
  ~RTLCoprocess();

  const char *modName() const { return modname; } 

  bool reload(); ///< just like load, but if it's already loded does unload() first
  bool load(); 
  bool unload(); 
  bool modLoaded() const;
  bool running() const { return modLoaded(); }

  typedef unsigned long Handle;
  static const Handle Failure;
  
  Handle createPID(unsigned datalog_id, const PIDFCParams &, const std::string & daq_ai = "", const std::string & daq_ao = "");
  Handle createPWM(unsigned datalog_id, const PWMVParams &, const std::string &daq);
  Handle createDAQ(const std::string & name, unsigned rate, unsigned minor, unsigned sdev, unsigned chan_mask, unsigned range, unsigned aref, bool if_its_dio_is_it_output_mode = true, const double *rangeOvrMin = 0, const double * rangeOvrMax = 0);
  bool start(Handle);
  bool stop(Handle);
  bool destroy(Handle);
  double getFlow(Handle, bool *ok = 0); ///> PID flow controller only
  bool setFlow(Handle, double flow); ///> PID flow controller only
  bool setValveV(Handle, double v_out); ///> PID flow controller when it's not running only!  Used for calibration
  bool getControlParams(Handle h,
                        std::vector<double> & kpkikd_out,
                        unsigned & num_ctrl_pts_out);
  bool setControlParams(Handle h,
                        const std::vector<double> & kpkikd,
                        unsigned num_ctrl_pts);
  bool getVClip(Handle h, double & min, double & max);
  bool setVClip(Handle h, double min, double  max);
  bool setCoeffs(Handle h, double a, double b, double c, double d);
  bool getLastOutV(Handle h, double & out);
  bool getLastInV(Handle h, double & out);
  bool getSample(Handle h, unsigned chan, lsampl_t & samp);
  bool putSample(Handle h, unsigned chan, lsampl_t samp);
  bool setParams(Handle h, const PWMVParams &);
  bool getParams(Handle h, PWMVParams &out);
  bool setLogging(Handle h, bool, int t);
  bool setLogging(Handle h, bool, unsigned chan, int id);
  bool getLogging(Handle h, int t);
  
  /// from DataLog superclass
  unsigned numEvents() const;
  /// from DataLog superclass
  unsigned getEvents(std::list<DataEvent> & out, unsigned start, unsigned num, bool = false);
  /// from DataLog superclass
  void clearEvents();

private:
  static Handle h2pwm(Handle h) { return (h+1) << 12; }
  static Handle pwm2h(Handle h) { return (h >> 12)-1; }
  static Handle h2daq(Handle h) { return (h+1) << 24; }
  static Handle daq2h(Handle h) { return (h >> 24)-1; }
  static bool ispwmh(Handle h) { return !isdaqh(h) && (h & ~( (0x1<<12) - 1 )); }
  static bool isdaqh(Handle h) { return h & ~( (0x1<<24) - 1 ); }
  static bool ispidh(Handle h) { return (h&((0x1<<12)-1)) || !h; }
  static Handle h2pid(Handle h) { return h; }
  static Handle pid2h(Handle h) { return h; }

  const String modname;
  RTShm<OlfCoprocessShm> shm;
  RTFifo fifo, fifo_datalog;
  volatile bool stopDataEventGrabberThread;
  mutable Mutex fifo_mut, data_mut;
  std::list<DataEvent> data_events;
  volatile unsigned num_data_events;
  static const unsigned max_data_events = 65535;
protected:
  void run(); ///< for Thread superclass (grabs data from datalog_fifo)
};

#endif
