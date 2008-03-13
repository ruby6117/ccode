#include "DataLogableProxy.h"


bool DataLogableProxy::setLoggingEnabled(bool enable, DataType t)
{
  return coprocess->setLogging(handle, enable, t);
}

bool DataLogableProxy::loggingEnabled(DataType t) const
{
  return coprocess->getLogging(handle, t);
}
