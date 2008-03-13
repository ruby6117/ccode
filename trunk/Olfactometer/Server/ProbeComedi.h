#ifndef ProbeComedi_H
#define ProbeComedi_H

#include "ComediDevice.h"
#include "CID.h"

#include <map>

typedef std::map<unsigned, ComediDevice> ComediDeviceMap;
typedef std::map<unsigned, void *> ComediHandleMap;

class ProbedComediDevices
{
public:
  ProbedComediDevices();
  ~ProbedComediDevices();
  void doProbe(unsigned start = 0, unsigned end = 15, bool verbose = true);
  unsigned nDevs() const { return devices.size(); }
  ComediDevice * dev(CID cid) const;
  ComediDevice * dev(unsigned minor) const;
  ComediSubDevice * subDev(CID cid) const;
  bool devSubDev(CID cid, ComediDevice * &, ComediSubDevice * &) const;
  void *handle(CID cid) const;
  ComediDevice * findBoard(const char *boardname) const;
  int minor(void *handle) const;

protected:
  void clear();
private:
  ComediDeviceMap devices;
  ComediHandleMap handles;
/** Probes all comedi devices from [start, end] and puts any found
    in the map from minor -> ComediDevice
    also puts their handles in handles_out which is 
    minor -> handle (the void * is really a comedi_t) */
  static void ProbeDevices(ComediDeviceMap & devs_out, 
                           ComediHandleMap & handles_out, 
                           unsigned start = 0, unsigned end = 15, 
                           bool verbose = true);
  static void CloseProbedHandles(const ComediHandleMap & handles);
};

#endif
