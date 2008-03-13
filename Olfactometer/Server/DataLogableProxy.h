#ifndef DataLogableProxy_H
#define DataLogableProxy_H

#include "RTProxy.h"
#include "DataLogable.h"
#include "RTLCoprocess.h"

class DataLogableProxy : public DataLogable, virtual public RTProxy
{
public:
  /// enables data logging for the specified datatype
  bool setLoggingEnabled(bool enable, DataType t = Cooked);
  /// disable data logging for the specified datatype
  bool loggingEnabled(DataType t = Cooked) const;  
protected:
  DataLogableProxy() {};  
};

#endif
