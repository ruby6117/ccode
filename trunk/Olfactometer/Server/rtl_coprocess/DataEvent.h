#ifndef Data_Event_H
#define Data_Event_H

#include "SysDep.h"

struct DataEvent
{
  long long ts_ns;
  unsigned id;
  char meta[8];
  double datum;
#ifndef __KERNEL__
  /// for stl containers...
  bool operator<(const DataEvent &e) const
  {
    if (ts_ns == e.ts_ns) return Memcmp(this, &e, sizeof(e)) < 0;
    else return ts_ns < e.ts_ns;
  }
#endif
};


#endif
