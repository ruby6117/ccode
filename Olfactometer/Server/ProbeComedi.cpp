#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <algorithm>

#include <comedilib.h>
#include <errno.h>
#include <string.h>

#include "ProbeComedi.h"
#include "Log.h"

void ProbedComediDevices::ProbeDevices(ComediDeviceMap & devices,
                                       ComediHandleMap & handles,
                                       unsigned start, unsigned end, bool verbose)
{
  if (verbose)
    Log() << "Probing all comedi devices from /dev/comedi" << start << " -> /dev/comedi" << end;

  devices.clear();
  CloseProbedHandles(handles);
  handles.clear();
      
  for (unsigned minor = start; minor <= end; ++minor) 
  {
    std::stringstream ss;
    ss << "/dev/comedi" << minor;
    const std::string devfile( ss.str() );
    comedi_t *it = ::comedi_open(devfile.c_str());
    if (!it) {
      if (verbose && (!devices.size() || ::comedi_errno() != ENODEV))
        // only display error message when we haven't a comedi device yet
        // or if it's something other than ENODEV
        Error() << devfile << ": " << ::comedi_strerror(::comedi_errno());
      continue;
    }

    ComediDevice & dev = devices[minor]; // put it in our map, creates a ComediDevice

    // now setup our ComediDevice
    dev.minor = minor;
    ::strncpy(dev.driverName, ::comedi_get_driver_name(it), CD_STRMAX);
    dev.driverName[CD_STRMAX] = 0;
    ::strncpy(dev.boardName, ::comedi_get_board_name(it), CD_STRMAX);
    dev.boardName[CD_STRMAX] = 0;
    dev.versionCode = ::comedi_get_version_code(it);
    dev.nSubdevs = ::comedi_get_n_subdevices(it);
    
    if (!dev.nSubdevs) {
      Warning() << "/dev/comedi" << minor << " has 0 subdevices? Ignoring..";
      devices.erase(minor);
      continue;
    }

    handles[minor] = it; // save the handle in the out param

    // now setup each subdevice
    for (unsigned sdevNo = 0; sdevNo < dev.nSubdevs && sdevNo < MAX_SUBDEVS; ++sdevNo) {
      ComediSubDevice & sdev = dev.subdev[sdevNo];
      sdev.minor = minor;
      sdev.id = sdevNo;
      sdev.type = ::comedi_get_subdevice_type(it, sdevNo);
      sdev.nChans = ::comedi_get_n_channels(it, sdevNo);
      sdev.maxdata = ::comedi_get_maxdata(it, sdevNo, 0);
      sdev.nRanges = ::comedi_get_n_ranges(it, sdevNo, 0);
      for (unsigned range = 0; range < sdev.nRanges && range < MAX_RANGES; ++range) {
        comedi_range *r = comedi_get_range(it, sdevNo, 0, range);
        if (!r) { sdev.nRanges = range; break; }
        CR_From_comedi_range(sdev.range[range], *r, range);        
      }
    }
      //comedi_close(it); don't close!! we keep the handle 
  }
}

static void CloseComediHandle(const std::pair<unsigned, void *> & p)
{ if (p.second) ::comedi_close(static_cast<comedi_t *>(p.second)); }

void ProbedComediDevices::CloseProbedHandles(const ComediHandleMap & handles)
{
  // clear any passed-in handles by closing them!
  for_each(handles.begin(), handles.end(), CloseComediHandle);
}

void ProbedComediDevices::doProbe(unsigned s, unsigned e, bool v)
{
  if (handles.size()) CloseProbedHandles(handles);
  handles.clear();
  devices.clear();
  ProbeDevices(devices, handles, s, e, v);
}

ProbedComediDevices::ProbedComediDevices() {}

ProbedComediDevices::~ProbedComediDevices()
{
  clear();
}

void
ProbedComediDevices::clear()
{
  CloseProbedHandles(handles);
  handles.clear();
  devices.clear();
}

ComediDevice * 
ProbedComediDevices::dev(CID c) const
{
  return dev(c.dev);
}

ComediDevice * 
ProbedComediDevices::dev(unsigned minor) const
{
  ComediDeviceMap::const_iterator it = devices.find(minor);
  if (it != devices.end()) return const_cast<ComediDevice *>(&it->second);
  return 0;
}

void * 
ProbedComediDevices::handle(CID c) const
{
  ComediHandleMap::const_iterator it = handles.find(c.dev);
  if (it != handles.end()) return const_cast<void *>(it->second);
  return 0;
}

ComediSubDevice * 
ProbedComediDevices::subDev(CID c) const
{
  ComediDevice *d = dev(c.dev);
  ComediSubDevice *s = 0;
  if (d && c.subdev < d->nSubdevs) s = &d->subdev[c.subdev];
  return s;
}

bool
ProbedComediDevices::devSubDev(CID c, ComediDevice * &dev_out, ComediSubDevice * & subdev_out) const
{
  ComediDevice *d = dev(c.dev);
  ComediSubDevice *s = subDev(c);
  bool ret = false;
  if (d && s) dev_out = d, subdev_out = s, ret = true;
  return ret;
}

ComediDevice * 
ProbedComediDevices::findBoard(const char *boardname) const
{
  ComediDeviceMap::const_iterator it;
  for (it = devices.begin(); it != devices.end(); ++it)
    if (!::strncmp(it->second.boardName, boardname, CD_STRMAX))
      return const_cast<ComediDevice *>(&it->second);
  return 0;
}

int
ProbedComediDevices::minor(void *handle) const
{
  ComediHandleMap::const_iterator it;
  for (it = handles.begin(); it != handles.end(); ++it)
    if (it->second == handle) return it->first;
  return -1;
}
