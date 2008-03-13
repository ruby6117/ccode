#include "Olfactometer.h"


FlowController::FlowController(Sensor * s, Actuator *a, Component *p, const std::string & name)
  : FlowMeter(s, p, name),  actuator(a), flow_set(0)
{
  // hack to make Bank and Mix work properly
  if (p && (dynamic_cast<Mix *>(p) || dynamic_cast<Bank *>(p)) ) {
    reparent(0);
    reparent(p);
  }
  if (a && a->parent() != this) a->reparent(this);
}

FlowController::~FlowController() {}

FlowMeter::FlowMeter(Sensor * s, Component *p, const std::string & name)
  : Component(p, name),  sensor(s)
{
  setUnit("ml/min");
  if (s && s->parent() != this) s->reparent(this);
}

FlowMeter::~FlowMeter() {}

bool FlowController::setFlow(double flow)
{
  if (actuator && actuator->write(flow)) {
    flow_set = flow;
    return true;
  }
  return false;
}

Bank::Bank(Component *p,
           const std::string & name, 
           unsigned numOdors, unsigned cleanOdor, const OdorTable & ot)
  : Component(p, name), currentodor(0), num_odors(numOdors), clean_odor(cleanOdor),
    flow(0)
{
  if (clean_odor >= num_odors) clean_odor = 0;
  setOdorTable(ot);
  // hack to make Mix work properly
  if (p && dynamic_cast<Mix *>(p) ) {
    reparent(0);
    reparent(p);
  }
}

Bank::~Bank() {}

void 
Bank::childAdded(Component *c)
{
  FlowController *f;
  if ( (f = dynamic_cast<FlowController *>(c)) ) setFlowController(f);  
  Component::childAdded(c);
}

void 
Bank::childRemoved(Component *c)
{
  FlowController *f;
  if ( (f = dynamic_cast<FlowController *>(c)) ) setFlowController(0);  
  Component::childRemoved(c);
}

struct FindOdorByNameFunctor
{
  FindOdorByNameFunctor(const std::string &name) : name(name) {}
  const std::string name;
  bool operator()(const Bank::OdorTable::value_type & it) const
  { return it.second.name == name; }
};

#include <algorithm>

bool Bank::setOdor(const std::string & name)
{
  OdorTable::iterator it = std::find_if(odor_table.begin(), odor_table.end(), FindOdorByNameFunctor(name));
  if (it != odor_table.end()) return setOdor(it->first);
  return false;
}

bool Bank::setOdor(unsigned o)
{
  if (o < numOdors()) {
    currentodor = o;
    return true;
  }
  return false;
}

struct OdorMapPairLessFunctor
{
  bool operator()(const std::pair<unsigned, Odor> & item1, 
                  const std::pair<unsigned, Odor> & item2) 
  { return item1.first < item2.first; }
};
void Bank::setOdorTable(const OdorTable & ot) 
{
  if (!num_odors) { odor_table = OdorTable(); return; }
  odor_table = ot;
  for (unsigned i = 0; i < num_odors; ++i)
    if (odor_table.find(i) == odor_table.end()) 
      odor_table[i] = Odor();
  OdorTable tmp;
  tmp[num_odors];
  if (odor_table.size() > num_odors) {
    OdorTable::iterator it = std::lower_bound(odor_table.begin(), odor_table.end(), *tmp.begin(), OdorMapPairLessFunctor());
    odor_table.erase(it, odor_table.end());
  }
}

const std::string & Bank::currentOdorName() const
{
  static const std::string dummy;
  OdorTable::const_iterator it;
  if (odor_table.find(currentOdor()) == odor_table.end()) return dummy;
  return it->second.name;
}

Mix::Mix(Component *p, const std::string & name)
  : ChildTypeCounter<Bank>(p, name), carrier(0)
{
  mix_total_sum = 0;
  desired_total_flow = 0;
}

void Mix::childAdded(Component *c)
{
  FlowController *f;
  if ( (f = dynamic_cast<FlowController *>(c)) ) setCarrier(f);
  ChildTypeCounter<Bank>::childAdded(c);
  Bank *b;
  if ( (b = dynamic_cast<Bank *>(c)) ) { 
    mix_ratio[b->name()] = 1.0;
    setMixtureRatios(mix_ratio);
  }
}

void Mix::childRemoved(Component *c)
{
  FlowController *f;
  if ( (f = dynamic_cast<FlowController *>(c)) ) setCarrier(0);
  ChildTypeCounter<Bank>::childRemoved(c);
  Bank *b;
  if ( (b = dynamic_cast<Bank *>(c)) ) { 
    mix_ratio.erase(b->name());
    setMixtureRatios(mix_ratio);
  }
}


Mix::~Mix() {}

std::list<std::string> Mix::bankNames() const
{
  std::list<std::string> ret;
  std::list<Bank *> theBanks = typedChildren();
  std::list<Bank *>::const_iterator it;
  for (it = theBanks.begin(); it != theBanks.end(); ++it)
    ret.push_back((*it)->name());
  return ret;
}

double Mix::commandedOdorFlow() const
{
  std::list<Bank *>::const_iterator it;
  std::list<Bank *> theBanks = typedChildren();
  double ret = 0.0;
  for (it = theBanks.begin(); it != theBanks.end(); ++it)
    ret += (*it)->commandedFlow();
  return ret;
}

double Mix::actualOdorFlow() const
{
  std::list<Bank *>::const_iterator it;
  std::list<Bank *> theBanks = typedChildren();
  double ret = 0.0;
  for (it = theBanks.begin(); it != theBanks.end(); ++it)
    ret += (*it)->actualFlow();
  return ret;
}

bool Mix::setDesiredTotalFlow(double f, bool apply)
{
  double of = odorFlow();

  if (apply) {
    double diff = f - of;
    if (!carrier->setFlow(diff)) return false;
  } else {
    if (f < carrier->min() || f > carrier->max()
        || f < of) return false;
  }
  desired_total_flow = f;
  return true;
}

bool Mix::setMixtureRatios(const MixtureRatio & m, bool apply)
{
  mix_total_sum = 0;
  for (MixtureRatio::const_iterator it = m.begin(); it != m.end(); ++it) {
    Bank *b = getBank(it->first);
    if (b) mix_total_sum += it->second;
  }
  mix_ratio = m;
  if (apply) 
    return setOdorFlow(odor_flow);
  return true;
}

bool Mix::setOdorFlow(double fl, bool apply)
{
    for (MixtureRatio::iterator it = mix_ratio.begin(); it != mix_ratio.end(); ++it) {
      Bank *b = getBank(it->first);
      if (b && b->getFlowController()) {
        double frac = mix_total_sum ? it->second/mix_total_sum : 0;
        double flow = frac * fl;
        if (flow < b->getFlowController()->min() || flow > b->getFlowController()->max()) return false;
      } else 
        return false;
    }
    if (apply) {
      //std::list<Bank *> theBanks = typedChildren();

      // first set them all off?
      //for (std::list<Bank *>::iterator it = theBanks.begin(); it != theBanks.end(); ++it)
      //  (*it)->setEnabled(false, Bank::Soft);

      for (MixtureRatio::iterator it = mix_ratio.begin(); it != mix_ratio.end(); ++it) {
        Bank *b = getBank(it->first);
        if (b) {
          double frac = mix_total_sum ? it->second/mix_total_sum : 0;
          double flow = frac * fl;
          if (!b->setFlow(flow)) return false; // argh! what to do about errors here?!
          //b->setEnabled(true, Bank::Soft);

        }
      }
    }
  odor_flow = fl;
  return true;
}

void Mix::setOdorTable(const std::string &s, const Bank::OdorTable &ot)
{
  Bank *b = getBank(s);
  if (!b) return;
  b->setOdorTable(ot);
}

Olfactometer::Olfactometer(Component *parent,
                           const std::string & n)
  : ChildTypeCounter<Mix>(parent, n)
{}

Olfactometer::~Olfactometer() {}

/* static */ int Odor::num = 0;

Sensor::Sensor(Component *parent,
               const std::string & name) : Component(parent, name) {}
Sensor::~Sensor() {}
Actuator::Actuator(Component *parent,
                   const std::string & name) 
  : Component(parent, name), m_lastWritten(0.), m_lastWrittenRaw(0.)
 {}
Actuator::~Actuator() {}

void FlowMeter::setMin(double d)
{
  UnitRangeObject::setMin(d);
  if (sensor) sensor->setMin(d);
}

void FlowMeter::setMax(double d)
{
  UnitRangeObject::setMax(d);
  if (sensor) sensor->setMax(d);
}

void FlowController::setMin(double m) 
{
  FlowMeter::setMin(m);
  if (actuator) actuator->setMin(m);
  if (flow_set < m) setFlow(m); 
}

void FlowController::setMax(double m) 
{ 
  FlowMeter::setMax(m);
  if (actuator) actuator->setMax(m);
  if (flow_set > m) setFlow(m); 
}

void FlowController::childRemoved(Component *child)
{
  if (child == actuator) actuator = 0;
  FlowMeter::childRemoved(child);
}

void FlowController::childAdded(Component *child)
{  
  Actuator *actuator;

  if ( ( actuator = dynamic_cast<Actuator *>(child) )) {
    actuator->setUnit(unit());
    actuator->setMin(min());
    actuator->setMax(max());
  }
  FlowMeter::childAdded(child);
}

void FlowMeter::childRemoved(Component *child)
{
  if (child == sensor) sensor = 0;
  Component::childRemoved(child);
}

void FlowMeter::childAdded(Component *child)
{  
  Sensor *s = 0;
  if ((s = dynamic_cast<Sensor *>(child))) {
    s->setMin(min());
    s->setMax(max());
    s->setUnit(unit());
  }
  Component::childAdded(child);
}

bool NullActuator::write(double v)
{
  double raw = 0.;
  if (max()-min() != 0.) {
    raw = (v-min())/(max()-min());
  }
  m_lastWrittenRaw = raw;
  m_lastWritten = v;
  return true;
}

bool NullActuator::writeRaw(double r)
{
  double v = r * (max()-min()) + min();
  m_lastWrittenRaw = r;
  m_lastWritten = v;
  return true;
}


//virtual 
bool Enableable::isEnabled(Type t) const  
{
  switch (t) { 
  case Soft: return soft_enabled;
  case Hard: return hard_enabled;
  case Both: return soft_enabled && hard_enabled;
  }
  return false;
}

//virtual 
void Enableable::setEnabled(bool en, Type t)  
{
  switch (t) { 
  case Soft: soft_enabled = en; break;
  case Hard: hard_enabled = en; break;
  case Both: hard_enabled = soft_enabled = en; break;
  }
}
