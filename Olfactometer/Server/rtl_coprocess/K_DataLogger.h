#ifndef K_DataLogger_H
#define K_DataLogger_H

#include "RTFifo.h"

namespace Kernel 
{

  class DataLogger : public RTFifo
  {
  public:
    DataLogger();
    virtual ~DataLogger();
    
    void log(unsigned id, double datum, const char *meta);
    
  private:
    DataLogger(const DataLogger &) {}
    DataLogger & operator=(const DataLogger &) { return *this; }
  };
  
}

#endif
