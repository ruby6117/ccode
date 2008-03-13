#include "Component.h"
#include <algorithm>

/* static */ const std::string Component::NAMELESS ("(NamelessComponent)");
/* static */ unsigned Component::compIdCtr = 0;
/* static */ Component::NameMap Component::nameCompMap; ///< map component names to component instances
/* static */ Component::IdCompMap Component::idCompMap; ///< map component id's to component instances

/// construct a new nested named component, adding it to parent's list
Component::Component(Component *p, const std::string & n)
  : _parent(0), nam(""), cid(compIdCtr++)
{
  if (!n.length()) setName(NAMELESS);
  else setName(n);
  reparent(p);
  if (name() != NAMELESS) nameCompMap[name()] = this;
  idCompMap[id()] = this;
}

Component::~Component()
{
  reparent(0);
  ComponentList childListCopy = childList; // need to work on a copy of the set due to iterator problems when deleting the children which attempt to remove themselves from the set..
  // clear these two to make the below delete call (which ultimately calls
  // reparent and childAdded()) peppier..
  childList.clear(); 
  childNameMap.clear(); 
  for (ComponentList::iterator it = childListCopy.begin(); it != childListCopy.end(); ++it) 
    delete (*it);
  idCompMap.erase(id());
  NameMap::iterator it = nameCompMap.find(nam);
  if (it != nameCompMap.end() && it->second == this) nameCompMap.erase(it);  
}

void
Component::reparent(Component *newParent)
{
  if (_parent) _parent->childRemoved(this);
  _parent = newParent;
  if (_parent) _parent->childAdded(this);
}

void 
Component::childAdded(Component *c)
{
  childList.push_back(c);
  childNameMap[ c->name() ] = c;
}

void
Component::childRemoved(Component *c)
{
  childNameMap.erase(c->name());
  ComponentList::iterator it =   std::find(childList.begin(), childList.end(), c);
  if (it != childList.end()) childList.erase(it);
}

/// return a list of all children.  If recursive, flatten out the 
/// nested hierarchy by putting all components and their children, etc
/// in the list in a depth-first-search
std::list<Component *> 
Component::children(bool recursive) const
{
  std::list<Component *> ret;
  for (ComponentList::const_iterator it = childList.begin(); it != childList.end(); ++it) {
    ret.push_back(const_cast<Component *>(*it));
    if (recursive) {
      std::list<Component *> chld ((*it)->children(true));
      ret.splice(ret.end(), chld);
    }
  }
  return ret;
}

/// find a component by name.  If searchRecursive is true, broadens the
/// search to all subcomponents in a depth-first-search
Component *
Component::find(const std::string & name, bool recursive) const
{
  NameMap::const_iterator n_it = childNameMap.find(name);
  if (n_it != childNameMap.end()) 
    return const_cast<Component *>(n_it->second);
  else if (recursive) {
    for (ComponentList::const_iterator it = childList.begin(); it != childList.end(); ++it) {
      Component *c = const_cast<Component *>(*it);
      if ((c = c->find(name, true))) return c;
    }
  }
  return 0;  
}

std::list<Component *>
Component::findAll(const std::string & name, bool recursive) const
{
  std::list<Component *> ret;
  for (ComponentList::const_iterator it = childList.begin(); it != childList.end(); ++it) {
    Component *c = const_cast<Component *>(*it);
    if (c->name() == name) ret.push_back(c);
    if (recursive) {
      std::list<Component *> chld(c->findAll(name, true));
      ret.splice(ret.end(), chld);
    }
  }
  return ret;  
}

/// sets the name of this component
void 
Component::setName(const std::string &name)
{
  NameMap::iterator it = nameCompMap.find(nam);
  if (it != nameCompMap.end() && it->second == this) nameCompMap.erase(it);
  nam = name;
  if (name != NAMELESS) nameCompMap[name] = this;
  if (_parent) _parent->childRenamed(this);
}

void 
Component::recalcNameMap()
{
  childNameMap.clear();
  ComponentList::iterator it;
  for (it = childList.begin(); it != childList.end(); ++it)
    childNameMap[ (*it)->name() ] = *it;
}

void 
Component::childRenamed(Component *child) 
{
  (void)child;
  recalcNameMap();
}

void 
Component::deleteAllChildren() const
{
  std::list<Component *> chlds = children();
  for (std::list<Component *>::iterator it = chlds.begin(); it != chlds.end(); ++it)
    delete *it;
}

/// globally find a component instance by id, or null if not found
/*static*/ Component * Component::findById(unsigned id)
{
  IdCompMap::iterator it = idCompMap.find(id);
  if (it != idCompMap.end()) return it->second;
  return 0;
}

/// globally find a component instance by name, or null if not found
/*static*/ Component * Component::findByName(const std::string & name)
{
  NameMap::iterator it = nameCompMap.find(name);
  if (it != nameCompMap.end()) return it->second;
  return 0;
}

/// globally find a component id by name, or ~0UL if not found
/*static*/ unsigned Component::idFromName(const std::string & name)
{
  Component *c = findByName(name);
  if (c) return c->id();
  return ~0U;
}

/// globally find a component name by id, or empty string if not found
/*static*/ std::string Component::nameFromId(unsigned id)
{
  Component *c = findById(id);  
  if (c) return c->name();
  return "";
}
