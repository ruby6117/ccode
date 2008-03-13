#ifndef ConfParseMisc_H
#define ConfParseMisc_H

#include <string>
#include <comedilib.h>
#include "ProbeComedi.h"
#include <algorithm>
#include <map>
#include <vector>
#include "ComediOlfactometer.h"

class DAQTaskProxy;

namespace Conf
{
  namespace Parse {
    /** return empty string on success, and sets either handle,sdev,fromtoChans *or* daqTask,fromToChans
        depending on if the spec string matched a daq task or a comedi device */
    extern  std::string 
      boardSpec(const std::string &specstr, 
                const ProbedComediDevices & devs,
                const std::map<std::string, DAQTaskProxy *> & daqs,
                DAQTaskProxy * & daqTask,
                comedi_t * & handle,
                ComediSubDevice * & sdev,
                std::pair<unsigned, unsigned> & fromToChans);
    /// compatibility function -- just like the above function but doesn't check for daq tasks and returns an error if the string was for a daq task
    extern std::string
      boardSpec(const std::string &specstr,
                const ProbedComediDevices & devs,
                comedi_t * & handle,
                ComediSubDevice * & sdev,
                std::pair<unsigned, unsigned> & fromToChans);

    extern Bank::OdorTable odorTable(const std::string & confstr);
    extern std::map<double, double> calibTable(const std::string & confstr);
    extern std::vector<double> calibCoeffs(const std::string & confstr);
    
  };

  namespace Gen {
    /// the inverse of Conf::Parse::odorTable
    extern std::string odorTable(const Bank::OdorTable &);
    /// the inverse of Conf::Parse::odorTable
    extern std::string calibTable(const std::map<double, double> &);
    extern std::string calibCoeffs(const std::vector<double> & v);
  };
};

#endif
