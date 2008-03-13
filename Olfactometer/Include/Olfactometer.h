#ifndef Olfactometer_H
#define Olfactometer_H

#include <string>
#include <list>
#include <map>

#include "Component.h"
#include "Lockable.h"
#include "Common.h"

class UnitRangeObject 
{
public:
  UnitRangeObject() : m_min(0), m_max(1), m_unit("U") {}
  virtual ~UnitRangeObject() {}
  double min() const { return m_min; }
  double max() const { return m_max; }
  std::string unit() const { return m_unit; }
  virtual void setMin(double d) { m_min = d; }
  virtual void setMax(double d) { m_max = d; }
  virtual void setUnit(const std::string &unit) { m_unit = unit; }
protected:
  double m_min, m_max;
  std::string m_unit;
};

class ErrorPercentObject
{
public:
  ErrorPercentObject() : error_pct(0.01) {}
  virtual ~ErrorPercentObject() {}
  /// error percent is a value from 0-1.0 (1.0 being 100%)
  double errorPercent() const { return error_pct; }
  /// set error percent --  a value from 0-1.0 (1.0 being 100%)
  virtual void setErrorPercent(double e) { error_pct = e; }
protected:
  double error_pct;
};

class Readable
{
public:
  virtual ~Readable() {}
  virtual double read()  = 0;
  virtual double readRaw()  = 0;
};

class Writeable
{
public:
  virtual ~Writeable() {}
  virtual bool write(double) = 0;
  virtual bool writeRaw(double) = 0;
};

class Enableable
{
public:

  enum Type { Hard, Soft, Both };

  Enableable() : hard_enabled(true), soft_enabled(true) {}
  virtual ~Enableable() {}

  virtual bool isEnabled(Type t = Both) const;
  virtual void setEnabled(bool en, Type t = Both);

protected:
  bool hard_enabled, soft_enabled;
};

class Sensor : public Component, 
               public UnitRangeObject, 
               virtual public Readable, 
               virtual public ErrorPercentObject
{
public:
  Sensor(Component *parent = 0,
         const std::string & name = "UnnamedSensor");
  virtual ~Sensor();

  /// reimplement in subclass to do calibration, return false if calib failed
  virtual bool calibrate(Sensor *sensor_reference) { (void)sensor_reference; return true; }

  /// override from generic component
  virtual std::string typeName() const { return "Sensor"; }
};

class Actuator : public Component, 
                 public UnitRangeObject, 
                 virtual public Writeable, 
                 virtual public Readable, 
                 virtual public ErrorPercentObject
{
public:
  Actuator(Component *parent = 0,
           const std::string & name = "UnnamedActuator");
  virtual ~Actuator();

  /// override from generic component
  virtual std::string typeName() const { return "Actuator"; }

protected:
  virtual double lastWritten() const { return m_lastWritten; }
  virtual double lastWrittenRaw() const { return m_lastWrittenRaw; }

  double m_lastWritten, m_lastWrittenRaw;
};


/// NullActuator just stores the set values and returns them when read()
class NullActuator : public Actuator
{
public:
  NullActuator(Component *parent, const std::string & name) 
    : Actuator(parent, name) {}
  virtual ~NullActuator() {}
  virtual double read() { return lastWritten(); }
  virtual double readRaw() { return lastWrittenRaw(); }
  virtual bool write(double v); 
  virtual bool writeRaw(double v);
};

/// LoopbackSensor just reads the raw values of its Actuator sibling
class LoopbackSensor : public Sensor
{
public:
  LoopbackSensor(Component *parent, const std::string & name, Actuator *sibling)
    : Sensor(parent, name), sib(sibling) {}
  virtual double read() { return sib->read(); }
  virtual double readRaw() { return sib->readRaw(); }
protected:
  Actuator *sib;
};


class FlowMeter : public Component, 
                  public UnitRangeObject, 
                  virtual public Readable
{
public:
  /// NB: reparents sensor this
  FlowMeter(Sensor * sensor, Component *parent = 0, const std::string & name = "UnnamedMeter");
  virtual ~FlowMeter();
  virtual double flow() const { return sensor ? sensor->read() : 0.; }
  void setMin(double d);
  void setMax(double d);
  
  /// from readable interface, forwards request to sensor child
  double read() { return sensor->read(); }
  double readRaw() { return sensor->readRaw(); }

  /// override from generic component
  virtual std::string typeName() const { return "FlowMeter"; }

  /// return the actual error magnitude in ml/min -- defaults to 1% error 
  virtual double errorMagnitude() const { return sensor ? (max()-min()) * sensor->errorPercent() : 0.; }

protected:
  virtual void childRemoved(Component *child); ///< need to still call on override
  virtual void childAdded(Component *child); ///< need to still call on override
  Sensor *sensor;  
};

class FlowController : public FlowMeter, virtual public Writeable
{
public:
  /// NB: reparents sensor and actuator to this
  FlowController(Sensor *sensor, Actuator *actuator,
                 Component *parent = 0,
                 const std::string & name = "UnnamedFlow");
  virtual ~FlowController();
  virtual double commandedFlow() const { return flow_set; }
  virtual bool setFlow(double flow);

  /// override from generic component
  virtual std::string typeName() const { return "FlowController"; }

  /// from Writeable interface -- forwards request to flow controller
  bool write(double d) { return actuator->write(d); }
  /// from Writeable interface -- forwards request to flow controller
  bool writeRaw(double d) { return actuator->writeRaw(d); }

  virtual void setMin(double m);
  virtual void setMax(double m);

protected:
  virtual void childRemoved(Component *child); ///< need to still call on override
  virtual void childAdded(Component *child); ///< need to still call on override
  Actuator *actuator;
  double flow_set;
};


struct Odor
{
  static int num;
  Odor() : name(std::string("UnnamedOdor") + Str(num++)) {}
  Odor(const std::string & name, const std::string &meta = "") : name(name), metadata(meta) {}
  std::string name;
  std::string metadata;
};

class Bank : public Component, 
             virtual public Readable, 
             virtual public Writeable
{
public:

  typedef std::map<unsigned, Odor> OdorTable;
  
  Bank(Component * parent = 0,
       const std::string & name = "UnnamedBank", 
       unsigned numOdors = 0, 
       unsigned cleanOdor = 0, 
       const OdorTable & = OdorTable());
  virtual ~Bank();
  virtual bool setOdor(unsigned odor);
  bool setOdor(const std::string & name);
  bool resetOdor() { return setOdor(clean_odor); } ///< sets the bank to the 'clean' odor
  unsigned currentOdor() const { return currentodor; }
  const std::string & currentOdorName() const;
  double actualFlow() const { return flow->flow(); }
  double commandedFlow() const { return flow->commandedFlow(); }
  bool setFlow(double fl) { return flow->setFlow(fl); }
  unsigned numOdors() const { return num_odors; }
  OdorTable odorTable() const { return odor_table; }
  virtual void setOdorTable(const OdorTable &);

  /// use this method with caution as it violates encapsulation
  FlowController * getFlowController() { return flow; }


  /// from Readable interface -- reads just the valves and returns current odor
  double read() { return currentOdor(); }
  /// from Readable interface -- same as read() above
  double readRaw() { return currentOdor(); }

  /// from Writeable interface -- just sets the odor calling setOdor()
  bool write(double d) { return setOdor(static_cast<unsigned>(d)); }
  /// from Writeable interface -- just sets the odor calling setOdor()
  bool writeRaw(double d) { return getFlowController()->writeRaw(d); }

  /// override from generic component
  virtual std::string typeName() const { return "Bank"; }

protected:
  friend class Mix;
  void setFlowController(FlowController *f) { flow = f; }
  void childAdded(Component *c);
  void childRemoved(Component *c);
private:
  unsigned currentodor, num_odors, clean_odor;
  FlowController *flow;
  OdorTable odor_table;
};

class Mix : public ChildTypeCounter<Bank>
{
public:
  Mix(Component *parent = 0, const std::string & name = "");
  virtual ~Mix();  
  unsigned numBanks() const { return typeCount(); }
  std::list<std::string> bankNames() const;
  virtual bool setDesiredTotalFlow(double f, bool apply=true);
  double desiredTotalFlow() const { return desired_total_flow; }
  // TODO HERE: see about shutting off unspecified flows if apply=true?
  virtual bool setOdorFlow(double fl, bool apply=true); ///< sets the total flow for all odors in this mix
  double odorFlow() const { return odor_flow; } ///< return the commanded oder flow as we remember it
  double actualOdorFlow() const; ///< returns the actual flow total for all odors in this mix
  double commandedOdorFlow() const; ///< returns the commanded flow total for all odors in this mix -- this actuall loops over all the banks
  typedef std::map<std::string, double> MixtureRatio;
  virtual bool setMixtureRatios(const MixtureRatio & m, bool apply=true);
  MixtureRatio mixtureRatios() const { return mix_ratio; }
  void setOdorTable(const std::string &, const Bank::OdorTable &);

  // use these with caution!
  FlowController * getCarrier() { return carrier; }
  Bank * getBank(const std::string & name) { return dynamic_cast<Bank *>(find(name)); }

  /// override from generic component
  virtual std::string typeName() const { return "Mix"; }

protected:
  void setCarrier(FlowController *f) { carrier = f; }
  void childAdded(Component *c);
  void childRemoved(Component *c);
private:
  FlowController *carrier;
  double desired_total_flow, odor_flow; 
  MixtureRatio mix_ratio;
  double mix_total_sum;
};

class Olfactometer : public ChildTypeCounter<Mix>, public Lockable
{
public:
  Olfactometer(Component *parent = 0, const std::string & name = "NamelessOlfactometer");
  virtual ~Olfactometer();
  Mix * mix(const std::string & mixname) { return dynamic_cast<Mix *>(find(mixname)); }
  std::list<Mix *> mixes() const { return typedChildren(); }
  unsigned numMixes() const { return typeCount(); }
  std::string description() const { return m_description; }

  /// override from generic component
  virtual std::string typeName() const { return "Olfactometer"; }

protected:
  std::string m_description;
};


#endif
