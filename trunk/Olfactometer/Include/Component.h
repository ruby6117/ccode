#ifndef Component_H
#define Component_H

#include <string>
#include <list>
#include <map>
#include <algorithm>

class Component
{
public:
  static const std::string NAMELESS;

  /// construct a new nested named component, adding it to parent's list
  Component(Component *parent = 0, const std::string & name = NAMELESS);
  virtual ~Component();
  
  /// return a list of all children.  If recursive, flatten out the 
  /// nested hierarchy by putting all components and their children, etc
  /// in the list in a depth-first-search
  std::list<Component *> children(bool recursive = false) const;

  /// find a component by name.  If searchRecursive is true, broadens the
  /// search to all subcomponents in a depth-first-search
  Component *find(const std::string & name, bool recursive = true) const;

  /// find all components with a given name.  Note that it's bad to 
  /// have more than 1 component share a name, but this function
  /// lets you find components of the same name
  std::list<Component *> findAll(const std::string & name, bool recursive = true) const;
  
  /// returns the parent of this component, or NULL if parentless
  Component *parent() const { return _parent; }

  /// returns the name of this component
  std::string name() const { return nam; }

  /// override this if you want your specific subclass to tell the outside world its type's name
  virtual std::string typeName() const { return "Component"; }

  /// sets the name of this component
  void setName(const std::string &name);

  /// reparents this component to a new parent
  virtual void reparent(Component *newParent); ///< if overriding, call this

  /// returns the unique identifier for this component instance
  unsigned id() const { return cid; }

  /// globally find a component instance by id, or null if not found
  static Component *findById(unsigned id);
  /// globally find a component instance by name, or null if not found
  static Component *findByName(const std::string & name);
  /// globally find a component id by name, or ~0U if not found
  static unsigned idFromName(const std::string & name);
  /// globally find a component name by id, or empty string if not found
  static std::string nameFromId(unsigned id);

protected:
  /// reparents this component to a new parent
  virtual void childRenamed(Component *child); ///< if overriding, call this
  virtual void childRemoved(Component *child); ///< need to still call on override
  virtual void childAdded(Component *child); ///< need to still call on override
  void deleteAllChildren() const;

private:
  Component(const Component &c) { (void)c; } ///< no copy constructor
  /// no operator=
  Component & operator=(const Component &rhs) { (void)rhs; return *this; } 

  void recalcNameMap();

  Component *_parent;
  std::string nam;
  unsigned cid; ///< component id -- unique identifier for a component instance
  typedef std::list<Component *> ComponentList;
  typedef std::map<std::string, Component *> NameMap;
  ComponentList childList;
  NameMap childNameMap;

  static unsigned compIdCtr; ///< source for component id's
  typedef std::map<unsigned, Component *> IdCompMap;
  static NameMap nameCompMap; ///< map component names to component instances
  static IdCompMap idCompMap; ///< map component id's to component instances

};

template <typename T>
class ChildTypeCounter : public Component
{
public:
  ChildTypeCounter(Component *c = 0, const std::string & n = NAMELESS) 
    : Component(c, n), dirty(false), ct(0) {}

  /// caveat: don't call this from a child's childRemoved() method!
  unsigned typeCount() const 
    { chkDirty();  return ct;   }

  /// caveat: don't call this from a child's childRemoved() method!
  typename std::list<T *> typedChildren() const 
    { chkDirty(); return typedchildren;  }

protected:
  virtual void childAdded(Component *c) { 
    //NB: we have to pend the update of the childTypeList because at this 
    //point 'c' has no dynamic type yet
    Component::childAdded(c);
    dirty = true;
  }
  
  virtual void childRemoved(Component *c) { 
    //NB: we have to pend the update of the childTypeList because at this 
    //point 'c' has lost its dynamic type
    Component::childRemoved(c);
    dirty = true;
  }

private:
  bool dirty;
  unsigned ct;
  typename std::list<T *> typedchildren;

  void chkDirty() const {
    ChildTypeCounter<T> *self = const_cast<ChildTypeCounter<T> *>(this);
    if (self->dirty) {
      self->rebuildTypedChildList();
      self->dirty = false;      
    }
  }

  void rebuildTypedChildList() {
    ct = 0;
    typedchildren.clear();
    std::list<Component *> chlist = children();
    for (std::list<Component *>::iterator it = chlist.begin(); it != chlist.end(); ++it) {
      T * t = dynamic_cast<T *>(*it);
      if (t) {
        typedchildren.push_back(t);
        ++ct;
      }
    }    
  }

};

#endif
