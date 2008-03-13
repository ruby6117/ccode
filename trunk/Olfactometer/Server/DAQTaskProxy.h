#ifndef DAQTaskProxy_H
#define DAQTaskProxy_H

#include <string>
#include "RTLCoprocess.h"
#include <comedilib.h>
#include "ComediDevice.h"
#include "RTProxy.h"

class DAQTaskProxy : public RTProxy
{
public:
  DAQTaskProxy(const std::string & name, RTLCoprocess *, unsigned rate_hz,
               const ComediSubDevice *sdev, 
               unsigned fromchan, unsigned tochan, 
               unsigned range = 0, unsigned aref = 0,
               bool ifDIOIsOutput = true,
               const double * rangeOvrMin = 0, const double *rangeOvrMax = 0);
  ~DAQTaskProxy();

  bool start();

  const std::string & name() const { return nam; }
  unsigned minor() const { return m_minor; }
  unsigned sdev() const { return m_sdev; }
  unsigned range() const { return m_range; }
  unsigned aref() const { return m_aref; }
  unsigned fromChan() const { return m_fromChan; }
  unsigned toChan() const { return m_toChan; }
  unsigned rateHz() const { return m_rate; }

  lsampl_t readSample(unsigned chan, bool *ok = 0);
  bool writeSample(unsigned chan, lsampl_t samp);

  bool setDataLogging(unsigned chan, bool, unsigned id);
  bool getDataLogging(unsigned chan);

private:
  std::string nam;
  unsigned m_minor, m_sdev, m_range, m_aref, m_fromChan, m_toChan, m_rate;
  bool m_isdio_out;
};


#endif
