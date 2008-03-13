#include "DAQTaskProxy.h"
#include "RTLCoprocess.h"
#include <string>

DAQTaskProxy::DAQTaskProxy(const std::string & name_in, 
                           RTLCoprocess *coprocess, 
                           unsigned rate_hz,
                           const ComediSubDevice *sdev, 
                           unsigned fromchan, 
                           unsigned tochan, 
                           unsigned range, 
                           unsigned aref,
                           bool dio_out,
                           const double * rangeOvrMin, const double *rangeOvrMax)
  : nam(name_in)
{
  this->coprocess = coprocess;
  unsigned mask = 0;
  for (unsigned i = fromchan; i <= tochan && i < sdev->nChans; ++i) mask |= 0x1<<i;
  handle = coprocess->createDAQ(name(), rate_hz, sdev->minor, sdev->id, mask, range, aref, dio_out, rangeOvrMin, rangeOvrMax);
  m_fromChan = fromchan;
  m_toChan = tochan;
  m_sdev = sdev->id;
  m_minor = sdev->minor;
  m_aref = aref;
  m_range = range;
  m_rate = rate_hz;
  m_isdio_out = dio_out;
}

bool DAQTaskProxy::start()
{
  return coprocess->start(handle);
}

DAQTaskProxy::~DAQTaskProxy()
{
  coprocess->stop(handle);
  handle = RTLCoprocess::Failure;
}

lsampl_t DAQTaskProxy::readSample(unsigned chan, bool *ok)
{
  lsampl_t ret = 0;
  if (coprocess->getSample(handle, chan, ret) && ok) *ok = true;
  else if (ok) *ok = false;
  return ret;
}

bool DAQTaskProxy::writeSample(unsigned chan, lsampl_t samp)
{
  return coprocess->putSample(handle, chan, samp);
}

bool DAQTaskProxy::setDataLogging(unsigned chan, bool b, unsigned id)
{
  return coprocess->setLogging(handle, b, chan, id);
}

bool DAQTaskProxy::getDataLogging(unsigned chan)
{
  return coprocess->getLogging(handle, chan);
}
