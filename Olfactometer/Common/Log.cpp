#include "Common.h"
#include "Log.h"
#include <pthread.h>
#include <syslog.h>
#include <string>
#include <errno.h>
#include <string.h>
#include <map>

struct Log::P
{
  P();
  ~P();
  void lock();
  void unlock();
  void openlog();
  void closelog();

  pthread_mutex_t mut;

  StringList logHistory;
  unsigned currSz;
  typedef std::map<Log::NotifyFunc_t, void *> NotifyMap;
  NotifyMap notifyMap;
  volatile bool deleted;
};

Log::P::P()
  : currSz(0)
{  
  pthread_mutex_init(&mut, 0); 
  openlog();
  deleted = false;
}
Log::P::~P() {  closelog(); deleted = true; }
void Log::P::lock() { pthread_mutex_lock(&mut); }
void Log::P::unlock() { pthread_mutex_unlock(&mut); }
void Log::P::openlog()
{
  ::openlog(PRG_NAME, /*LOG_CONS|*/LOG_NDELAY|LOG_PID|LOG_PERROR, LOG_USER);
}

void Log::P::closelog()
{
  ::closelog();
}

// the static 'p' member of Log  
Log::P Log::p;

Log::Log(Type ty) : dontprint(false), t(ty) {}

Log::~Log()
{
  if (p.deleted || dontprint) return;
  p.lock();

  String entry = ss.str().c_str();

  // output the log entry to the syslog
  ::syslog(t|LOG_USER, "%s", static_cast<const char *>(entry));

  // now push the log entry to the history
  p.currSz++;
  p.logHistory.push_back( entry.trimWS() );
  if (p.currSz > MaxHistory) {
    p.currSz--;
    p.logHistory.pop_front();
  }
  P::NotifyMap funcs = p.notifyMap;
  p.unlock();

  // now notify interested functions via callbacks
  for (P::NotifyMap::iterator it = funcs.begin(); it != funcs.end(); ++it)
    if (it->first)
      (it->first)(it->second, entry); // call the function
}

void Log::addNotifyFunc(NotifyFunc_t f, void *arg)
{
  p.lock();
  p.notifyMap[ f ] = arg;
  p.unlock();
}

void Log::removeNotifyFunc(NotifyFunc_t f)
{
  p.lock();
  p.notifyMap.erase(f);
  p.unlock();
}

Perror::Perror(const std::string & prefix)
{
  err_no = errno; // save errno
  (*this) << prefix << ": ";
}

Perror::~Perror()
{
  (*this) << ::strerror(err_no);
}

//static
unsigned Log::tail(StringList & lout, unsigned num)
{
  lout.clear();
  unsigned i;
  p.lock();
  StringList::reverse_iterator it = p.logHistory.rbegin();
  for (i = 0; i < num && it != p.logHistory.rend(); ++i, ++it)
    lout.push_front(*it);
  p.unlock();
  return i;
}

bool Debug::enabled = false;
