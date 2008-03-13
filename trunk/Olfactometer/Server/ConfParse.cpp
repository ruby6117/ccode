#include "ConfParse.h"
#include <boost/regex.hpp>
#include "Common.h"
#include "CID.h"
#include "DAQTaskProxy.h"
#include "Olfactometer.h"

#include "Log.h" 

namespace Conf 
{
  namespace Parse 
  {

      std::string boardSpec(const std::string &specstr, 
                            const ProbedComediDevices & devs,
                            comedi_t * & handle,
                            ComediSubDevice * & sdev,
                            std::pair<unsigned, unsigned> & fromToChans) 
      {
        std::map<std::string, DAQTaskProxy *> nulldaqs;
        DAQTaskProxy *daq;
        return boardSpec(specstr, devs, nulldaqs, daq, handle, sdev, fromToChans);
      }

      std::string boardSpec(const std::string &specstr, 
                            const ProbedComediDevices & devs,
                            const std::map<std::string, DAQTaskProxy *> & daqs,
                            DAQTaskProxy * &daqTask,
                            comedi_t * & handle,
                            ComediSubDevice * & sdev,
                            std::pair<unsigned, unsigned> & fromToChans) 
      {
        static const boost::regex re_daq ("^([[:alnum:]_]+):([*]|([[:digit:]]+)(-([[:digit:]]+))?)$");
        boost::cmatch caps;
        // test for daq task match
        if (boost::regex_match(specstr.c_str(), caps, re_daq)) {
          handle = 0;
          sdev = 0;
          if (caps.empty() || caps.size() < 6) return std::string("regex capture failed on '") + specstr + "'";
          
          std::map<std::string, DAQTaskProxy *>::const_iterator it = daqs.find(caps[1].str());
          if (it == daqs.end()) 
            return std::string("DAQ Task '") + caps[1].str() + "' not found.";
          daqTask = const_cast<DAQTaskProxy *>(it->second);
          // tell calling code about the handle and sdev
          CID cid;
          cid.dev = daqTask->minor();
          cid.subdev = daqTask->sdev();
          
          handle = reinterpret_cast<comedi_t *>(devs.handle(cid));
          sdev = devs.subDev(cid);
          if (!handle || !sdev) {
            return String("DAQ Task '") + caps[1].str() + "' minor " + cid.dev + " subdev " + cid.subdev + " not found\n"; 
          }
          if (String("*") == caps[2].str()) {
            fromToChans.first = 0;
            fromToChans.second = sdev->nChans-1;
          } else {
            fromToChans.first = fromToChans.second = ToUInt(caps[3].str());
            if (caps[5].matched) fromToChans.second = ToUInt(caps[5].str());
          }
          if (fromToChans.first > fromToChans.second)
            return std::string("channel range invalid: ") + Str(fromToChans.first) + "-" + Str(fromToChans.second);
          // testing
          Debug() << daqTask->name() << "," << cid.dev << "," << cid.subdev << "," << fromToChans.first << "," << fromToChans.second << "\n";
          return "";
        }

        daqTask = 0; // wasn't a daq task match
        static const boost::regex re ("^([[:alnum:]_/]+)/([[:digit:]]+):([*]|([[:digit:]]+)(-([[:digit:]]+))?)$");
        if (!boost::regex_match(specstr.c_str(), caps, re)) return std::string("regex match failed on '") + specstr + "'";
        unsigned from = 0, to = 0;
        if (caps.empty() || caps.size() < 7) return std::string("regex capture failed on '") + specstr + "'";
        //  Debug() << " Caps size: " << caps.size() << " caps1:" << caps[1].str() << " caps2:" << caps[2].str() << " caps3:" << caps[3].str() << " caps4:" << caps[4].str() << " caps5:" << caps[5].str();
        std::string boardName = caps[1].str();
        ComediDevice *d = devs.findBoard(boardName.c_str());
        if (!d) {
          // hmm, boardname failed.. try and match /dev/comediXX style board specs
          static const boost::regex re_dev_comedi ("^/dev/comedi/?([[:digit:]]+)$");
          boost::cmatch caps_dev_comedi;
          if (boost::regex_match(boardName.c_str(), caps_dev_comedi, re_dev_comedi)){
            CID c;  
            c.dev = String(caps_dev_comedi[1]).toUInt();
            d = devs.dev(c);
          }
          if (!d) return std::string("board not found or not configured: ") + boardName;
        }
        CID c;
        c.dev = d->minor;
        c.subdev = ToUInt(caps[2].str());
        c.chan = 0;  
        sdev = devs.subDev(c);
        if (!sdev) return std::string("subdevice ") + Str(c.subdev) + " on " + boardName + " not found";
        if (String(caps[3].str()) == "*") { // compute from and to to mean all chans
          from = 0;
          to = sdev->nChans-1;
        } else {
          from = ToUInt(caps[4].str());
          if (caps.size() > 6 && caps[6].matched) {
            to = ToUInt(caps[6].str());
            if (to == 0 && from != 0) return std::string("channel range invalid: ") + Str(to) + "-" + Str(from);
          } else {
            to = from;
          }
        }
        fromToChans.first = from;
        fromToChans.second = to;
        handle = (comedi_t *)devs.handle(c);
        if ( from >= sdev->nChans ) return std::string("start channel ") + Str(from) + " greater than subdevice nchans of " + Str(sdev->nChans) + "!";
        if ( to >= sdev->nChans ) return std::string("end channel ") + Str(to) + " greater than subdevice nchans of " + Str(sdev->nChans) + "!";
        //Debug() << specstr << " -> minor: " << d->minor << "  sdev: " << sdev->id << "  first: " << from << "  last: " << to;  
        return "";
      }

    std::map<double, double> calibTable(const std::string &str)
    {
      std::map<double, double> ret;
      StringList calibs = String::split(str, "\\s+");
      // build calibration table
      for (StringList::iterator it = calibs.begin(); it != calibs.end(); ++it) {
        StringList pair = String::split(*it, "->");
        if (pair.size() != 2) continue;
        bool ok1, ok2;
        double v = pair.front().toDouble(&ok1), f = pair.back().toDouble(&ok2);
        if (ok1 && ok2) ret [ v ] = f;
      }
      return ret;
    }

    std::vector<double> calibCoeffs(const std::string & confstr)
    {
      std::vector<double> coeffs;
      StringList cfl = String::split(confstr, "[[:space:],]+");
      for(StringList::iterator it = cfl.begin(); it != cfl.end(); ++it) {
        bool ok; double d;
        d = it->toDouble(&ok);
        if (!ok) break;
        coeffs.push_back(d);
      }
      return coeffs;
    }
      
    Bank::OdorTable odorTable(const std::string & str)
    {
      Bank::OdorTable ret;
      StringList odors = String::split(str, "\\|\\|");
      // build calibration table
      for (StringList::iterator it = odors.begin(); it != odors.end(); ++it) {
        StringList pair = String::split(*it, "->");
        if (pair.size() != 2) continue;
        String num = pair.front();
        String name_meta = pair.back();
        StringList name_meta_pair = String::split(name_meta, "::");
        String name = "", meta = "";
        name = name_meta_pair.front(); name_meta_pair.pop_front();
        meta = String::join(name_meta_pair, " ");
        bool ok;
        unsigned o = pair.front().toUInt(&ok);
        if (ok) ret [ o ] = Odor(name, meta);
      }
      return ret;      
    }
  }
  namespace Gen {
    /// the inverse of Conf::Parse::odorTable
    std::string odorTable(const Bank::OdorTable &ot)
    {
      String ret = "";
      for (Bank::OdorTable::const_iterator it = ot.begin(); it != ot.end(); ++it)
        ret += String::Str(it->first) + "->" + it->second.name + "::" + it->second.metadata + "||";
      return ret;
    }

    /// the inverse of Conf::Parse::odorTable
    std::string calibTable(const std::map<double, double> &ct)
    {
      String ret = "";
      for (std::map<double, double>::const_iterator it = ct.begin(); it != ct.end(); ++it)
        ret += String::Str(it->first) + "->" + String::Str(it->second) + " ";
      return ret;
    }


    std::string calibCoeffs(const std::vector<double> & v)
    {
      String str = "";
      std::vector<double>::const_iterator it;
      for(it = v.begin(); it != v.end(); ++it)
        str += String::Str(*it) + " ";
      return str;      
    }

  };


}
