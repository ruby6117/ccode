#ifndef K_DataLogable_H
#define K_DataLogable_H

#include "../DataLogable.h"
#include "K_DataLogger.h"

namespace Kernel {

class DataLogable : public ::DataLogable
{
public:
  DataLogable(DataLogger *logger, unsigned id);
  virtual ~DataLogable();

  /// enables data logging for the specified datatype
  bool setLoggingEnabled(bool enable, DataType t = Cooked);
  /// disable data logging for the specified datatype
  bool loggingEnabled(DataType t = Cooked) const;
  /// log a data point for the specified datatype -- a data point is a float -- if logging is disabled for that datatype nothing happens
  bool logDatum(double datum, DataType = Cooked, const char *meta = 0);
  
private:
  unsigned logging_enabled_mask;
  DataLogger *logger;
  unsigned id;
  DataLogable() {}
  DataLogable(const DataLogable &) {}  
  DataLogable & operator=(const DataLogable &) { return *this; }
};

}

#endif
