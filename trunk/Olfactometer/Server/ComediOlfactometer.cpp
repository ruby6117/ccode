#include <unistd.h>
#include <boost/regex.hpp>
#include <string>
#include <algorithm>
#include <sstream>
#include <comedilib.h>
#include "ComediOlfactometer.h"
#include "Common.h"
#include "Log.h"
#include "PIDFlowController.h"
#include "ConfParse.h"
#include "PWMValveProxy.h"
#include "DAQTaskProxy.h"

// capture 3 is the ${var} form and capture 4 is the $var form..
//static const boost::regex varRE ("\\$((\\{\\<(.*)\\>\\})|([^{0-9]+\\w*\\>))");
std::string ComediOlfactometer::replaceHostName(const std::string & str)
{
  static const boost::regex hostnameRE ("\\$((\\{\\<HOSTNAME\\>\\})|(HOSTNAME\\>))");
  static char hostname[HOST_NAME_MAX+1] = { 0 };
  if (!hostname[0]) {
    ::gethostname(hostname, HOST_NAME_MAX);
    hostname[HOST_NAME_MAX] = 0;
  }
  return boost::regex_replace(str, hostnameRE, hostname, boost::match_default | boost::regex_constants::format_literal);
}

StringList  ComediOlfactometer::split(const std::string & str)
{
  return String::split(str, "[[:space:],]+");
}

ComediOlfactometer::ComediOlfactometer() { clearSetup(); }
ComediOlfactometer::~ComediOlfactometer() { } 

void ComediOlfactometer::clearSetup()
{
  deleteAllChildren();  
  for (std::map<std::string, DAQTaskProxy *>::iterator it = daqTasks.begin();
       it != daqTasks.end(); ++it)
    delete it->second;
  daqTasks.clear();
  useBoards.clear();
  coprocess = 0;
}

bool
ComediOlfactometer::doBoardSetup(const Settings & ini)
{
    // pick up board names, see that they exist..
    std::string str = ini.get(Conf::Sections::Devices, Conf::Keys::comediboards);    
    StringList boards = split(str);
    if (!str.length()) {
      Error() << "Configuration file error: missing comedi boards specification!\n";
      return false;
    }
    for(StringList::iterator it = boards.begin(); it != boards.end(); ++it) {
      ComediDevice *dev = findBoard(it->c_str());
      if (!dev) {
        Error() << "Configuration file error: board '" << *it << "' specified in configuration file is not found or configured in comedi\n";
        return false;
      }
      useBoards[*it] = dev;  
      Log() << "Using device " << *it << "\n";
    }
    if (useBoards.size() == 0) {
      Error() << "Configuration file error: comedi boards specification invalid or empty!\n";
      return false;      
    }
    return true;
}

bool
ComediOlfactometer::doDaqTaskSetup(const Settings & ini)
{
    // pick up board names, see that they exist..
    std::string str = ini.get(Conf::Sections::Devices, Conf::Keys::rtdaq_tasks);    
    StringList tasks = split(str);
    if (!str.length())  return true; // silently ignore missing rtdaq_tasks..
    Settings::SectionExists sectionExists(ini);
    for (StringList::iterator it = tasks.begin(); it != tasks.end(); ++it) {
      String taskname = *it,
             spec = ini.get(taskname, Conf::Keys::boardspec),
             rateHz = ini.get(taskname, Conf::Keys::rate_hz);
      if (!sectionExists(taskname) || !spec.length()) {
        Error() << "Configuration file error: rtdaq_task '" << taskname << "' specified in configuration file is missing its specification section\n";
        return false;
      }
      bool ok;
      unsigned rate = rateHz.toUInt(&ok);
      if (!ok) rate = 0; /* a passive daq task has rate 0 */ 
      if (daqTasks.find(taskname) != daqTasks.end()) {
        Error() << "Configuraton file error: rtdaq_task " << taskname << " appears multiple definitions in the Devices secton!\n";
        return false;
      }
      comedi_t *handle;  ComediSubDevice *sdev; std::pair<unsigned, unsigned> fromTo;
      std::string ret = Conf::Parse::boardSpec(spec, *this, handle, sdev, fromTo);
      if (ret.length()) {
        Error() << "Configuration file error: rtdaq_task '" << taskname << "' specified in configuration file has error: '" << ret << "'\n";
        return false;
      }
      if (!coprocess) {
        Error() << "Configuration file error: rtdaq_task '" << taskname << "' specified in configuration file is valid but there is no RTLCoprocess running!\n";
        return false;        
      }
      bool dioOutput = ini.get(taskname, Conf::Keys::dio) == "output";
      unsigned range = String(ini.get(taskname, Conf::Keys::range)).toUInt();
      unsigned aref = String(ini.get(taskname, Conf::Keys::aref)).toUInt();
      String rng = ini.get(taskname, Conf::Keys::range_override);
      boost::cmatch caps;
      double rngmin, rngmax;
      double *p_rngmin = 0, *p_rngmax = 0;
      static const boost::regex numRangeRE("^([[:digit:].]+)-([[:digit:].]+)$");
      if (rng.length() && boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
        p_rngmin = &rngmin;
        p_rngmax = &rngmax;
        *p_rngmin = ToDouble(caps[1].str());
        *p_rngmax = ToDouble(caps[2].str());
      }

      daqTasks[taskname] = new DAQTaskProxy(taskname, coprocess, rate, sdev, fromTo.first, fromTo.second, range, aref, dioOutput, p_rngmin, p_rngmax);
      if (!daqTasks[taskname]->start()) {
        Error() << "Internal error: rtdaq_task '" << taskname << "' could not be started!\n";
        return false;        
      }
      if (rate) 
        Log() << "RT-DAQ active periodic task " << taskname << " started at " << rate << "Hz\n";
      else
        Log() << "RT-DAQ passive multiplexing task " << taskname << " created.";
    }
    return true;
}

bool 
ComediOlfactometer::doGeneralSetup(const Settings & ini)
{
    // get General name
    std::string str = ini.get(Conf::Sections::General, Conf::Keys::name);
    str = replaceHostName(str);
    if (!str.length()) {
      Error() << "Configuration file error: missing an olfactometer name in the [ " << Conf::Sections::General << " ] section!\n";
      return false;
    }
    setName(str);
    
    // get General description
    str = ini.get(Conf::Sections::General, Conf::Keys::description);  
    m_description = replaceHostName(str);
    return true;
}

bool  ComediOlfactometer::validateLayoutSection(const Settings & ini, StringList & mixes, StringList & flow_controllers, StringList & flow_meters, StringList & pwm_valves, StringList &sensors)
{
  mixes = split(ini.get(Conf::Sections::Layout, Conf::Keys::mixes));
  if ( !mixes.size() ) {
    Error() << "Configuration file error: mixes specification in Layout section is invalid, missing or empty!\n";
    return false;
  }
  flow_controllers = split(ini.get(Conf::Sections::Layout, Conf::Keys::standalone_flows));
  flow_meters = split(ini.get(Conf::Sections::Layout, Conf::Keys::standalone_meters));
  pwm_valves = split(ini.get(Conf::Sections::Layout, Conf::Keys::pwm_valves));
  sensors = split(ini.get(Conf::Sections::Layout, Conf::Keys::standalone_sensors));
  // make sure they all exist
  Settings::SectionExists vse(ini);
  // stupid stl doesn't pass by reference, so we must assign vse to get the results
  vse = std::for_each(flow_controllers.begin(), flow_controllers.end(), vse);
  vse = std::for_each(flow_meters.begin(), flow_meters.end(), vse);
  vse = std::for_each(mixes.begin(), mixes.end(), vse);
  vse = std::for_each(pwm_valves.begin(), pwm_valves.end(), vse);
  vse = std::for_each(sensors.begin(), sensors.end(), vse);
  
  if (vse.missing_ct) {
    Error() << "Configuration file error: " << vse.missing.front() << " specified in Layout section, but it is missing or empty\n";
    return false;
  }

  return true;
}

/// functor passed to std::for_each algorithm which, for each string it is given attempts to locate and construct (using operator new) a Bank with miker 'parent' as parent
struct BuildBanks
{
  BuildBanks(const ComediOlfactometer &olf, Settings & ini, 
             const std::map<std::string, DAQTaskProxy *> & daqTasks,
             Mix *parent, std::string & err) : olf(olf), ini(ini), daqTasks(daqTasks), parent(parent), err(err) {}
  const ComediOlfactometer &olf;
  Settings &ini;
  const std::map<std::string, DAQTaskProxy *> & daqTasks;
  Mix *parent;
  std::string &err;
  void operator()(const std::string & bankstr);
};

void BuildBanks::operator()(const std::string & bankstr)
{
  if (err.length()) return;
  std::string v, he, se, fc;
  v = ini.get(bankstr, Conf::Keys::valves);
  he = ini.get(bankstr, Conf::Keys::hard_enable);
  se = ini.get(bankstr, Conf::Keys::soft_enable);
  fc = ini.get(bankstr, Conf::Keys::flow_controller);
  if (!v.length()) err = Conf::Keys::valves;
  else if (!he.length()) err = Conf::Keys::hard_enable;
  else if (!se.length()) err = Conf::Keys::soft_enable;
  else if (!fc.length()) err = Conf::Keys::flow_controller;
  if (err.length()) {
    err = std::string("Configuration error: bank ") + bankstr + " is missing section " + err + "\n";
    return;
  }
  ComediSubDevice *sdev;
  std::pair<unsigned, unsigned> fromTo;
  std::string ret;
  comedi_t *handle = 0;
  DAQTaskProxy *daq_valves = 0;
  if ( (ret = Conf::Parse::boardSpec(v, olf, daqTasks, daq_valves, handle, sdev, fromTo)).length() ) {
    err = std::string("Configuration error: bank ") + bankstr + " parse/setup failed on valves= spec! (" + ret + ")";
    return;
  }
  unsigned nValves = (fromTo.second - fromTo.first) + 1;
  if ( nValves != ComediBank::RequiredValves) {
    err = std::string("Configuration error: bank ") + bankstr + " appears to have " + Str(nValves) + " valves when it requires " + Str(ComediBank::RequiredValves) + " valves!\n";
    return;
  }
  if (fromTo.first + nValves >= sdev->nChans) {
    err = std::string("Configuration error: bank ") + bankstr + " appears to have a valve specification that specifies non-existant channels: " + v +"\n";
    return;
  }
  ComediSubDevice *sdev_he, *sdev_se;
  std::pair<unsigned, unsigned> ch_se, ch_he;
  comedi_t *handle_he, *handle_se;
  DAQTaskProxy *daq_he = 0, *daq_se = 0;
  
  if ( (ret = Conf::Parse::boardSpec(he, olf, daqTasks, daq_he, handle_he, sdev_he, ch_he)).length() ) {
    err = std::string("Configuration error: bank ") + bankstr + " parse/setup failed on hard_enable= spec! (" + ret + ")";
    return;
  }
  if ( (ret = Conf::Parse::boardSpec(se, olf, daqTasks, daq_se, handle_se, sdev_se, ch_se)).length() ) {
    err = std::string("Configuration error: bank ") + bankstr + " parse/setup failed on soft_enable= spec! (" + ret + ")";
    return;
  }
//   if (handle_he != handle_se || handle != handle_he || sdev_he != sdev_se 
//       || sdev != sdev_he) {
//     err = std::string("Configuration error: bank ") + bankstr + " parse/setup failed -- all lines for a bank must be on the same comedi subdevice!";
//     return;
//   }
  if (ch_he.first >= sdev_he->nChans || ch_se.first >= sdev_se->nChans) {
    err = std::string("Configuration error: bank ") + bankstr + " parse/setup failed -- hard or soft enable line out of subdevice channel range!";
    return;
  }
  ComediChan hard_en(handle_he, sdev_he->minor, sdev_he->id, ch_he.first, 0, 0, 0, 0, daq_he);
  ComediChan soft_en(handle_se, sdev_se->minor, sdev_se->id, ch_se.first, 0, 0, 0, 0, daq_se);
  std::vector<ComediChan> valves;
  valves.reserve(nValves);
  for (unsigned i = fromTo.first; i <= fromTo.second; ++i) {
    valves.push_back(ComediChan(handle, sdev->minor, sdev->id, i, 0, 0, 0, 0, daq_valves));
  }
  ComediBank * bank = new ComediBank(parent, bankstr, valves, hard_en, soft_en, ini); // pointer gets auto added to childlist of Mix *parent
  if ( !olf.buildFlowController(ini, bank, fc, err) ) {
    // err gets set by above.. so just undo what we did and return
    delete bank;
    return;
  }
  // parse odor table
  Bank::OdorTable table = Conf::Parse::odorTable(ini.get(bankstr, Conf::Keys::odor_table));
  if (table.size() == ComediBank::RequiredValves 
      && table.begin()->first == 0
      && table.rbegin()->first == ComediBank::RequiredValves-1) 
    bank->setOdorTable(table);
}

bool ComediOlfactometer::buildMixSection(Settings & ini, const std::string & mix) 
{
  String c, b, dft, dof;
  if (!(c = ini.get(mix, Conf::Keys::carrier)).length()) {
    Error() << "Configuration error: mix " << mix << " is missing a carrier definition\n";
    return false;
  }
  if (!(b = ini.get(mix, Conf::Keys::banks)).length()) {
    Error() << "Configuration error: mix " << mix << " is missing a banks definition\n";
    return false;
  }
  bool ok;
  dft = ini.get(mix, Conf::Keys::default_flow_total);
  dft.toDouble(&ok);
  if (!ok) {
    Warning() << "Configuration error: mix " << mix << " missing/invalid default flow total definition, defaulting to 1000\n";
    dft = "1000";
  }
  dof = ini.get(mix, Conf::Keys::default_odor_flow);
  dof.toDouble(&ok);
  if (!ok) {
    Warning() << "Configuration error: mix " << mix << " missing/invalid default odor flow definition, defaulting to 100\n";
    dof = "100";
  }

  Settings::SectionExists vse(ini);

  if (!vse(c)) {
    Error() << "Configuration error: mix " << mix << " cannot find flow controller " << c << "!\n";
    return false;
  }

  StringList banks = split(b);
  vse = std::for_each(banks.begin(), banks.end(), vse);
  if (vse.missing_ct) {
    Error() << "Configuration error: mix " << mix << " specified banks " << vse.missing.front() << " which lack definitions\n";
    return false;
  }
  Mix *m = new Mix(this, mix);
  std::string err;
  BuildBanks bb(*this, ini,  daqTasks, m, err);
  std::for_each(banks.begin(), banks.end(), bb);
  if (err.length()) {
    Error() << err;
    deleteAllChildren(); // deletes m too
    return false;
  }
  
  // setup the carrier
  if ( !buildFlowController(ini, m, c, err) ) {
    Error() << err;
    deleteAllChildren();
    return false;
  }
  // tell mix that the flow total should be dft from config file..
  m->setOdorFlow(dof.toDouble(), true); // default flow for odor channels
  m->setDesiredTotalFlow(dft.toDouble(), true);

  return true;
}

bool
ComediOlfactometer::doLayoutSetup(Settings & ini)
{
  StringList extra_flow_conts, extra_meters, mixes, pwms, sensors;
  StringList::iterator it;
  if (!validateLayoutSection(ini, mixes, extra_flow_conts, extra_meters, pwms, sensors)) return false;
  for (it = mixes.begin(); it != mixes.end(); ++it) {
    if ( !buildMixSection(ini, *it) ) return false;
  }
  for (it = extra_flow_conts.begin(); it != extra_flow_conts.end(); ++it) {
    std::string errstr;
    if ( !buildFlowController(ini, this, *it, errstr) ) {
      Error() << errstr;
      return false;
    }
  }
  for (it = extra_meters.begin(); it != extra_meters.end(); ++it) {
    std::string errstr;
    if ( !buildFlowMeter(ini, this, *it, errstr) ) {
      Error() << errstr;
      return false;
    }
  }
  for (it = pwms.begin(); it != pwms.end(); ++it) {
    std::string errstr;
    if ( !buildPWM(ini, this, *it, errstr) ) {
      Error() << errstr;
      return false;
    }
  }
  for (it = sensors.begin(); it != sensors.end(); ++it) {
    std::string errstr;
    if ( !buildSensor(ini, this, *it, errstr) ) {
      Error() << errstr;
      return false;
    }
  }
  return true;
}

bool ComediOlfactometer::buildPWM(const Settings &ini, Component *parent, const std::string &name, std::string & error_out) const
{
  if (!coprocess) {
    error_out = std::string("Error: Cannot built PWM valve " + name + ".  PWM valves require that the RT Coprocess (kernel module) be loaded and running!\n");
    return false;
  }

  DAQTaskProxy *daq;
  comedi_t *handle;
  ComediSubDevice *sdev;
  std::pair<unsigned, unsigned> fromTo;
  
  String writechan = ini.get(name, Conf::Keys::writechan), err;
  err = Conf::Parse::boardSpec(writechan, *this, daqTasks, daq, handle, sdev, fromTo);
  if (err.length()) {
    std::ostringstream os;
    os << "Configuration file error: " << name << " failed on writechan= spec.: " << err << "\n";
    error_out = os.str();
    return false;
  }
  PWMVParams params;
  params.windowSizeMicros = 1000000;
  params.dutyCycle = 50;
  params.dev = sdev->minor;
  params.subdev = sdev->id;
  params.chan = fromTo.first;
  ComediChan ch(handle, sdev->minor, sdev->id, fromTo.first, 0, 0, 0, 0, daq);
  PWMValveProxy *pwm = new PWMValveProxy(parent, name, const_cast<RTLCoprocess *>(coprocess), daq->name(), params, ch);
  if (!pwm->isOk()) {
    std::ostringstream os;
    os << "Internal error: Failed to start PWM Valve " << name << " for an unspecified reason (DEBUG ME).\n";
    error_out = os.str();
    delete pwm;
    return false;    
  }
  return true;
}


bool
ComediOlfactometer::doSetup(Settings & ini, RTLCoprocess *coprocess) 
{
  if (!nDevs()) doProbe();
  if (!nDevs()) return false;
  clearSetup();
  this->coprocess = coprocess;
  try {

    if ( !doGeneralSetup(ini) // name, description
         || !doBoardSetup(ini) // Devices comediboards= spec
         || !doDaqTaskSetup(ini) // Devices rtdaq_tasks= spec
         || !doLayoutSetup(ini) )// the rest 
      { clearSetup(); return false; }

    // verify that all children do not contain spaces
    std::list<Component *> childList = children(true);
    for (std::list<Component *>::const_iterator it = childList.begin();
         it != childList.end();
         ++it)
    {
      const String componentName = (*it)->name();
      if (componentName.search("\\s") > -1) {
        Error() << "Configuration file error: Component name \"" << componentName << "\" is invalid because it contains spaces.  Please fix the configuration file!\n";
        clearSetup(); 
        return false; 
      }
    }

    //Debug() << "Olfactometer name: " << name() << "\n";
  } catch (const boost::bad_expression & e) {

    Error() << "Configuration file error: " << e.what() << "\n";    
    return false;

  } catch (const std::runtime_error & e) {

    Error() << "Internal Runtime error during config setup: " 
            << e.what() << " .. bad config file?\n";
    return false;

  }

  return true;
}

ComediBank::ComediBank(Component *parent, const std::string &name,
                       const std::vector<ComediChan> & v,
                       const ComediChan & he,
                       const ComediChan & se, Settings & ini)
  : Bank(parent, name, RequiredValves),
    DOEnableable(this, &he, &se),
    valves(this, name+"_Valves", v), 
    ini(ini)
{
  resetOdor();
}

ComediBank::~ComediBank()
{
  resetOdor();
  //Debug() << "ComediBank::~ComediBank()\n";
}

bool
ComediBank::setOdor(unsigned o)
{
  unsigned oldOdor = currentOdor();
  if (Bank::setOdor(o) && valves.write(o)) 
    return true;
  // else...
  valves.write(oldOdor);
  return false;
}

bool ComediBank::save()
{
  ini.put(name(), Conf::Keys::odor_table, Conf::Gen::odorTable(odorTable()));
  return ini.save();
}

ComediSensor::ComediSensor(Component *parent, const std::string & name, const ComediChan & aio_template)
  : Sensor(parent, name), ComediDataLogable(aio_template, id())
{}

CalibComediSensor::CalibComediSensor(Component *parent, const std::string & name, const ComediChan & aio_template)
  : ComediSensor(parent, name, aio_template) {}

ComediSensor::~ComediSensor() {}

double ComediSensor::read() 
{
  double v = readRaw();
  return (v-rangeMin())/(rangeMax()-rangeMin()) * (max()-min()) + min();
}

double CalibComediSensor::read() 
{
  double v = readRaw();
  Calib *c = !dynamic_cast<PIDFlowController *>(parent()) ? dynamic_cast<Calib *>(parent()) : 0;
  if (c)  v = c->inverse(v);  // apply possible calibration settings, if any

  return (v-rangeMin())/(rangeMax()-rangeMin()) * (max()-min()) + min();
}

double ComediSensor::readRaw() 
{
  bool ok;
  double v = dataRead(&ok);
  if (!ok) return min();
  return v;
}

ComediActuator::ComediActuator(Component *parent, const std::string & name, const ComediChan &aio_template)
  : Actuator(parent, name), ComediDataLogable(aio_template, id())
{}

CalibComediActuator::CalibComediActuator(Component *parent, const std::string & name, const ComediChan &aio_template)
  : ComediActuator(parent, name, aio_template)
{}

ComediActuator::~ComediActuator() {}

bool ComediActuator::write(double d)
{
  if (d < min() || d > max()) return false;
  double v = (d-min())/(max()-min()) * (rangeMax()-rangeMin()) + rangeMin();

  bool ret = writeRaw(v);
  if (ret) m_lastWritten = d;
  return ret;
}

bool ComediActuator::writeRaw(double d)
{ 
  bool ret = dataWrite(d); 
  if (ret) m_lastWrittenRaw = d; 
  return ret; 
}

double ComediActuator::read() 
{ 

  if (getDAQ()) {
    double v = ((dataRead()+rangeMin())/(rangeMax()-rangeMin())) * (max()-min()) + min();
    return v;
  }
  else return lastWritten();
}

bool CalibComediActuator::write(double d)
{
  if (d < min() || d > max()) return false;
  double v = (d-min())/(max()-min()) * (rangeMax()-rangeMin()) + rangeMin();

  Calib *c = dynamic_cast<Calib *>(parent());
  if (c)  v = c->xform(v);  // apply possible calibration settings, if any

  bool ret = writeRaw(v);
  if (ret) m_lastWritten = d;
  return ret;
}

double CalibComediActuator::read()
{
  if (getDAQ()) {
    // apply possible calibration settings, if any -- UGLY HACK don't apply calib here for pidflow since it actually uses calibration in the kernel thread?
    Calib *c = dynamic_cast<Calib *>(parent());
    double v = ((dataRead()+rangeMin())/(rangeMax()-rangeMin())) * (max()-min()) + min();
    if (c) v = c->inverse(v); // assumption is that calibration is just 1 term, the 4th term of a 3rd degree equation
    return v;
  }
  else return lastWritten();
}

double ComediActuator::readRaw() 
{
  if (getDAQ()) return dataRead();
  else return lastWrittenRaw();
}

bool ComediOlfactometer::buildFlowController(Settings & ini, Component *parent, const std::string &fcname, std::string & error_out) const
{
  std::string errstr, parentname = "<No parent>";
  if (parent) parentname = parent->name();
  DAQTaskProxy *daqin = 0, *daqout = 0;
  std::string fcdescname = std::string("Flow controller ") + parentname + "->" + fcname;
  if (  !Settings::SectionExists(ini)(fcname) ) { // make sure the flow-controller spec exists
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " not found!\n";
    error_out = os.str();
    return false;
  }
  std::string type = ini.get(fcname, Conf::Keys::type);
  if (type.length() == 0) type = "aalborg";
  if (type != "pid" && type != "aalborg" && type != "automatic" && type != "null" && type != "manual" && type != "loopback") {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " type = " << type << " is an unknown flow controller type!\n";
    error_out = os.str();
    return false;
  }

  if ( !Settings::SectionExists(ini)(fcname) ) { // make sure the flow-controller spec exists
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " not found!\n";
    error_out = os.str();
    return false;
  }
  // generic flowcontroller setup
  std::string rng = ini.get(fcname, Conf::Keys::mass_range_ml);
  boost::cmatch caps;
  static const boost::regex numRangeRE("^([[:digit:].]+)-([[:digit:].]+)$");
  if (!rng.length() || !boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed to read/parse mass_range_ml!\n";    
    error_out = os.str();
    return false;
  }
  double flow_min = ToDouble(caps[1].str()), flow_max = ToDouble(caps[2].str());
  if (flow_min < 0 || flow_max < 0 || flow_max < flow_min || flow_max - flow_min < 0.1 || flow_max - 0 < 0.1) {
    std::ostringstream os;        
    os << "Configuration file error: " << fcdescname << " mass range ml seems too tiny or invalid.\n";
    error_out = os.str();
    return false;
  }

  FlowController *fc = 0;

  if (type == "manual" || type == "null" || type == "loopback") {
    // 'null' flow controllers have a simple setup..

    Actuator *a = new NullActuator(0, fcname+"_Actuator");
    Sensor *s = new LoopbackSensor(0, fcname+"_Sensor", a);    
    fc = new FlowController(s, a, parent, fcname);

  } else {
    // specific to comedi-device-based flow controllers -- parse boardspec, etc

    comedi_t *handle_rd = 0,  *handle_wr = 0;
    ComediSubDevice * sdev_rd = 0, *sdev_wr = 0;
    unsigned minor_rd = 0, minor_wr = 0;
    std::pair<unsigned, unsigned> chans_rd, chans_wr; 
    std::string readchan = ini.get(fcname, Conf::Keys::readchan);
    if (!readchan.length()) {
      std::ostringstream os;
      os << "Configuration file error: " << fcdescname << " lacks a readchan specification!\n";
      error_out = os.str();
      return false;
    }
    errstr = Conf::Parse::boardSpec(readchan, *this, daqTasks, daqin, handle_rd, sdev_rd, chans_rd);
    if (errstr.length()) {
      std::ostringstream os;
      os << "Configuration file error: " << fcdescname << " failed on readchan= spec.: " << errstr << "\n";
      error_out = os.str();
      return false;
    } 
    minor_rd = minor(handle_rd);
    std::string writechan = ini.get(fcname, Conf::Keys::writechan);
    errstr = Conf::Parse::boardSpec(writechan, *this,  daqTasks, daqout, handle_wr, sdev_wr, chans_wr);
    if (errstr.length()) {
      std::ostringstream os;
      os << "Configuration file error: " << fcdescname << " failed on writechan= spec.: " << errstr << "\n";
      error_out = os.str();
      return false;
    }
    minor_wr = minor(handle_wr);
    unsigned chan_rd = chans_rd.first;
    unsigned chan_wr = chans_wr.first;
    double rdrngmin = 0., rdrngmax = 5., wrrngmin = 0., wrrngmax = 5.;
    unsigned rng_rd = 0, rng_wr = 0;
    if (daqin) rng_rd = daqin->range();
    if (daqout) rng_wr = daqout->range();
    comedi_range *r = comedi_get_range(handle_rd, sdev_rd->id, chan_rd, rng_rd);
    if (r) rdrngmin = r->min, rdrngmax = r->max;
    r = comedi_get_range(handle_wr, sdev_wr->id, chan_wr, rng_wr);
    if (r) wrrngmin = r->min, wrrngmax = r->max;

    rng = ini.get(fcname, Conf::Keys::read_range_override);
    if (rng.length()) {
      if (!boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
        std::ostringstream os;
        os << "Configuration file error: " << fcdescname << " failed to read/parse read_range_override!\n";    
        error_out = os.str();
        return false;
      }
      rdrngmin = ToDouble(caps[1].str()), rdrngmax = ToDouble(caps[2].str());
    }
    rng = ini.get(fcname, Conf::Keys::write_range_override);
    if (rng.length()) {
      if (!boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
        std::ostringstream os;
        os << "Configuration file error: " << fcdescname << " failed to read/parse write_range_override!\n";    
        error_out = os.str();
        return false;
      }
      wrrngmin = ToDouble(caps[1].str()), wrrngmax = ToDouble(caps[2].str());
    }

    ComediChan comedi_in(handle_rd, minor_rd, sdev_rd->id, chan_rd, 0,  0, &rdrngmin, &rdrngmax, daqin);
    ComediChan comedi_out(handle_wr, minor_wr, sdev_wr->id, chan_wr, 0, 0, &wrrngmin, &wrrngmax, daqout);

    if (type == "automatic" || type == "aalborg") {

      Sensor *s = new CalibComediSensor(0, fcname + "_Sensor", comedi_in);
      Actuator *a = new CalibComediActuator(0, fcname + "_Actuator", comedi_out);
      CalibFlowController * cfc;
      fc = cfc = new CalibFlowController(ini, s, a, parent, fcname);
      if (!cfc->load(ini, fcname)) {
        // apply default calibration settings, which is just the identity transformation
        Calib::Coeffs coeffs;
        coeffs.push_back(0.0);
        coeffs.push_back(0.0);
        coeffs.push_back(1.0);
        coeffs.push_back(0.0);
        cfc->setCoeffs(coeffs);
      }

    } else if (type == "pid" ) {

        ComediChan *soft_en = 0, *hard_en = 0;
        
        { // scope this code that tries to detect hard_enable and soft_enable spec in ini file
            String se = ini.get(fcname, Conf::Keys::soft_enable);
            String he = ini.get(fcname, Conf::Keys::hard_enable);
            ComediSubDevice *sdev_se = 0, *sdev_he;
            std::pair<unsigned, unsigned> ch_se, ch_he;
            comedi_t *handle_se, *handle_he;
            DAQTaskProxy *daq_se = 0, *daq_he = 0;
            
            if (se.length()) {  // support optional soft_enable line
              
              errstr = Conf::Parse::boardSpec(se, *this, daqTasks, daqin, handle_se, sdev_se, ch_se);
              
              if (errstr.length()) {
                std::ostringstream os;
                os << "Configuration file error: " << fcdescname << " failed on soft_enable= spec.: " << errstr << "\n";
                error_out = os.str();
                return false;
              } 
              
              soft_en = new ComediChan(handle_se, sdev_se->minor, sdev_se->id, ch_se.first, 0, 0, 0, 0, daq_se);
              
            }
            
            if (he.length()) { // support optional hard_enable line
              
              errstr = Conf::Parse::boardSpec(he, *this, daqTasks, daqin, handle_he, sdev_he, ch_he);
              
              if (errstr.length()) {
                std::ostringstream os;
                os << "Configuration file error: " << fcdescname << " failed on hard_enable= spec.: " << errstr << "\n";
                error_out = os.str();
                return false;
              }
              
              hard_en = new ComediChan(handle_he, sdev_he->minor, sdev_he->id, ch_he.first, 0, 0, 0, 0, daq_he);
              
            }
        }

        if (!hard_en && !soft_en) {
          error_out = "Configuration file error: " + fcdescname + " requires at least a hard_enable or soft_enable spec!\n";
          return false;
        }


        PIDFlowController *pfc;
        fc = pfc = new PIDFlowController(ini, coprocess, comedi_in,  comedi_out, hard_en, soft_en, parent, fcname);

        delete hard_en; hard_en = 0;
        delete soft_en; soft_en = 0;

        if (!pfc->isValid()) {
           error_out = pfc->errorString();
           delete pfc;
           return false;
        }
    }

  } 
  fc->setMin(flow_min);
  fc->setMax(flow_max);
  
  return true;
}

bool ComediOlfactometer::buildFlowMeter(const Settings & ini, Component *parent, const std::string &fcname, std::string & error_out) const
{
  std::string errstr, parentname = "<No parent>";
  if (parent) parentname = parent->name();
  DAQTaskProxy *daqin = 0;
  std::string fcdescname = std::string("Flow meter ") + parentname + "->" + fcname;
  if (  !Settings::SectionExists(ini)(fcname) ) { // make sure the flow-meter spec exists
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " not found!\n";
    error_out = os.str();
    return false;
  }
  std::string type = ini.get(fcname, Conf::Keys::type);
  if (type.length() == 0) type = "aalborg";
  if (type != "aalborg" && type != "automatic") {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " type = " << type << " is an unsupported flow meter type!\n";
    error_out = os.str();
    return false;
  }

  // generic flowmeter setup
  comedi_t *handle_rd = 0;
  ComediSubDevice * sdev_rd = 0;
  unsigned minor_rd = 0;
  std::pair<unsigned, unsigned> chans_rd; 
  if ( !Settings::SectionExists(ini)(fcname) ) { // make sure the flow-controller spec exists
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " not found!\n";
    error_out = os.str();
    return false;
  }
  std::string rng = ini.get(fcname, Conf::Keys::mass_range_ml);
  boost::cmatch caps;
  static const boost::regex numRangeRE("^([[:digit:].]+)-([[:digit:].]+)$");
  if (!rng.length() || !boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed to read/parse mass_range_ml!\n";    
    error_out = os.str();
    return false;
  }
  double flow_min = ToDouble(caps[1].str()), flow_max = ToDouble(caps[2].str());
  if (flow_min < 0 || flow_max < 0 || flow_max < flow_min || flow_max - flow_min < 0.1 || flow_max - 0 < 0.1) {
    std::ostringstream os;        
    os << "Configuration file error: " << fcdescname << " mass range ml seems too tiny or invalid.\n";
    error_out = os.str();
    return false;
  }
  std::string readchan = ini.get(fcname, Conf::Keys::readchan);
  if (!readchan.length()) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " lacks a readchan specification!\n";
    error_out = os.str();
    return false;
  }
  errstr = Conf::Parse::boardSpec(readchan, *this, daqTasks, daqin, handle_rd, sdev_rd, chans_rd);
  if (errstr.length()) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed on readchan= spec.: " << errstr << "\n";
    error_out = os.str();
    return false;
  } 
  minor_rd = minor(handle_rd);
  unsigned chan_rd = chans_rd.first;  
  unsigned rng_rd = 0;
  double rdrngmin = 0., rdrngmax = 5.;
  if (daqin) rng_rd = daqin->range();
  comedi_range *r = comedi_get_range(handle_rd, sdev_rd->id, chan_rd, rng_rd);
  if (r) rdrngmin = r->min, rdrngmax = r->max;

  rng = ini.get(fcname, Conf::Keys::read_range_override);
  if (rng.length()) {
    if (!boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
      std::ostringstream os;
      os << "Configuration file error: " << fcdescname << " failed to read/parse read_range_override!\n";    
      error_out = os.str();
      return false;
    }
    rdrngmin = ToDouble(caps[1].str()), rdrngmax = ToDouble(caps[2].str());
  }
  FlowMeter *fm = 0;
  ComediChan comedi_in(handle_rd, minor_rd, sdev_rd->id, chan_rd, 0,  0, &rdrngmin, &rdrngmax, daqin);

  Sensor *s  = new ComediSensor(0, fcname + "_Sensor", comedi_in);

  fm = new FlowMeter(s, parent, fcname);

  fm->setMin(flow_min);
  fm->setMax(flow_max);
  
  return true;
}


bool ComediOlfactometer::buildSensor(const Settings &ini, Component *parent, const std::string &name, std::string & error_out) const
{
  std::string errstr, parentname = "<No parent>";
  if (parent) parentname = parent->name();
  DAQTaskProxy *daqin = 0;
  std::string fcdescname = std::string("Sensor ") + parentname + "->" + name;
  if (  !Settings::SectionExists(ini)(name) ) { // make sure the sensor spec exists
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " not found!\n";
    error_out = os.str();
    return false;
  }

  // generic setup
  comedi_t *handle_rd = 0;
  ComediSubDevice * sdev_rd = 0;
  unsigned minor_rd = 0;
  std::pair<unsigned, unsigned> chans_rd; 
  std::string rng = ini.get(name, Conf::Keys::unit_range);
  boost::cmatch caps;
  static const boost::regex numRangeRE("^([[:digit:].]+)-([[:digit:].]+)$");
  if (!rng.length() || !boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed to read/parse unit_range!\n";    
    error_out = os.str();
    return false;
  }
  String units = ini.get(name, Conf::Keys::units);
  if (!units.length()) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed to read/parse units!\n";    
    error_out = os.str();
    return false;
  }
  double unit_min = ToDouble(caps[1].str()), unit_max = ToDouble(caps[2].str());
  std::string readchan = ini.get(name, Conf::Keys::readchan);
  if (!readchan.length()) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " lacks a readchan specification!\n";
    error_out = os.str();
    return false;
  }
  errstr = Conf::Parse::boardSpec(readchan, *this, daqTasks, daqin, handle_rd, sdev_rd, chans_rd);
  if (errstr.length()) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed on readchan= spec.: " << errstr << "\n";
    error_out = os.str();
    return false;
  } 
  minor_rd = minor(handle_rd);
  unsigned chan_rd = chans_rd.first;

  rng = ini.get(name, Conf::Keys::read_range_override);
  if (!rng.length() || !boost::regex_match(rng.c_str(), caps, numRangeRE) ) {
    std::ostringstream os;
    os << "Configuration file error: " << fcdescname << " failed to read/parse read_range_override!\n";    
    error_out = os.str();
    return false;
  }
  double rdrngmin = ToDouble(caps[1].str()), rdrngmax = ToDouble(caps[2].str());
  ComediChan comedi_in(handle_rd, minor_rd, sdev_rd->id, chan_rd, 0,  0, &rdrngmin, &rdrngmax, daqin);
  ComediSensor *s = new ComediSensor(parent, name, comedi_in);
  s->setMin(unit_min);
  s->setMax(unit_max);
  s->setUnit(units);  
  return true;
}

ComediDataLogable::ComediDataLogable(const ComediChan & aio_template, int id)
  : ComediChan(aio_template), m_id(id)
{}

ComediDataLogable::~ComediDataLogable() 
{ 
  setLoggingEnabled( false, Raw );
  setLoggingEnabled( false, Cooked );
  setLoggingEnabled( false, Other );
}

bool ComediDataLogable::setLoggingEnabled(bool b, DataType t) 
{ // nb: datatype will always be raw no matter what you pass in
  (void)t;
  if (!getDAQ()) return false;
  return getDAQ()->setDataLogging(chan(), b, m_id);
}

bool ComediDataLogable::loggingEnabled(DataType t) const
{
  if (t != Raw || !getDAQ()) return false;
  return getDAQ()->getDataLogging(chan());
}

MutExIOBank::MutExIOBank(Component *parent, const std::string & name,
                         const std::vector<ComediChan> & iozz)
  : Actuator(parent, name), ios(iozz) 
{
  std::vector<ComediChan>::iterator it;
  for (it = ios.begin(); it != ios.end(); ++it) {    
    it->setDIOOutput();
    if (it == ios.begin()) it->dataWrite(it->rangeMax()); // set line 0 high by default
    else it->dataWrite(it->rangeMin());
  }
  m_lastWrittenRaw = m_lastWritten = 0;
  setUnit("Chan.");
  setMin(0);
  setMax(numIOS()-1);
}

MutExIOBank::~MutExIOBank() 
{
  setLoggingEnabled( false, Raw );
  setLoggingEnabled( false, Cooked );
  setLoggingEnabled( false, Other );
}

bool MutExIOBank::writeRaw(double d) 
{
  unsigned id = static_cast<unsigned>(d), cur = unsigned(m_lastWritten);
  if (id >= numIOS()) return false;
  ios[cur].dataWrite(ios[cur].rangeMin()); // set old line low
  ios[id].dataWrite(ios[id].rangeMax()); // set new line high
  m_lastWrittenRaw = m_lastWritten = id; // save new valve id 
  return true;
}

double MutExIOBank::readRaw() { return m_lastWrittenRaw; }

bool MutExIOBank::setLoggingEnabled(bool b, DataType t)
{ // nb: datatype will always be raw no matter what you pass in
  (void)t;
  bool ok = true;
  for (unsigned i = 0; ok && i < numIOS(); ++i) 
    if (ios[i].getDAQ())
      ok = ios[i].getDAQ()->setDataLogging(ios[i].chan(), b, id());
  return ok;
}

bool MutExIOBank::loggingEnabled(DataType t) const
{
  bool ok = 1;
  
  for (unsigned i = 0; ok && i < numIOS(); ++i) {
    if (t != Raw || !ios[i].getDAQ()) ok = false;
    else ok = ios[i].getDAQ()->getDataLogging(ios[i].chan());  
  }
  return ok;
}

DOEnableable::DOEnableable(Component *actuator_parent,
                           const ComediChan *he,
                           const ComediChan *se)
  : hard_enable(0), soft_enable(0)
{
  if (he) hard_enable = new ComediActuator(actuator_parent, 
                                           actuator_parent->name() + "_HardEn",
                                           *he);
  if (se) soft_enable = new ComediActuator(actuator_parent, 
                                           actuator_parent->name() + "_SoftEn",
                                           *se);

  if (hard_enable) {
    hard_enable->setUnit("B");
    hard_enable->setMin(0.);
    hard_enable->setMax(1.);
    hard_enable->setDIOOutput();
  }
  if (soft_enable) {
    soft_enable->setUnit("B");
    soft_enable->setMin(0.);
    soft_enable->setMax(1.);
    soft_enable->setDIOOutput();
  }
  
  setEnabled(true, Soft);
  setEnabled(true, Hard);

}

DOEnableable::~DOEnableable()
{
  setEnabled(false, Soft);
  setEnabled(false, Hard);
  delete hard_enable;
  delete soft_enable;
  hard_enable = 0;
  soft_enable = 0;
}

void DOEnableable::setEnabled(bool en, Type et)
{
  Enableable::setEnabled(en, et);
  switch(et) {
  case Soft:
    if (soft_enable) soft_enable->write( en ? 1. : 0. );
  break;
  case Hard:
    if (hard_enable)
      hard_enable->write( en ? 0. : 1. ); /* hard line is inverted */;
    break;
  case Both:
    setEnabled(en, Hard); // call ourselves twice so that the above write()s happen
    setEnabled(en, Soft);
    break;
  }
}
