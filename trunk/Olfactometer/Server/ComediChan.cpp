#include "ComediChan.h"
#include "DAQTaskProxy.h"

ComediChan::ComediChan(comedi_t *dev, unsigned minor, unsigned subdev, unsigned chan, unsigned range, unsigned aref, const double * range_override_min, const double * range_override_max, DAQTaskProxy *daq)
  : m_dev(dev), m_minor(minor), m_subdev(subdev), m_chan(chan), m_range(range), m_aref(aref), daq(daq)
{

  m_isdio = m_isai = m_isao = false;

  int t = comedi_get_subdevice_type(m_dev, m_subdev);
  if (t == COMEDI_SUBD_AO) m_isao = true;
  else if (t == COMEDI_SUBD_AI) m_isai = true;
  else if (t == COMEDI_SUBD_DIO) m_isdio = true;
  else {
    err = "Invalid parameters.";
    return;
  }
  if (range_override_min) m_rangeMin = *range_override_min;
  else {
    comedi_range *r = comedi_get_range(dev, subdev, chan, range);
    if (!r) { err = "Invalid parameters."; return; }
    m_rangeMin = r->unit == UNIT_mA ? r->min*1e3 : r->min;
  }
  if (range_override_max) m_rangeMax = *range_override_max;
  else {
    comedi_range *r = comedi_get_range(dev, subdev, chan, range);
    if (!r) { err = "Invalid parameters."; return; }
    m_rangeMax = r->unit == UNIT_mA ? r->max*1e3 : r->max;
  }
  m_maxdata = comedi_get_maxdata(dev, subdev, chan);
  if (!m_maxdata) m_maxdata = 1;
  err = "";
}

ComediChan::~ComediChan() {}

double ComediChan::dataRead(bool *ok) const
{
  lsampl_t samp;
  bool myok = true;
  double ret = 0.;
  if (daq) {
    samp = daq->readSample(m_chan, &myok);
  } else if ( comedi_data_read(dev(), m_subdev, m_chan, m_range, 0, &samp) < 1 ) {
    err = "Read error.";
    myok = false;
    return 0.;
  }
  if (!myok) err = "Read error.";
  else ret = (samp/static_cast<double>(m_maxdata) * (m_rangeMax-m_rangeMin)) + m_rangeMin;
  if (ok) *ok = myok;
  return ret;
}

bool   ComediChan::dataWrite(double d)
{
  if (d < m_rangeMin || d > m_rangeMax) {
    err = "Out of range.";
    return false;
  }
  lsampl_t samp = static_cast<lsampl_t>(((d-m_rangeMin)/(m_rangeMax-m_rangeMin)) * m_maxdata);
  
  if (daq) {
    return daq->writeSample(m_chan, samp);
  } else if ( comedi_data_write(m_dev, m_subdev, m_chan, m_range, 0, samp) < 1 ) {
    err = "Write error.";
    return false;
  }
  return true;
}

int    ComediChan::dioRead(bool *ok) const
{
  double v = dataRead(ok);
  // see if the voltage is close enough to rangemax...
  if (v >= m_rangeMax-epsilon) return 1;
  return 0;
}

bool   ComediChan::dioWrite(int bit)
{
  double v = bit ? m_rangeMax : m_rangeMin;
  return dataWrite(v);
}

  /** set this to a read (input) device -- note that this only does something for DIO chans in non-daq-task mode.  
  */
void ComediChan::setDIOInput()
{
  if (m_isdio && !daq)
    comedi_dio_config(m_dev, m_subdev, m_chan, COMEDI_INPUT);
}
/** set this to a write (output) device -- note that this only does something for DIO chans in non-daq-task mode.  
  */
void ComediChan::setDIOOutput()
{
  if (m_isdio && !daq)
    comedi_dio_config(m_dev, m_subdev, m_chan, COMEDI_OUTPUT);
}
