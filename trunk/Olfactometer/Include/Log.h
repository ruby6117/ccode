#ifndef Log_H
#define Log_H

#include <sstream>
#include <string>
#include "Common.h"

/** A class for logging from multiple threads concurrently. 
    Usage example:
    Log() << "My message: " << "blah blah " << 25 << " etc..";
    
    The log will be created and the data will be appended to an in-memory
    buffer until the Log destructor is called, at which point it will
    be synchronously (using a process-wide pthread_mutex_t) appended to 
    the system log. */
class Log 
{
public:
  enum Type { Emergency = 0, Alert, Crit, Err, Warn, Notice, Info, Debug };
    
  Log(Type t = Info);
  virtual ~Log(); ///< actually locks the syslog and prints to it, then unlocks it
  template <typename T> Log & operator<<(const T & thing) 
  { if (dontprint) return *this; ss << thing; return *this; }

  // returnes the last nEntries log entries.. note that there is a limit to
  // how many entries this function returns depending on how big the internal
  // history buffer is
  static unsigned tail(StringList & list_out, unsigned nEntries);

  typedef void (*NotifyFunc_t)(void *, const String &);
  static void addNotifyFunc(NotifyFunc_t, void *arg);
  static void removeNotifyFunc(NotifyFunc_t);

  static const unsigned MaxHistory = 10000;

protected:
  bool dontprint;
  Log(void *dummy) { (void)dummy; dontprint = true; }
private:
  Log(const Log &l) {(void)l;} // no copy c'tor
  Log & operator=(const Log & l) { (void)l; return *this; } // no operator=
  std::stringstream ss;  
  Type t;
  struct P;
  static P p;
};

class Error : public Log
{
public:
  Error() : Log(Log::Err) {}
};

class Critical : public Log
{
public:
  Critical() : Log(Log::Crit) {}
};

class Warning : public Log
{
public:
  Warning() : Log(Log::Warn) {}
};

class Perror : public Critical
{
public:
  Perror(const std::string & perror_param);
  ~Perror();
private: 
  int err_no;
};

class Debug : public Log
{
public:

  static bool enabled; /* defaults to false, set this to true to enable
                          debug printing */

#ifdef NDEBUG
  Debug() : Log(static_cast<void *>(0)) {}
#else
  Debug() : Log() { if (!enabled) dontprint = true; (*this) << "DEBUG: ";}
#endif
};

#endif
