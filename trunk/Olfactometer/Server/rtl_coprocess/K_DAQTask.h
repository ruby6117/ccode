#ifndef DAQTask_H
#define DAQTask_H

#ifndef __KERNEL__
#  include <comedilib.h>
#else
#  include "kcomedilib.h"
#endif
#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "Timer.h"
#include "K_DataLogger.h"

namespace Kernel {

class DAQTask : protected Thread
{
public:
  static const unsigned MAX_CHANS = sizeof(unsigned)*8;

  /** Construct a DAQTask.  Note that a rate_hz of 0 is ok, it means
      the DAQ task doesn't run as a periodic thread but instead
      acts as a multiplexer of a comedi subdevice, where it
      can be used to share 1 comedi subdevice amongst multiple
      threads (start() still needs to be called in that case,
      because it does use a realtime thread for the multiplexing).
      If rate_hz is nonzero then isntead a periodic thread runs to acquire 
      samples and calls to getSample() gets the last sample read
      and putSample() enqueues a sample to be written when the 
      periodic thread runs again. */
  DAQTask(const char *name,
          unsigned rate_hz, 
          unsigned minor, unsigned subdev, 
          unsigned chan_mask = ~0U,
          unsigned range = 0, unsigned aref = 0,
          DataLogger *logger = 0, const int *override_min = 0, const int *override_max = 0);
  ~DAQTask();
  lsampl_t getSample(unsigned chan_id) const;
  bool putSample(unsigned chan_id, lsampl_t sample);
  
  /// the scan is read in such a way that it contains only the samples in the chan_mask next to each other so 10101 is chans: 0,2,4 in the out buf!
  unsigned getScan(lsampl_t *outbuf, unsigned num_in_buf = MAX_CHANS) const;

  /// write a whole scan at a time -- not sure how useful this is
  bool putScan(const lsampl_t *buf, unsigned num_in_buf = MAX_CHANS);

  bool isOk() const { return dev; }
  bool isRead() const { return is_read; }
  bool isWrite() const { return is_write; }
  unsigned chanMask() const { return chan_mask; }
  /** set this to a read (input) device -- note that this only does something for DIO chans.  
  */
  void setDIOInput();
  /** set this to a write (output) device -- note that this only does something for DIO chans.  
  */
  void setDIOOutput();
  using Thread::start;
  using Thread::running;
  void stop();

  unsigned rateHz() const { return rate; }

  const char *name() const { return namestr; }

  void setDataLogging(unsigned chan, int id);
  int getDataLogging(unsigned chan) const;

  unsigned numChans() const { return nchans; }

protected:
  void run();
private:
  void uninitComedi();
  void configDIOChans();
  DAQTask(const DAQTask &d) : Thread() { (void)d; }
  DAQTask & operator=(const DAQTask &d)  { (void)d; return *this; }

  bool is_read, is_write, is_dig;

  volatile bool pleaseStop;
  Timer timer;
  mutable Mutex mut;
  char *namestr;
  comedi_t *dev;
  unsigned subdev, chan_mask, range, aref, nchans, changed_mask, rate, maxdata;  
  comedi_krange krange;
  double range_min, range_max;
  bool need_range_calc;
  int subd_type;
  volatile lsampl_t scan[MAX_CHANS];
  int datalogging[MAX_CHANS]; // if non-negative, log channel using ID
  // for rate=0 requests
  mutable Condition cond_req, cond_reply;
  mutable unsigned req_chanmask;
  void doIO(unsigned mask); ///< the actual function that does the IO called from run()
  void doDataLogging(unsigned mask);
  DataLogger *logger;
};

}

#endif
