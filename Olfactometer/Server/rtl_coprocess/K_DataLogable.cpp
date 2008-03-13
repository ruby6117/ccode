#include "K_DataLogable.h"

namespace Kernel
{

DataLogable::DataLogable(DataLogger *l, unsigned id)
  : logging_enabled_mask(0), logger(l), id(id)
{}

DataLogable::~DataLogable()
{}

/// enables data logging for the specified datatype
bool DataLogable::setLoggingEnabled(bool enabled, DataType t)
{
  if (enabled)  logging_enabled_mask |= unsigned(t);
  else logging_enabled_mask &= ~unsigned(t);
  return true;
}
/// disable data logging for the specified datatype
bool DataLogable::loggingEnabled(DataType t) const
{
  return logging_enabled_mask & unsigned(t);
}

/// log a data point for the specified datatype -- a data point is a float -- if logging is disabled for that datatype nothing happens
bool DataLogable::logDatum(double datum, DataType t, const char *meta)
{
  if (logger && logging_enabled_mask & unsigned(t)) {
    if (!meta) {
      switch(t) {
      case Cooked: meta = "Cooked"; break;
      case Raw: meta = "Raw"; break;
      default: meta = "Other"; break;
      }
    }
    logger->log(id, datum, meta);
    return true;
  }
  return false;
}

}
