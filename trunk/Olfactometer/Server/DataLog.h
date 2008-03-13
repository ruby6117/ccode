#ifndef DataLog_H
#define DataLog_H

#include <list>
#include "rtl_coprocess/DataEvent.h"

class DataLog
{
public:  
  virtual ~DataLog() {} /**< Shut the compiler up.. */
  virtual unsigned numEvents() const = 0;
  virtual unsigned getEvents(std::list<DataEvent> & evts_out, unsigned start, unsigned num, bool erase_retreived_events = false)  = 0;
  virtual void clearEvents() = 0;
protected:
  DataLog() {}  
  DataLog(const DataLog &) {}
  DataLog &operator=(const DataLog &) { return *this; }
};

#endif
