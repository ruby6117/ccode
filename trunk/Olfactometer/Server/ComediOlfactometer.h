#ifndef ComediOlfactometer_H
#define ComediOlfactometer_H

#include <string>
#include <map>
#include <list>
#include <vector>
#include <comedilib.h>
#include "ProbeComedi.h"
#include "Olfactometer.h"
#include "Settings.h"
#include "Conf.h"
#include "RTLCoprocess.h"
#include "DataLog.h"
#include "ComediChan.h"
#include "DAQTaskProxy.h"
#include "DataLogable.h"
#include "Saveable.h"
#include "Calib.h"

// inheritence order is important! we need Olfactometer d'tor called before probedcomedidevices d'tor!
class ComediOlfactometer : public ProbedComediDevices, public Olfactometer
{
public:
  ComediOlfactometer();
  virtual ~ComediOlfactometer();
  bool doSetup(Settings &, RTLCoprocess *); ///< probes comedi, constructs children, etc
  void clearSetup();

  /// may return null if no datalog exists
  DataLog *dataLog() { return coprocess; }

private:
  std::map<std::string, ComediDevice *> useBoards;
  std::map<std::string, DAQTaskProxy *> daqTasks;
  
  RTLCoprocess *coprocess;

  // all of these throw a setup exception
  bool doGeneralSetup(const Settings &);
  bool doBoardSetup(const Settings &);
  bool doDaqTaskSetup(const Settings &);
  bool doLayoutSetup(Settings &);


  // static helper functions
  static std::string replaceHostName(const std::string & str);
  static StringList split(const std::string & str); // splits on commas
  static bool validateLayoutSection(const Settings & ini, StringList & mixes, StringList & flow_controllers, StringList & extra_meters, StringList & pwm_valves, StringList &sensors);
  bool buildMixSection(Settings &, const std::string &mixname);  
  /** generic flow controller factory routine  */
  bool buildFlowController(Settings &, Component *parent, const std::string &fcname, std::string & error_out) const;
  bool buildFlowMeter(const Settings &, Component *parent, const std::string &fcname, std::string & error_out) const;
  bool buildPWM(const Settings &, Component *parent, const std::string &name, std::string & error_out) const;
  bool buildSensor(const Settings &, Component *parent, const std::string &name, std::string & error_out) const;

  friend struct BuildBanks;
};

class ComediDataLogable : public ComediChan, public DataLogable
{
public:
  ComediDataLogable(const ComediChan &aio_template, int id);
  virtual ~ComediDataLogable();

  // from datalogable
  bool setLoggingEnabled(bool, DataType); // nb: datatype will always be raw no matter what you pass in
  bool loggingEnabled(DataType) const;
protected:
  int m_id;
};

class ComediSensor : public Sensor, public ComediDataLogable
{
public:
  ComediSensor(Component *parent, const std::string & name, const ComediChan & aio_template);
  ~ComediSensor();
  
  virtual double read();
  virtual double readRaw();
};

class ComediActuator : public Actuator, public ComediDataLogable
{
public:
  ComediActuator(Component *parent, const std::string & name, const ComediChan & aio_template);
  ~ComediActuator();

  // from inherited Actuator class
  virtual bool write(double d);
  virtual bool writeRaw(double d);
  virtual double read();
  virtual double readRaw();
};

/// like the ComediActuator but it applies calibration settings of its parent
/// only used for regular aalborg flow controllers
/// for PID flows the calibration settings are *in the kernel task* and 
/// this class is not used
class CalibComediActuator : public ComediActuator
{
public:
  CalibComediActuator(Component *parent, const std::string & name, const ComediChan & aio_template);

  virtual bool write(double d);
  virtual double read();

};

/// like the ComediSensor but it applies calibration settings of its parent
/// only used for regular aalborg flow controllers -- PIDFlows apply the 
/// calibration settings *in the kernel task*
class CalibComediSensor : public ComediSensor
{
public:
  CalibComediSensor(Component *parent, const std::string & name, const ComediChan & aio_template);

  virtual double read();
};

class CalibFlowController : public FlowController, public Calib, public Saveable
{
public:
  CalibFlowController(Settings & ini, Sensor *s, Actuator *a, Component *parent = 0,
                      const std::string & name = "UnnamedFlow")
    : FlowController(s, a, parent, name), ini(ini) {}
  bool setCoeffs(const Coeffs & c) { coeffs = c; return true; }
  bool save() { return Calib::save(ini, name()); }
private:
  Settings & ini;
};


/// mutually exclusive IO bank -- basically a set of IO channels where only
/// one of them can be high at any given time
class MutExIOBank : public Actuator, public DataLogable
{
public:
  MutExIOBank(Component *parent, const std::string & name,
              const std::vector<ComediChan> & ios);
  virtual ~MutExIOBank();
  /// write a number form 0->(numIOS()-1) to enable 1 line
  bool writeRaw(double);
  bool write(double d) { return writeRaw(d); }
  /// returns a number form 0->(numIOS()-1) to indicate which line is high
  double read() { return readRaw(); }
  double readRaw();

  unsigned numIOS() const { return ios.size(); }

  // from datalogable
  bool setLoggingEnabled(bool, DataType); // nb: datatype will always be raw no matter what you pass in
  bool loggingEnabled(DataType) const;

private:
  std::vector<ComediChan> ios;
};

class DOEnableable : public Enableable
{
public:
  DOEnableable(Component *actuator_parent,
               const ComediChan *hard_enable,
               const ComediChan *soft_enable);
  ~DOEnableable();
  void setEnabled(bool en, Type = Both); // overrides parent
  
protected:
  ComediActuator *hard_enable, *soft_enable;
};

class ComediBank : public Bank, public Saveable, public DOEnableable
{
public:

  /// for now we hardcoded the requirement that all valve banks require 16 valves
  enum MiscVars { RequiredValves = 16 };

  ComediBank(Component *parent, const std::string & name, 
             const std::vector<ComediChan> & valves,
             const ComediChan & hard_enable,
             const ComediChan & soft_enable,
             Settings &);
  virtual ~ComediBank();
  bool setOdor(unsigned odor);
  bool save();
//   bool write(double d) { return writeRaw(d); }
//   bool writeRaw(double d) { return setOdor(static_cast<unsigned>(d)); }
//   double read() { return currentOdor(); }
//   double readRaw() { return valves.readRaw(); }
private:
  MutExIOBank valves;
  Settings & ini;
};


#endif

