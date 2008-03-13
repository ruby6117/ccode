#include "K_DataLogger.h"
#include "DataEvent.h"
#include "SysDep.h"
#include "Timer.h"
#include "Module.h"

namespace Kernel {

DataLogger::DataLogger()
  : RTFifo(sizeof(struct DataEvent) * 1024)
{}

DataLogger::~DataLogger() {}

void DataLogger::log(unsigned id, double datum, const char *meta)
{
  DataEvent e;
  e.id = id;
  Strncpy(e.meta, meta, sizeof(e.meta));
  e.meta[sizeof(e.meta)-1] = 0; // force null terminate
  Cpy(e.datum, datum);
  e.ts_ns = Timer::absTime();
  write(&e, sizeof(e));
  Debug("Datalog: %u %s\n", id, meta);
}

}
