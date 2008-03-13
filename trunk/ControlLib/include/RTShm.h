#ifndef RTShm_H
#define RTShm_H

#include "SysDep.h"

template <class C>
class RTShm
{
public:
  RTShm() : is_attached(false), n(0), p(0) {}
  ~RTShm() { detach(); }

  bool attach(const char *shmName) { 
    detach();
    if ((p = reinterpret_cast<C *>(ShmAttach(shmName, sizeof(C))))) {
      is_attached = true;
      n = Strdup(shmName);
    }
    return is_attached;
  }

  void detach() {
    if (!is_attached) return;
    ShmDetach(n, p);
    is_attached = false;
    Strfree(n);
    n = 0;
    p = 0;
  }

  bool isAttached() const { return is_attached; }

  operator const C *() const { return p; }
  const C * operator->() const { return p; }
  const C * operator*() const { return p; }

  operator  C *()  { return p; }
  C * operator->() { return p; }
  C * operator*()  { return p; }

private:
  bool is_attached;
  const char *n;
  C *p;
};

#endif
