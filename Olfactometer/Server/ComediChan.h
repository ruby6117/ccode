#ifndef ComediChan_H
#define ComediChan_H

#include <comedilib.h>
#include <string>
class DAQTaskProxy;

class ComediChan
{
public:
  ComediChan(comedi_t *dev, unsigned minor, unsigned subdev, unsigned chan, unsigned range = 0, unsigned aref = 0, const double * range_override_min = 0, const double * range_override_max = 0, DAQTaskProxy * daq = 0);
  virtual ~ComediChan();

  /// if returned string is nonzero length, there is some sort of error -- this can also be set by constructor to indicate invalid params
  const std::string & error() const { return err; }

  double dataRead(bool *ok = 0) const;
  bool   dataWrite(double);
  int    dioRead(bool *ok = 0) const;
  bool   dioWrite(int bit);
  
  unsigned   minor() const { return m_minor; }
  comedi_t * dev() const { return const_cast<comedi_t *>(m_dev); }
  unsigned   subDev() const { return m_subdev; }
  unsigned   chan() const { return m_chan; }
  unsigned   range() const { return m_range; }
  unsigned   aref() const { return m_aref; }
  unsigned   maxData() const { return m_maxdata; }
  double     rangeMin() const { return m_rangeMin; }
  double     rangeMax() const { return m_rangeMax; }
  bool isAI() const { return m_isai; }
  bool isAO() const { return m_isao; }
  bool isDIO() const { return m_isdio; }
  /** set this to a read (input) device -- note that this only does something for DIO chans in non-daq-task mode.  
  */
  void setDIOInput();
  /** set this to a write (output) device -- note that this only does something for DIO chans in non-daq-task mode.  
  */
  void setDIOOutput();

  /// if this instance is using a daqtaskproxy, returns non-NULL
  DAQTaskProxy *getDAQ() const { return const_cast<DAQTaskProxy *>(daq); }
  /// to tell this instance to use a daq task instead
  void setDAQ(DAQTaskProxy *dt) { daq = dt; }

protected:
  static const double epsilon = 1e-6; ///< a double value for testing equality
  mutable std::string err;
  comedi_t *m_dev;
  unsigned m_minor, m_subdev, m_chan, m_range, m_aref;
  double m_rangeMin, m_rangeMax;
  lsampl_t m_maxdata;  
  DAQTaskProxy *daq;
  bool m_isai, m_isao, m_isdio;
};



#endif
