#include "ConnThread.h"
#include "Protocol.h"
#include "Log.h"
#include "Common.h"
#include "Olfactometer.h"
#include "ComediOlfactometer.h"
#include "PIDFlowController.h"
#include "StartStoppable.h"
#include "Controller.h"
#include "Saveable.h"
#include "DataLog.h"
#include "Calib.h"
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <netinet/tcp.h> 
#include <netdb.h>
extern int h_errno;
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <strings.h>

#include <comedilib.h>

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <set>

#define ABS(x) ( (x) < 0 ? -(x) : (x) )

#define DEFAULT_CONN_TIMEOUT (60*60*1000) // 1 hour timeout for conns by default? note this is overridden by config file?
#define MAX_LINE_LEN 65535

pthread_mutex_t ConnThread::mut = PTHREAD_MUTEX_INITIALIZER;
ConnThread::ThreadMap ConnThread::threads; // all the threads
#define LOGPREFIX ( std::string("(") + remoteHost + ") " )
#define LOG() (::Log() << LOGPREFIX)
#define PERROR(x) (::Perror(LOGPREFIX + x))
#define ERROR() (::Error() << LOGPREFIX)
#define WARNING() (::Warning() << LOGPREFIX)
#define CRITICAL() (::Critical() << LOGPREFIX)

ConnThread::ConnThread(int s, const std::string & rh, Olfactometer & theOlf, int t_out)
  : sock(s), remoteHost(rh), olf(theOlf), timeout_ms(DEFAULT_CONN_TIMEOUT)
{
  if (t_out) setTimeout(t_out);

  lineBuf = new char[MAX_LINE_LEN+1];
  ::memset(lineBuf, 0, MAX_LINE_LEN+1);
  sz = 0;

  startTime = GetTime();
  pthread_mutex_lock(&mut);
  if ( pthread_create(&thr, 0, ThreadFuncWrapper, (void *)this) ) {
    CRITICAL() << "Error creating connection thread!\n";
    pthread_mutex_unlock(&mut);
    ::exit(1);
  }
  threads[thr] = this;
  pthread_mutex_unlock(&mut);
}

ConnThread::~ConnThread() 
{
  if (sock >= 0) ::close(sock);
  delete [] lineBuf;
}

int ConnThread::timeout() const
{
  return timeout_ms / 1000;
}

void ConnThread::setTimeout(int seconds) 
{
  // check for overflow, if so, just set to INT_MAX
  long long tmp = static_cast<long long>(seconds) * 1000LL;
  if (tmp >= static_cast<long long>(INT_MAX))
    tmp = INT_MAX;

  timeout_ms = static_cast<int>(tmp);
}

bool ConnThread::xmitBuf(const void *buf, size_t num, bool isBinary, bool doLog)
{
  ssize_t ret;

  if (doLog) {
    if (isBinary) 
      LOG() << "Sending binary data of size " << num << "\n";
    else 
      LOG() << "Sending: " << static_cast<const char *>(buf);
  }

  while (num) {
    ret = send(sock, buf, num, MSG_NOSIGNAL);

    if (ret < 0 && errno != EAGAIN) return false;
    else if (ret > 0) {
      num -= ret;
      buf = static_cast<const char *>(buf) + ret;
    } else {
      ERROR() << "Unexpected condition in TransmitBuf() or connection lost!\n";
      pthread_exit(0);
    }
  }
  return true;
}

bool ConnThread::xmit(const std::string & str)
{
  bool ret = xmitBuf(str.c_str(), str.length(), false);
  if (!ret) {
    PERROR("xmit");
    pthread_exit(0);
  }
  return ret;
}


double ConnThread::GetTime()
{ 
  struct timeval tv;
  ::gettimeofday(&tv, 0);
  return static_cast<double>(tv.tv_sec) + static_cast<double>(tv.tv_usec)/1000000.0;
}

void ConnThread::CleanupHandler(void *arg)
{
    ConnThread *thr = static_cast<ConnThread *>(arg);
    
    ::close(thr->sock);
    thr->sock = -1;
    pthread_mutex_lock(&mut);
    ThreadMap::iterator it = threads.find(thr->thr);
    if (it != threads.end()) {
      pthread_detach(thr->thr); // reap ourselves
      threads.erase(thr->thr);
      delete it->second; // if it's a valid pointer
    }
    pthread_mutex_unlock(&mut);    
}

void *ConnThread::ThreadFuncWrapper(void *arg)
{
  ConnThread *self = static_cast<ConnThread *>(arg);
  pthread_cleanup_push(CleanupHandler, arg);
  self->doIt();
  pthread_cleanup_pop(1);
  return NULL;
}

void ConnThread::doIt()
{
    LOG() << "New connection from: " << remoteHost;
  

    // turn off nagle, just to be peppy
    int tmp = 1;
    tmp = ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &tmp, sizeof(tmp));
    if (tmp) {
      PERROR("setsockopt");
      return;
    }
    
    while (1) {
      std::string theLine;
      switch( recvLine(theLine) ) {
      case RecvAgain: continue; // try again
      case RecvError: 
        Log() << "Connection to " << remoteHost << " closed connection after " << (GetTime() - startTime) << " seconds.";
        pthread_exit(0); // kills the thread, basically
        break; // not reached
      case RecvOK: 
      default:
        processCommand(theLine);
        break;
      }
    }
}

// static
const int ConnThread::ProtocolHandler::NOARGCHK = INT_MIN;

// static
const ConnThread::ProtocolHandler ConnThread::protocolHandlers[] =
  { 
    { cmd     : Protocol    :: Help,            // HELP
      nArgs   : ProtocolHandler::NOARGCHK, synopsis : "command",
      handler : &ConnThread :: doHelp },
    { cmd     : Protocol    :: Help2,            // ?
      nArgs   : ProtocolHandler::NOARGCHK, synopsis : "command",
      handler : &ConnThread :: doHelp },    
    { cmd     : Protocol    :: Noop,            // NOOP
      nArgs   : 0, synopsis : "",
      handler : &ConnThread :: doNoop },
    { cmd     : Protocol    :: Quit,            // QUIT
      nArgs   : 0, synopsis : "",
      handler : &ConnThread :: doQuit },
    { cmd     : Protocol    :: GetName,         // GET NAME
      nArgs   : 0, synopsis : "",
      handler : &ConnThread :: doGetName },
    { cmd     : Protocol    :: GetDescription,  // GET DESCRIPTION
      nArgs   : 0, synopsis : "",
      handler : &ConnThread :: doGetDescription },
    { cmd     : Protocol    :: GetVersion,      // GET VERSION
      nArgs   : 0, synopsis : "",
      handler : &ConnThread :: doGetVersion },
    { cmd     : Protocol    :: GetMixes,        // GET MIXES
      nArgs   : 0, synopsis : "",
      handler : &ConnThread :: doGetMixes },
    { cmd     : Protocol    :: GetBanks,        // GET BANKS
      nArgs   : 1, synopsis : "mixname",
      handler : &ConnThread :: doGetBanks },
    { cmd     : Protocol    :: GetNumOdors,     // GET NUM ODORS
      nArgs   : 1, synopsis : "bankname",
      handler : &ConnThread :: doGetNumOdors },
    { cmd     : Protocol    :: GetOdorTable,    // GET ODOR TABLE
      nArgs   : 1, synopsis : "bankname",
      handler : &ConnThread :: doGetOdorTable },
    { cmd     : Protocol    :: SetOdorTable,    // SET ODOR TABLE
      nArgs   : 1, synopsis : "bankname",
      handler : &ConnThread :: doSetOdorTable },
    { cmd     : Protocol    :: GetOdor,         // GET BANK ODOR
      nArgs   : 1, synopsis : "bankname",
      handler : &ConnThread :: doGetOdor },
    { cmd     : Protocol    :: SetOdor,         // SET BANK ODOR
      nArgs   : -2, synopsis : "bankname odorNum ...",
      handler : &ConnThread :: doSetOdor },
    { cmd     : Protocol    :: GetActualOdorFlow,     // GET ACTUAL ODOR FLOW
      nArgs   : 1, synopsis : "mixname", // mixname
      handler : &ConnThread :: doGetActualOdorFlow },
    { cmd     : Protocol    :: GetCommandedOdorFlow,     // GET CMD ODOR FLOW
      nArgs   : 1, synopsis : "mixname", // mixname
      handler : &ConnThread :: doGetCommandedOdorFlow },
    { cmd     : Protocol    :: SetOdorFlow,     // SET ODOR FLOW
      nArgs   : 2, synopsis : "mixname flow", // mixname flow
      handler : &ConnThread :: doSetOdorFlow },
    { cmd     : Protocol    :: GetMixtureRatio, // GET MIX RATIO
      nArgs   : 1, synopsis : "mixname", // mixname 
      handler : &ConnThread :: doGetMixtureRatio },
    { cmd     : Protocol    :: SetMixtureRatio, // SET MIX RATIO
      nArgs   : -2, synopsis : "mixname bank1name=flow1 ...", // variable 
      handler : &ConnThread :: doSetMixtureRatio },
    { cmd     : Protocol    :: GetActualBankFlow,   // GET ACTUAL BANK FLOW
      nArgs   : 1,  synopsis : "bankname",
      handler : &ConnThread :: doGetActualBankFlow },
    { cmd     : Protocol    :: GetCommandedBankFlow, // GET COMMANDED BANK FLOW
      nArgs   : 1,  synopsis : "bankname",
      handler : &ConnThread :: doGetCommandedBankFlow }, 
    { cmd     : Protocol    :: OverrideBankFlow,   // OVERRIDE BANK FLOW
      nArgs   : 2,  synopsis : "bankname flow",
      handler : &ConnThread :: doOverrideBankFlow },
    { cmd     : Protocol    :: GetActualCarrierFlow, // GET ACTUAL CARRIER FLOW
      nArgs   : 1,  synopsis : "mixname",
      handler : &ConnThread :: doGetActualCarrierFlow },
    { cmd     : Protocol    :: GetCommandedCarrierFlow, // GET COMM. CAR.. FLOW
      nArgs   : 1,  synopsis : "mixname",
      handler : &ConnThread :: doGetCommandedCarrierFlow }, 
    { cmd     : Protocol    :: OverrideCarrierFlow,   // OVERRIDE CARRIER FLOW
      nArgs   : 2,  synopsis : "mixname flow",
      handler : &ConnThread :: doOverrideCarrierFlow },
    { cmd     : Protocol    :: GetCommandedFlow, // GET COMMANDED FLOW
      nArgs   : 1,  synopsis : "mixname|bankname|flow_controller",
      handler : &ConnThread :: doGetCommandedFlow }, 
    { cmd     : Protocol    :: GetActualFlow, // GET ACTUAL FLOW
      nArgs   : 1,  synopsis : "mixname|bankname|flow_controller",
      handler : &ConnThread :: doGetActualFlow }, 
    { cmd     : Protocol    :: OverrideFlow,   // OVERRIDE FLOW
      nArgs   : 2,  synopsis : "mixname|bankname|flow_controller flow",
      handler : &ConnThread :: doOverrideFlow },
    { cmd     : Protocol    :: Enable,   // ENABLE 
      nArgs   : 1,  synopsis : "enableable_comp",
      handler : &ConnThread :: doEnable },
    { cmd     : Protocol    :: Disable, // DISABLE 
      nArgs   : 1,  synopsis : "enableable_comp",
      handler : &ConnThread :: doDisable },
    { cmd     : Protocol    :: IsEnabled, // IS ENABLED
      nArgs   : 1,  synopsis : "enableable_comp",
      handler : &ConnThread :: doIsEnabled },
    { cmd     : Protocol    :: Monitor, // MONITOR
      nArgs   : 3,  synopsis : "rate_hz bankname|mixname actualflow|commandedflow|actualcarrierflow|commandedcarrierflow|actualodorflow|commandedodorflow|enabled|odor|odortable",
      handler : &ConnThread :: doMonitor },
    { cmd     : Protocol    :: SetDesiredTotalFlow, // SET DESIRED TOTAL FLOW
      nArgs   : 2,  synopsis : "mixname flow_in_ml_min",
      handler : &ConnThread :: doSetDesiredTotalFlow },
    { cmd     : Protocol    :: List, // LIST
      nArgs   : ProtocolHandler::NOARGCHK,  synopsis : "[logables|startstoppables|controllables|readables|writables|saveables|calib|enableables]",
      handler : &ConnThread :: doList },
    { cmd     : Protocol    :: GetControlParams, // GET CONTROL PARAMS
      nArgs   : 1,  synopsis : "controllable_obj",
      handler : &ConnThread :: doGetControlParams },
    { cmd     : Protocol    :: SetControlParams, // GET CONTROL PARAMS
      nArgs   : -2,  synopsis : "controllable_obj args...",
      handler : &ConnThread :: doSetControlParams },
    { cmd     : Protocol    :: GetCoeffs, // GET COEFFS
      nArgs   : 1,  synopsis : "pidflowcontroller",
      handler : &ConnThread :: doGetCoeffs },
    { cmd     : Protocol    :: SetCoeffs, // SET COEFFS
      nArgs   : 5,  synopsis : "pidflowcontroller a b c d",
      handler : &ConnThread :: doSetCoeffs },
    { cmd     : Protocol    :: GetCalib, // GET CALIB
      nArgs   : 1,  synopsis : "pidflowcontroller",
      handler : &ConnThread :: doGetCalib },
    { cmd     : Protocol    :: SetCalib, // SET CALIB
      nArgs   : 1,  synopsis : "pidflowcontroller (requires input of voltage_sensor=flow_ml_min, one per line)",
      handler : &ConnThread :: doSetCalib },
    { cmd     : Protocol    :: Start, // START
      nArgs   : 1,  synopsis : "startable_object",
      handler : &ConnThread :: doStart },
    { cmd     : Protocol    :: Stop, // STOP
      nArgs   : 1,  synopsis : "stopable_object",
      handler : &ConnThread :: doStop },
    { cmd     : Protocol    :: Running, // RUNNING
      nArgs   : 1,  synopsis : "startable_object",
      handler : &ConnThread :: doRunning },
    { cmd     : Protocol    :: Read, // READ
      nArgs   : 1,  synopsis : "actuator_or_sensor_or_component_containing_one",
      handler : &ConnThread :: doRead },
    { cmd     : Protocol    :: ReadRaw, // RREAD
      nArgs   : 1,  synopsis : "actuator_or_sensor_or_component_containing_one",
      handler : &ConnThread :: doReadRaw },
    { cmd     : Protocol    :: Write, // WRITE
      nArgs   : 2,  synopsis : "actuator_or_component_containing_one value",
      handler : &ConnThread :: doWrite },
    { cmd     : Protocol    :: WriteRaw, // RWRITE
      nArgs   : 2,  synopsis : "actuator_or_component_containing_one value",
      handler : &ConnThread :: doWriteRaw },
    { cmd     : Protocol    :: Save, // SAVE
      nArgs   : 1,  synopsis : "saveable_component",
      handler : &ConnThread :: doSave },
    { cmd     : Protocol    :: IsDataLogging, // IS DATA LOGGING
      nArgs   : 2,  synopsis : "logable_component [cooked|raw|other|any]",
      handler : &ConnThread :: doIsDataLogging },
    { cmd     : Protocol    :: SetDataLogging, // SET DATA LOGGING
      nArgs   : 3,  synopsis : "logable_component [cooked|raw|other|all] bool_flg",
      handler : &ConnThread :: doSetDataLogging },
    { cmd     : Protocol    :: DataLogCount, // DATA LOG COUNT
      nArgs   : 0,  synopsis : "(no args)",
      handler : &ConnThread :: doDataLogCount },
    { cmd     : Protocol    :: GetDataLog, // GET DATA LOG
      nArgs   : -2,  synopsis : "first count [bool_clear_gotten_entries]",
      handler : &ConnThread :: doGetDataLog },
    { cmd     : Protocol    :: ClearDataLog, // CLEAR DATA LOG
      nArgs   : 0,  synopsis : "(no args)",
      handler : &ConnThread :: doClearDataLog },    
   
    // To tell algorithms static array ended.
    { cmd : 0, nArgs : 0, synopsis : 0,  handler : 0 }  
  };

//static 
const ConnThread::ProtocolHandler *
ConnThread::findProtocolHandler(const String &cmd)
{
  String lineUpper = cmd;
  std::transform(lineUpper.begin(), lineUpper.end(), lineUpper.begin(), toupper);
  
  // iterate through list of protocol commands 
  for (int i = 0; protocolHandlers[i].cmd; ++i) {
    const ProtocolHandler & p = protocolHandlers[i];
    if (lineUpper.find(p.cmd) == 0) return &p;
  }
  return 0;
}

void ConnThread::processCommand(const std::string & line_in)
{
  String line (line_in);
  LOG() << "Got Line: " << line;

  line.trimWS(); // trim trailing/leading whitespace

  const ProtocolHandler *p = findProtocolHandler(line);

  if (!p) {
    // invalid command was given
    ERROR() << "Parse error for line\n";
    Protocol::SendError(sock, "Invalid protocol command.");
    return;
  }

  String args = line.substr(::strlen(p->cmd)); // strip protocol command
  StringList argv = args.split("\\s+");
  bool wrongNumberOfArgs = false;
  if (p->nArgs != ProtocolHandler::NOARGCHK) {
    // chk arg count
    if (p->nArgs < 0 && static_cast<int>(argv.size()) < ABS(p->nArgs))
      wrongNumberOfArgs = true;
    else if (p->nArgs >= 0 && static_cast<int>(argv.size()) != p->nArgs)
      wrongNumberOfArgs = true;
  }
  if ( wrongNumberOfArgs ) { // check number of args..
    if (!p->nArgs || !p->synopsis)
      Protocol::SendError(sock, String("Wrong number of arguments -- ") + p->cmd + " requires exactly " + p->nArgs + " arguments.");
    else
      Protocol::SendError(sock, String("Argument/usage error --  synopsis: ") + p->cmd + " " + p->synopsis);
    return;
  }
  if ( (this->*p->handler)(argv) ) { // call pointer to member on 'this'
    // if protocol handler returned true, send ok otherwise descriptive
    // error was sent by protocol command
    Protocol::SendOK(sock);
  }
}

ConnThread::RecvStatus ConnThread::recvLine(std::string & line_out)
{
    char *line = lineBuf;
    char * newlinePos = 0;

    // keep reading until we have a newline, we fill the buffer, or we
    // get a socket error
    while (!(newlinePos = ::strstr(line, "\n")))  {
      PollStatus ps = poll();
      if (ps == PollAgain) continue;
      else if (ps != PollOK) return RecvError;
  
      int tmp = ::recv(sock, line + sz, MAX_LINE_LEN - sz, MSG_DONTWAIT);

      if (tmp < 0) {
        if (errno == EAGAIN || errno == EINTR)
          return RecvAgain;
        else {
          PERROR("read");
          return RecvError;
        }
      } else if (tmp == 0) {
        if (sz == MAX_LINE_LEN) {
          // out of buffer space!
          WARNING() << "Out of buffer space reading line!\n";
          newlinePos = (lineBuf + MAX_LINE_LEN) - 1;
          break;
        } else {
          LOG() << "Client closed connection...\n";
          return RecvError;
        }
      }
      // at this point we know we got data, resize the string
      sz += tmp;
    }
    if (newlinePos - line > MAX_LINE_LEN || sz > MAX_LINE_LEN) {
      // This should never happen but i am tired now and
      // i just want to program defensively..
      CRITICAL() << "INTERNAL ERROR - ACK!  Buffer overflow in " << __FILE__;
      line_out = "";
      return RecvError;
    } else {
      *newlinePos = 0;
      lineBuf[sz] = 0;
      // try and grab the line if we have it now..
      line_out = lineBuf; // assign it to output
      ::memmove(lineBuf, newlinePos+1, MAX_LINE_LEN - sz);
      sz -= line_out.length()+1;
    }
    return RecvOK;
}

ConnThread::PollStatus ConnThread::poll() const
{
      struct pollfd pfd = { fd: sock, events: POLLIN|POLLPRI, revents: 0 };

      pthread_testcancel();

      int tmp = ::poll(&pfd, 1, timeout_ms);

      if (tmp < 0) {
        if (errno == EINTR) return PollAgain; // try again?
        // error
        PERROR("poll");
        return PollError;
      } else if (tmp == 0) {
        // timeout?
        return PollTimedOut;
        //pthread_exit(0);
      } // else ...
      return PollOk;
}


void ConnThread::CancelAll()
{
  pthread_mutex_lock(&mut);
  for (ThreadMap::iterator it = threads.begin(); it != threads.end(); ++it) {
    pthread_cancel(it->first);
    ::close(it->second->sock); it->second->sock = -1;        
    //pthread_join(it->first, NULL);
  }  
  pthread_mutex_unlock(&mut);
}

bool ConnThread::doGetName(StringList &args_ignored)
{
    (void) args_ignored;
    xmit(olf.name() + "\n");
    return true;
}

bool ConnThread::doGetDescription(StringList &args_ignored)
{
    (void) args_ignored;
    xmit(olf.description() + "\n");
    return true;
}

bool ConnThread::doGetVersion(StringList &args_ignored)
{
    (void) args_ignored;
    std::ostringstream ss;
    ss << ((PRG_VERSION>>16)&0xf) << "." << ((PRG_VERSION>>8)&0xf) << "." << (PRG_VERSION&0xf) << "\n";   
    xmit(ss.str());
    return true;
}

bool ConnThread::doGetMixes(StringList &args_ignored)
{
    (void) args_ignored;
    std::ostringstream ss;
    olf.lock();
    std::list<Mix *> mixes = olf.mixes();
    for (std::list<Mix *>::iterator it = mixes.begin(); it != mixes.end(); ++it) {
      if (it != mixes.begin()) ss << " ";
      ss << (*it)->name();
    }
    ss << "\n"; // don't forget the newline!!
    olf.unlock();
    xmit(ss.str());
    return true;
}

bool ConnThread::doGetBanks(StringList &argv)
{
    String mixname = argv.front();
    olf.lock();
    Mix * m = olf.mix(mixname);
    if (!m) {
      olf.unlock();
      Protocol::SendError(sock, (mixname + " not found.").c_str());
      return false;      
    }
    std::list<std::string> banks = m->bankNames();  
    olf.unlock();
    std::ostringstream oss;
    for (std::list<std::string>::iterator it = banks.begin(); it != banks.end(); ++it) {
      if (it != banks.begin()) oss << " ";
      oss << *it;
    }
    oss << "\n"; // don't forget the newline!!
    xmit(oss.str());
    return true;
}

bool ConnThread::doGetNumOdors(StringList &argv)
{
    String bankname = argv.front();
    olf.lock();
    Bank *b = 0;
    if ( !(b = dynamic_cast<Bank *>(olf.find(bankname))) ) {
      olf.unlock();
      Protocol::SendError(sock, (bankname + " not found.").c_str());
      return false;            
    }
    unsigned numOdors = b->numOdors();
    olf.unlock();
    std::ostringstream oss;
    oss << numOdors << "\n";
    //oss << "\n"; // don't forget the newline!!
    xmit(oss.str());
    return true;
}

bool ConnThread::doGetOdorTable(StringList &argv)
{
    String bankname = argv.front();
    olf.lock();
    Bank *b = 0;
    if ( !(b = dynamic_cast<Bank *>(olf.find(bankname))) ) {
      olf.unlock();
      Protocol::SendError(sock, (bankname + " not found.").c_str());
      return false;            
    }
    String output = dumpOdorTable(b);
    olf.unlock();
    xmit(output + "\n");
    return true;
}

bool ConnThread::doSetOdorTable(StringList &argv)
{
    String bankname = argv.front();
    olf.lock();
    Bank *b = 0;
    if ( !(b = dynamic_cast<Bank *>(olf.find(bankname))) ) {
      olf.unlock();
      Protocol::SendError(sock, (bankname + " not found.").c_str());
      return false;            
    }
    Bank::OdorTable odorTable = b->odorTable();
    unsigned numOdors = b->numOdors();
    olf.unlock();
    Protocol::SendReady(sock);
    std::set<int> seen;
    for (unsigned i = 0; i < numOdors; ++i) {
      std::string line;
      int r = recvLine(line); // ignoring errors, hopefully that's ok
      if (r != RecvOK)  return false;
      StringList fields = String::split(line, "\\s+");
      if (fields.size() < 2) {
        Protocol::SendError(sock, "Not enough fields in line, need at least 2 fields per odor table line!");
        return false;        
      }      
      Odor o;
      int num = -1;
      bool ok;
      num = String::toInt(fields.front(), &ok);
      if (!ok) {
        Protocol::SendError(sock, (fields.front() + " is an invalid odor id!").c_str());
        return false;        
      }
      fields.pop_front();
      o.name = fields.front();
      fields.pop_front();
      o.metadata = String::join(fields, " ");
      if (num < 0 || num >= (int)numOdors) {
        Protocol::SendError(sock, (Str(num) + " is an invalid odor id!").c_str());
        return false;
      } else if (seen.count(num)) {
        Protocol::SendError(sock, (Str(num) + " is a duplicate odor id!").c_str());
        return false;
      }
      odorTable[num] = o;
      seen.insert(num);
    }
    if (seen.size() != numOdors) {
      Protocol::SendError(sock, (std::string("Need to send exactly ") + Str(numOdors) + " odors for odor table!").c_str());
      return false;
    }
    olf.lock();
    b->setOdorTable(odorTable);
    olf.unlock();

    return true;
}

bool ConnThread::doGetOdor(StringList &argv)
{
  String bankname = argv.front();
  olf.lock();
  Bank *b = 0;
  if ( !(b = dynamic_cast<Bank *>(olf.find(bankname))) ) {
    olf.unlock();
    Protocol::SendError(sock, (bankname + " not found.").c_str());
    return false;            
  }
  unsigned odor = b->currentOdor();
  olf.unlock();
  xmit(Str(odor) + "\n");
  return true;
}

bool ConnThread::doSetOdor(StringList &argv)
{
  std::map<Bank *, unsigned> oldOdorSettings;
  bool undoIt = false;
  String errStr;

  if (argv.size() % 2) {
    Protocol::SendError(sock, "Wrong number of arguments -- need even number of arguments.");
    return false;
  }
  
  olf.lock();
  // loop through bank odornum pairs and set the odor for each bank.. note that if we get some error at some point we will undo the damage done..
  while (argv.size() && !undoIt) 
  { 
      String bankname = argv.front(); argv.pop_front();
      String odorStr = argv.front(); argv.pop_front();    
      bool ok = false;
      unsigned odor = odorStr.toInt(&ok), oldOdor = 0;
      
      Bank *b = 0;
      if ( !(b = dynamic_cast<Bank *>(olf.find(bankname))) ) {
        errStr = bankname + " not found.";
        undoIt = true;
        continue;
      }
      oldOdor = b->currentOdor();
      if (!ok || !b->setOdor(odor)) {
        undoIt = true;
        errStr = odorStr + " invalid odor or odor set error.";
        continue;
      }
      oldOdorSettings[b] = oldOdor;
  }
  if (undoIt) {
    for (std::map<Bank *, unsigned>::iterator it = oldOdorSettings.begin();
         it != oldOdorSettings.end();
         ++it)    
                it->first->setOdor(it->second);
  }
  olf.unlock();
  if (undoIt) {
    Protocol::SendError(sock, errStr.c_str());
    return false;            
  }
  return true;
}

bool ConnThread::doNoop(StringList &ignored)
{
  (void)ignored;
  return true;
}

bool ConnThread::doQuit(StringList &ignored)
{
  (void)ignored;
  pthread_exit(0);
  // not reached
  Protocol::SendError(sock, "Huh?  Impossible!  pthread_exit() failed!  What is the world coming to?!");
  return false;
}

/// similar to above, takes 1 args mixname and returns a double (flow ml/min)
bool ConnThread::doGetActualOdorFlow(StringList &argv) 
{
  String mixname = argv.front();
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  double flow = m->actualOdorFlow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;  
}
/// similar to above, takes 1 args mixname and returns a double (flow ml/min)
bool ConnThread::doGetCommandedOdorFlow(StringList &argv) 
{
  String mixname = argv.front();
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  double flow = m->commandedOdorFlow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;  
}

/// similar to above, takes 2 args bankname and double (flow in ml/min)
bool ConnThread::doSetOdorFlow(StringList &argv) 
{
  String mixname = argv.front();
  argv.pop_front();
  bool ok;
  double flow = argv.front().toDouble(&ok);
  if (!ok) {
    Protocol::SendError(sock, String("Parse error on '") + flow + "' argument must be a double!");
    return false;
  }
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  ok = m->setOdorFlow(flow);
  olf.unlock();
  if (!ok) {
    Protocol::SendError(sock, "Command failed -- is flow out of range?");
    return false;
  }
  return true;  
}

/// similar to above, takes 1 arg, a mixname, returns 1 line for each bank
bool ConnThread::doGetMixtureRatio(StringList &argv) 
{
  String mixname = argv.front();
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  Mix::MixtureRatio mr = m->mixtureRatios();
  olf.unlock();
  for (Mix::MixtureRatio::iterator it = mr.begin(); it != mr.end(); ++it)
    xmit(it->first + " " + Str(it->second) + "\n");
  return true;
}

/// takes a mixname, plus BankName=ratio, 1 per bank all on 1 line
bool ConnThread::doSetMixtureRatio(StringList &argv) 
{
  if (!argv.size()) {
    Protocol::SendError(sock, "Command requires arguments: Mixname bankname1=ratio1 ...");
    return false;
  }
  String mixname = argv.front();
  argv.pop_front();
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  if (argv.size() != m->numBanks()) {
    unsigned nb = m->numBanks();
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " has " + nb + " banks but only " + argv.size() + " mix ratios were specified.");
    return false;
  }
  Mix::MixtureRatio mr = m->mixtureRatios();
  olf.unlock();
  int i = 2;
  for (StringList::iterator it = argv.begin(); it != argv.end(); ++it, ++i) {
    StringList nv = it->split("=");
    if (nv.size() != 2) {
      Protocol::SendError(sock, String("Parse error on mix ratio argument ") + i + ".");
      return false;
    }
    String & name = nv.front(), & value = nv.back();
    if (mr.find(name) == mr.end()) {
      Protocol::SendError(sock, String("Unknown bank: ") + name + ".");
      return false;      
    }
    bool ok;
    double ratio = value.toDouble(&ok);
    if (!ok) {
      Protocol::SendError(sock, String("Parse error on ratio value: ") + value + ".");
      return false;            
    }
    mr[name] = ratio;
  }
  olf.lock();
  bool ok = m->setMixtureRatios(mr);
  olf.unlock();
  if (!ok) {
    Protocol::SendError(sock, mixname + " returned false when setting mixture ratios.");
    return false;
  }
  return true;
}

bool ConnThread::doHelp(StringList &argv)
{
  if (!argv.empty()) {
    String argStr = String::join(argv, " ");
    const ProtocolHandler *p = findProtocolHandler(argStr);
    if (p) {
      xmit("usage:\n");
      xmit(String(p->cmd) + " " + p->synopsis + "\n");
      return true;
    }
  }
  xmit("*** Try HELP <command> for specific help.\nCommand list:\n");
  for(int i = 0; protocolHandlers[i].cmd; ++i)
    xmit(String(protocolHandlers[i].cmd) + "\n");
  return true;
}

bool ConnThread::doGetActualBankFlow(StringList &argv)
{
  String bankname = argv.front();
  olf.lock();
  Bank *b = dynamic_cast<Bank *>(olf.find(bankname));
  if (!b) {
    olf.unlock();
    Protocol::SendError(sock, String("Bank ") + bankname + " not found.");
    return false;
  }
  double flow = b->actualFlow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;
}

bool ConnThread::doGetCommandedBankFlow(StringList &argv)
{
  String bankname = argv.front();
  olf.lock();
  Bank *b = dynamic_cast<Bank *>(olf.find(bankname));
  if (!b) {
    olf.unlock();
    Protocol::SendError(sock, String("Bank ") + bankname + " not found.");
    return false;
  }
  double flow = b->commandedFlow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;
}

bool ConnThread::doOverrideBankFlow(StringList &argv)
{
  String bankname = argv.front();
  argv.pop_front();
  String flowstr = argv.front();
  bool ok;
  double flow = flowstr.toDouble(&ok);
  if (!ok) {
    Protocol::SendError(sock, flowstr + " is not a valid real number.");
    return false;    
  }  
  olf.lock();
  Bank *b = dynamic_cast<Bank *>(olf.find(bankname));
  if (!b) {
    olf.unlock();
    Protocol::SendError(sock, String("Bank ") + bankname + " not found.");
    return false;
  }
  ok = b->setFlow(flow);
  olf.unlock();
  if (!ok) {
    Protocol::SendError(sock, "Set flow operation failed -- flow might be out of range.");
    return false;
  }
  return true;
}

bool ConnThread::doGetActualCarrierFlow(StringList &argv)
{
  String mixname = argv.front();
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  FlowController *c;
  if (!(c = m->getCarrier())) {
    olf.unlock();
    Protocol::SendError(sock, String("Internal error!  Mix ") + mixname + " has no carrier defined!");
    return false;
  }
  double flow = c->flow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;
}

bool ConnThread::doGetCommandedCarrierFlow(StringList &argv)
{
  String mixname = argv.front();
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  FlowController *c;
  if (!(c = m->getCarrier())) {
    olf.unlock();
    Protocol::SendError(sock, String("Internal error!  Mix ") + mixname + " has no carrier defined!");
    return false;
  }
  double flow = c->commandedFlow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;
}

bool ConnThread::doOverrideCarrierFlow(StringList &argv)
{
  String mixname = argv.front();
  argv.pop_front();
  String flowstr = argv.front();
  bool ok;
  double flow = flowstr.toDouble(&ok);
  if (!ok) {
    Protocol::SendError(sock, flowstr + " is not a valid real number.");
    return false;    
  }  
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  FlowController *c;
  if (!(c = m->getCarrier())) {
    olf.unlock();
    Protocol::SendError(sock, String("Internal error!  Mix ") + mixname + " has no carrier defined!");
    return false;
  }
  ok = c->setFlow(flow);
  olf.unlock();
  if (!ok) {
    Protocol::SendError(sock, "Set flow operation failed -- flow might be out of range.");
    return false;
  }
  return true;
}

bool ConnThread::doGetCommandedFlow(StringList &argv)
{
  String name = argv.front();
  olf.lock();
  Bank *b = dynamic_cast<Bank *>(olf.find(name));
  Mix *m = dynamic_cast<Mix *>(olf.find(name));
  FlowController *c = dynamic_cast<FlowController *>(olf.find(name));  
  if (!m && !b && !c) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix/Bank/Flow ") + name + " not found.");
    return false;
  }
  if (!c && m) c = m->getCarrier();
  else if (!c && b) c = b->getFlowController();
  if (!c) {
    olf.unlock();
    Protocol::SendError(sock, String("Internal error!  Mix or bank ") + name + " has no flow controller defined!");
    return false;
  }
  double flow = c->commandedFlow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;
}

bool ConnThread::doGetActualFlow(StringList &argv)
{
  String name = argv.front();
  olf.lock();
  Bank *b = dynamic_cast<Bank *>(olf.find(name));
  Mix *m = dynamic_cast<Mix *>(olf.find(name));
  FlowMeter *fm = dynamic_cast<FlowMeter *>(olf.find(name));
  if (!m && !b && !fm) {
    olf.unlock();
    Protocol::SendError(sock, String("Component ") + name + " not found or is not of the right type.");
    return false;
  }
  if (!fm && m) fm = m->getCarrier();
  else if (!fm && b) fm = b->getFlowController();
  if (!fm) {
    olf.unlock();
    Protocol::SendError(sock, String("Internal error!  Mix or bank ") + name + " has no flow controller defined!");
    return false;
  }
  double flow = fm->flow();
  olf.unlock();
  xmit(Str(flow) + "\n");
  return true;
}

bool ConnThread::doOverrideFlow(StringList &argv)
{
  String name = argv.front();
  argv.pop_front();
  String flowstr = argv.front();
  bool ok;
  double flow = flowstr.toDouble(&ok);
  if (!ok) {
    Protocol::SendError(sock, flowstr + " is not a valid real number.");
    return false;    
  }  
  olf.lock();
  Bank *b = dynamic_cast<Bank *>(olf.find(name));
  Mix *m = dynamic_cast<Mix *>(olf.find(name));
  FlowController *c = dynamic_cast<FlowController *>(olf.find(name));  
  if (!m && !b && !c) {
    olf.unlock();
    Protocol::SendError(sock, String("Component ") + name + " not found.");
    return false;
  }
  if (!c && m) c = m->getCarrier();
  else if (!c && b) c = b->getFlowController();
  if (!c) {
    olf.unlock();
    Protocol::SendError(sock, String("Internal error!  Mix or bank ") + name + " has no flow controller defined!");
    return false;
  }
  ok = c->setFlow(flow);
  olf.unlock();
  if (!ok) {
    Protocol::SendError(sock, "Set flow operation failed -- flow might be out of range.");
    return false;
  }
  return true;
}


bool ConnThread::doEnable(StringList &argv)
{
  String name = argv.front();
  olf.lock();
  
  Enableable *e = dynamic_cast<Enableable *>(olf.find(name));
  if ( !e ) {
    olf.unlock();
    Protocol::SendError(sock, String("Component ") + name + " is not 'enableable' or does not exist.");
    return false;
  }
  e->setEnabled(true);
  olf.unlock();
  return true;
}

bool ConnThread::doDisable(StringList &argv)
{
  String name = argv.front();
  olf.lock();
  
  Enableable *e = dynamic_cast<Enableable *>(olf.find(name));
  if ( !e ) {
    olf.unlock();
    Protocol::SendError(sock, String("Component ") + name + " is not 'enableable' or does not exist.");
    return false;
  }
  e->setEnabled(false);
  olf.unlock();
  return true;
}

bool ConnThread::doIsEnabled(StringList &argv)
{
  String name = argv.front();
  olf.lock();
  Enableable *e = dynamic_cast<Enableable *>(olf.find(name));
  if (!e) {
    olf.unlock();
    Protocol::SendError(sock, String("Enableable ") + name + " not found.");
    return false;
  }
  bool isIt = e->isEnabled();
  olf.unlock();
  xmit(isIt ? "1\n" : "0\n");
  return true;  
}

#include <time.h>

bool ConnThread::doMonitor(StringList &argv)
{
  String rateStr = argv.front(); argv.pop_front();
  String objName = argv.front(); argv.pop_front();
  String cmd = argv.front(); argv.pop_front();
  unsigned rate = rateStr.toUInt();
  if (!rate || rate > 100) {
    Protocol::SendError(sock, String("Rate ") + rateStr + " is either invalid or out of range.");
    return false;
  }
  olf.lock();
  Component *obj = olf.find(objName);
  olf.unlock();
  Bank *b = 0;
  Mix *m = 0;
  FlowController *c = 0;
  if (obj) {
    b = dynamic_cast<Bank *>(obj);
    m = dynamic_cast<Mix *>(obj);
    c = dynamic_cast<FlowController *>(obj);
  }
  std::transform(cmd.begin(), cmd.end(), cmd.begin(), tolower);
  //actualflow|commandedflow|actualcarrierflow|commandedcarrierflow|actualodorflow|commandedodorflow|enabled|odor|odortable
  static const String validMixCmds[] = {
    "actualcarrierflow", "commandedcarrierflow", "actualodorflow", "commandedodorflow"
  };
  static const String *endValidMixCmds = &validMixCmds[sizeof(validMixCmds) / sizeof(*validMixCmds)];
  static const String validBankCmds[] = {
    "actualflow", "commandedflow", "enabled", "odor", "odortable"    
  };
  static const String *endValidBankCmds = &validBankCmds[sizeof(validBankCmds) / sizeof(*validBankCmds)];
  static const String validFCCmds[] = {
    "actualflow", "commandedflow"
  };
  static const String *endValidFCCmds = &validFCCmds[sizeof(validFCCmds) / sizeof(*validFCCmds)];

  if (m) {
    const String *it = std::find(validMixCmds, endValidMixCmds, cmd); 
    if (it >= endValidMixCmds) {
      Protocol::SendError(sock, String("Command ") + cmd + " is not a valid monitor mix command.");
      return false;
    }
  } else if (b) {
    const String *it = std::find(validBankCmds, endValidBankCmds, cmd); 
    if (it >= endValidBankCmds) {
      Protocol::SendError(sock, String("Command ") + cmd + " is not a valid monitor bank command.");
      return false;
    }
  } else if (c) {
    const String *it = std::find(validFCCmds, endValidFCCmds, cmd); 
    if (it >= endValidFCCmds) {
      Protocol::SendError(sock, String("Command ") + cmd + " is not a valid monitor flow controller command.");
      return false;
    }
  } else {
    Protocol::SendError(sock, String("Component ") + objName + " is not found.");
    return false;
  }
  Protocol::SendOK(sock);
  const unsigned period_ns = 1000000000 / rate;
  const struct timespec ts = { tv_sec : period_ns / 1000000000, tv_nsec : period_ns % 1000000000 };
  while (1) {
    ::pthread_testcancel();
    olf.lock();
    String res;
    if (m) {
      if (cmd == "actualcarrierflow") 
        res = Str( m->getCarrier()->flow() );
      if (cmd == "commandedcarrierflow") 
        res = Str( m->getCarrier()->commandedFlow() );
      if (cmd == "actualodorflow")
        res = Str( m->actualOdorFlow() );
      if (cmd == "commandedodorflow")
        res = Str( m->commandedOdorFlow() );
    } else if (b) {
      if (cmd == "actualflow") 
        res = Str( b->actualFlow() ); 
      if (cmd == "commandedflow") 
        res = Str( b->commandedFlow() );
      if (cmd == "enabled") {
        Enableable *e = dynamic_cast<Enableable *>(b);
        res = e ? Str( e->isEnabled() ) : "1";
      }
      if (cmd == "odor")
        res = Str( b->currentOdor() );
      if (cmd == "odortable")
        res = dumpOdorTable(b);
    } else if (c) {
      if (cmd == "actualflow") 
        res = Str( c->flow() ); 
      if (cmd == "commandedflow") 
        res = Str( c->commandedFlow() );
    }
    olf.unlock();
    xmit(res + "\n");
    ::clock_nanosleep(CLOCK_REALTIME, 0, &ts, 0);
  }
  // never reached!
  return false;
}

// caller must hold olf. lock!!
String ConnThread::dumpOdorTable(Bank *b) const
{
  std::ostringstream oss;
  Bank::OdorTable odorTable = b->odorTable();
  for (Bank::OdorTable::iterator it = odorTable.begin(); it != odorTable.end(); ++it) 
    oss << it->first << "\t" 
        << it->second.name << "\t" 
        << it->second.metadata << "\n";  
  return oss.str();
}

bool ConnThread::doSetDesiredTotalFlow(StringList &argv)
{
  String mixname = argv.front(); argv.pop_front();
  String flowstr = argv.front(); argv.pop_front();
  bool ok;
  double flow = flowstr.toDouble(&ok);
  if (!ok) {
    Protocol::SendError(sock, flowstr + " is not a valid real number.");
    return false;    
  }  
  olf.lock();
  Mix *m = dynamic_cast<Mix *>(olf.find(mixname));
  if (!m) {
    olf.unlock();
    Protocol::SendError(sock, String("Mix ") + mixname + " not found.");
    return false;
  }
  ok = m->setDesiredTotalFlow(flow);
  olf.unlock();
  if (!ok) {
    Protocol::SendError(sock, "Set flow operation failed -- flow might be out of range.");
    return false;
  }
  return true;  
}

bool ConnThread::doList(StringList &args)
{
  std::ostringstream ss;
  olf.lock();    
  std::list<Component *> cl = olf.children(true);

  if (args.size() == 1 && args.back().lower().startsWith("log")) {   // list logables only  
    
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      DataLogable *d = dynamic_cast<DataLogable *>(c);
      if (d) {        
        ss << c->name() << "\tLogID:" << c->id() << "\t";
        int ct = 0;
        if (d->loggingEnabled(DataLogable::Cooked)) ss << "LOGGING: Cooked", ++ct;
        if (d->loggingEnabled(DataLogable::Raw)) ss << (ct ? "," : "LOGGING: ") << "Raw", ++ct;
        if (d->loggingEnabled(DataLogable::Other)) ss << (ct ? "," : "LOGGING: ") << "Other", ++ct;
        if (!ct) ss << "NOT LOGGING";
        ss << "\n";
      }
    }
    
  } else if (args.size() == 1 && args.back().lower().startsWith("cont")) {   // list controlables only  
    
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      Controller *d = dynamic_cast<Controller *>(c);
      if (d) {        
        ss << c->name() << "\tCurrentParams: ";
        std::vector<double> p = d->controlParams();
        for (unsigned i = 0; i < p.size(); ++i)
          ss << (i ? "," : "") << p[i];
        ss << "\n";
      }
    }

  } else if (args.size() == 1 && (args.back().lower().startsWith("start") || args.back().lower().startsWith("run")) ) {   // list startstopables only  
    
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      StartStoppable *d = dynamic_cast<StartStoppable *>(c);
      if (d) {        
        ss << c->name() << "\t" << (d->isStarted() ? "RUNNING" : "NOT RUNNING") << "\n";
      }
    }
  } else if (args.size() == 1 && args.back().lower().startsWith("read")) {
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      Readable *r = dynamic_cast<Readable *>(c);
      if (r) 
        ss << c->name() << "\t" << r->read() << "\t" << r->readRaw() << "\n";
    }
  } else if (args.size() == 1 && args.back().lower().startsWith("writ")) {
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      Writeable *w = dynamic_cast<Writeable *>(c);
      if (w)
        ss << c->name() << "\n";
    }
  } else if (args.size() == 1 && args.back().lower().startsWith("sav")) {
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      Saveable *s = dynamic_cast<Saveable *>(c);
      if (s)
        ss << c->name() << "\n";
    }
  } else if (args.size() == 1 && args.back().lower().startsWith("calib")) {
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      Calib *ca = dynamic_cast<Calib *>(c);
      if (ca)
        ss << c->name() << "\n";
    }
  } else if (args.size() == 1 && args.back().lower().startsWith("enab")) {
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      Enableable *en = dynamic_cast<Enableable *>(c);
      if (en)
        ss << c->name() << "\n";
    }
  } else if (args.size() == 0) { // list all components

    cl.push_front(&olf); // just so the olfactometer itself is enumerated
    for (std::list<Component *>::iterator it = cl.begin(); it != cl.end(); ++it) {
      Component *c = *it;
      // first col, name
      ss << c->name() << "\t";

      // second col, parent
      ss << "[Parent:";
      if (c->parent()) {
        ss << c->parent()->name();
      } else {
        ss << "NONE";
      }
      ss << "]\t";

      // last col, type
      ss << "<" << c->typeName() << ">\n";
    }

  } else {

    olf.unlock();
    Protocol::SendError(sock, "Invalid component type: valid values are 'logables', 'startstoppables', 'controllables', 'readables', 'writeables', 'saveables', 'calib' or the empty string to list all components.");
    return false;

  }

  olf.unlock();
  xmit(ss.str());
  return true;
}

bool ConnThread::doGetControlParams(StringList &args)
{
    String cname = args.front();
    olf.lock();
    Component * c = olf.find(cname, true);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, String("Component ") + cname + " not found.");
      return false;
    }
    Controller *pc = dynamic_cast<Controller *>(c);
    if (!pc) {
      olf.unlock();
      Protocol::SendError(sock, String("Component ") + cname + " is not an object that has control parameters associated with it.");
      return false;      
    }
    std::ostringstream ss;
    std::vector<double> cp = pc->controlParams();
    for (unsigned i = 0; i < cp.size(); ++i)  ss << (i ? " " : "") << cp[i];
    ss << "\n";
    olf.unlock();
    xmit(ss.str());
    return true;
}

bool ConnThread::doSetControlParams(StringList &args)
{
    String cname = args.front();
    olf.lock();
    Component * c = olf.find(cname, true);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, String("Component ") + cname + " not found.");
      return false;
    }
    Controller *pc = dynamic_cast<Controller *>(c);
    if (!pc) {
      olf.unlock();
      Protocol::SendError(sock, String("Component ") + cname + " is not an object that uses control parameters.");
      return false;      
    }
    args.pop_front();
    std::vector<double> cp(pc->numControlParams());
    if (args.size() != cp.size()) {
      olf.unlock();
      Protocol::SendError(sock, String("Controllable ") + cname + " requires " + cp.size() + " control params, but only " + args.size() + " specified!");
      return false;
    }
    for(unsigned i = 0; i < cp.size(); ++i) {
      bool ok;      
      cp[i] = args.front().toDouble(&ok);
      if (!ok) {
        olf.unlock();
        Protocol::SendError(sock, String("Argument #") + (i+1) + " \"" + args.front() + "\" is not a valid real number.");
        return false;
      }
      args.pop_front();
    }

    if (!pc->setControlParams(cp)) {
      olf.unlock();
      Protocol::SendError(sock, String("Controller ") + cname + " error setting control params.");
      return false;      
    }
    olf.unlock();
    return true;
}

bool ConnThread::doGetCoeffs(StringList &args)
{
    String cname = args.front();
    std::ostringstream ss;
    olf.lock();
    Component * comp = olf.find(cname, true);
    if (!comp) {
      olf.unlock();
      Protocol::SendError(sock, String("Component ") + cname + " not found.");
      return false;
    }
    Calib *c = dynamic_cast<Calib *>(comp);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, cname + " is not a calibratable component.");
      return false;
    }
    Calib::Coeffs coffs = c->getCoeffs();
    olf.unlock();
    for (unsigned i = 0; i < coffs.size(); ++i)
      ss << (i ? " " : "") << coffs[i];
    ss << "\n";
    xmit(ss.str());
    return true;    
}

bool ConnThread::doSetCoeffs(StringList &args)
{
    String cname = args.front();
    std::vector<double> coffs(4);
    bool ok1, ok2, ok3, ok4;
    args.pop_front();
    coffs[0] = args.front().toDouble(&ok1);
    args.pop_front();
    coffs[1] = args.front().toDouble(&ok2);
    args.pop_front();
    coffs[2] = args.front().toDouble(&ok3);
    args.pop_front();
    coffs[3] = args.front().toDouble(&ok4);
    if (!ok1 || !ok2 || !ok3 || !ok4) {
      Protocol::SendError(sock, "Argument error -- please pass 4 numeric args after the calib_name: a b d c");
      return false;
    }    
    olf.lock();
    Component * comp = olf.find(cname, true);
    Calib *c = 0;
    if (!comp || !(c = dynamic_cast<Calib *>(comp))) {
      olf.unlock();
      Protocol::SendError(sock, String("Component ") + cname + " not found or is not a calibratable object.");
      return false;
    }
    if (!c->setCoeffs(coffs)) {
      olf.unlock();
      Protocol::SendError(sock, String("Calibratable ") + cname + " rejected the new coefficients.");
      return false;
    }
    olf.unlock();
    return true;
}

bool ConnThread::doGetCalib(StringList &argv)
{
    String cname = argv.front();
    olf.lock();
    Calib *c = 0;
    if ( !(c = dynamic_cast<Calib *>(olf.find(cname))) ) {
      olf.unlock();
      Protocol::SendError(sock, (cname + " not found.").c_str());
      return false;
    }
    Calib::Table table = c->getTable();
    olf.unlock();
    Calib::Table::const_iterator it;
    for (it = table.begin(); it != table.end(); ++it) {
      std::ostringstream ss;
      ss << it->first << " " << it->second << "\n";
      xmit(ss.str());
    }
    return true;
}

bool ConnThread::doSetCalib(StringList &argv)
{
    String cname = argv.front();
    olf.lock();
    Calib *c = 0;
    if ( !(c = dynamic_cast<Calib *>(olf.find(cname))) ) {
      olf.unlock();
      Protocol::SendError(sock, (cname + " not found.").c_str());
      return false;
    }
    olf.unlock();
    Calib::Table table; /*std::map<double, double>*/
    Protocol::SendReady(sock);
    for (; ;) {
      std::string line;
      int r = recvLine(line); // ignoring errors, hopefully that's ok

      // end of input  check
      if (line.length() == 0 || line[0] == '\n' || line[0] == '\r') 
        break;

      if (r != RecvOK)  return false;
      StringList fields = String::split(line, "[[:space:]=]+");
      if (fields.size() != 2) {
        Protocol::SendError(sock, "Wrong number of fields in line, need exactly two entries per line!");
        return false;        
      }
      double v, f;
      bool ok;
      v = String::toDouble(fields.front(), &ok);
      if (!ok) {
        Protocol::SendError(sock, (fields.front() + " is not a valid voltage!").c_str());
        return false;        
      }
      fields.pop_front();
      f = String::toDouble(fields.front(), &ok);
      if (!ok) {
        Protocol::SendError(sock, (fields.front() + " is not a valid flow!").c_str());
        return false;        
      }
      table[v] = f;
    }
    if (table.size() < 2) {
      Protocol::SendError(sock, "Need to send at least 2 entries for the calibration table!");
      return false;
    }
    olf.lock();
    bool status = c->setTable(table);
    olf.unlock();
    if (!status) 
      Protocol::SendError(sock, "Failed to set calibration table.");
    return status;
}

bool ConnThread::doStart(StringList &argv)
{
    String name = argv.front();
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    StartStoppable *s = dynamic_cast<StartStoppable *>(c);
    if ( !s ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a start/stopable object.").c_str());
      return false;
    }
    bool res = s->start();
    olf.unlock();
    if (!res) {
      Protocol::SendError(sock, (name + " refused to start or is already running.").c_str());
      return false;      
    }
    return true;
}

bool ConnThread::doStop(StringList &argv)
{
    String name = argv.front();
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    StartStoppable *s = dynamic_cast<StartStoppable *>(c);
    if ( !s ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a start/stopable object.").c_str());
      return false;
    }
    bool res = s->stop();
    olf.unlock();
    if (!res) {
      Protocol::SendError(sock, (name + " refused to stop or is already stopped.").c_str());
      return false;
    }
    return true;
}

bool ConnThread::doRunning(StringList &argv)
{
    String name = argv.front();
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    StartStoppable *s = dynamic_cast<StartStoppable *>(c);
    if ( !s ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a start/stopable object.").c_str());
      return false;
    }
    bool res = s->isStarted();
    olf.unlock();
    std::string str = res ? "1\n" : "0\n";
    xmit(str);
    return true;
}

bool ConnThread::doRead(StringList &argv)
{
    String name = argv.front();
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    Readable *r = dynamic_cast<Readable *>(c);
    if ( !r ) {
      // component itself not a sensor/actuator, but try and find one in the
      // child list
      std::list<Component *> chlds = c->children(false);
      for (std::list<Component *>::iterator it = chlds.begin();  it != chlds.end() && !r; ++it)
        r = dynamic_cast<Readable *>(*it);
    }
    if ( !r ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a readable and/or it does not contain any readables as subcomponents.").c_str());
      return false;
    }
    double res = 0.;
    res = r->read(); 
    olf.unlock();
    String str = String::Str(res) + "\n";
    xmit(str);
    return true;
}

bool ConnThread::doReadRaw(StringList &argv)
{
    String name = argv.front();
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    Readable *r = dynamic_cast<Readable *>(c);
    if ( !r ) {
      // component itself not a sensor/actuator, but try and find one in the
      // child list
      std::list<Component *> chlds = c->children(false);
      for (std::list<Component *>::iterator it = chlds.begin();  it != chlds.end() && !r; ++it)
        r = dynamic_cast<Readable *>(*it);
    }
    if ( !r ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a readable and/or it does not contain any readables as subcomponents.").c_str());
      return false;
    }
    double res = 0.;
    res = r->readRaw(); 
    olf.unlock();
    String str = String::Str(res) + "\n";
    xmit(str);
    return true;
}

bool ConnThread::doWrite(StringList &argv)
{
    String name = argv.front(); argv.pop_front();
    String valStr = argv.front(); argv.pop_front();
    bool ok;
    double val = valStr.toDouble(&ok);
    if (!ok) {
      Protocol::SendError(sock, (valStr + " is not a valid value.").c_str());
      return false;
    }
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    Writeable *w = dynamic_cast<Writeable *>(c);
    if ( !w ) {
      // component itself not an actuator, but try and find one in the
      // child list
      std::list<Component *> chlds = c->children(false);
      for (std::list<Component *>::iterator it = chlds.begin();  it != chlds.end() && !w; ++it)
        w = dynamic_cast<Writeable *>(*it);
    }
    if ( !w ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not an writeable and/or it does not contain any writeables as subcomponents.").c_str());
      return false;
    }
    ok = w->write(val);
    olf.unlock();
    if (!ok) {
      Protocol::SendError(sock, (name + " refused to accept " + valStr).c_str());
      return false;

    }
    return true;
}

bool ConnThread::doWriteRaw(StringList &argv)
{
    String name = argv.front(); argv.pop_front();
    String valStr = argv.front(); argv.pop_front();
    bool ok;
    double val = valStr.toDouble(&ok);
    if (!ok) {
      Protocol::SendError(sock, (valStr + " is not a valid value.").c_str());
      return false;
    }
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    Writeable *w = dynamic_cast<Writeable *>(c);
    if ( !w) {
      // component itself not an actuator, but try and find one in the
      // child list
      std::list<Component *> chlds = c->children(false);
      for (std::list<Component *>::iterator it = chlds.begin();  it != chlds.end() && !w; ++it)
        w = dynamic_cast<Writeable *>(*it);
    }
    if ( !w ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not an writeable and/or it does not contain any writeables as subcomponents.").c_str());
      return false;
    }
    ok = w->writeRaw(val);
    olf.unlock();
    if (!ok) {
      Protocol::SendError(sock, (name + " refused to accept " + valStr).c_str());
      return false;

    }
    return true;
}

bool ConnThread::doSave(StringList &argv)
{
    String name = argv.front(); argv.pop_front();
    olf.lock();
    Component *c = olf.find(name);
    if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
    }
    Saveable *s = dynamic_cast<Saveable *>(c);
    if ( !s ) {
      // component itself not a Saveable, but try and find one in the
      // child list
      std::list<Component *> chlds = c->children(false);
      for (std::list<Component *>::iterator it = chlds.begin();  it != chlds.end() && !s; ++it)
        s = dynamic_cast<Saveable *>(*it);
    }
    if ( !s ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a saveable and/or it does not contain any saveables as subcomponents.").c_str());
      return false;
    }
    bool ok = s->save();
    olf.unlock();
    if (!ok) {
      Protocol::SendError(sock, (name + " save failed.").c_str());
      return false;
    }
    return true;  
}

namespace {
  int dl_dt_from_str(const String &in) {
    String s = in.lower(); 
    if (s == "cooked") return DataLogable::Cooked;
    if (s == "raw") return DataLogable::Raw;
    if (s == "other") return DataLogable::Other;
    if (s == "any" || s == "all") return DataLogable::Other|DataLogable::Raw|DataLogable::Cooked;
    return -1;
  }
}

bool ConnThread::doIsDataLogging(StringList &args)
{
  String name = args.front();
  int dt = dl_dt_from_str(args.back());
  if (dt < 0) {
    Protocol::SendError(sock, (args.back() + " unknown log type -- must be one of cooked|raw|other|any.").c_str());
    return false;    
  }
  olf.lock();
  Component *c = olf.find(name);
  if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
  }
  DataLogable *d = dynamic_cast<DataLogable *>(c);
  if ( !d ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a data-logable component.").c_str());
      return false;
  }
  int ans = 0;
  while (dt) {
    int bit = ffs(dt) - 1, t = 0x1<<bit;
    dt &= ~t;
    ans |= d->loggingEnabled(static_cast<DataLogable::DataType>(t));
  }
  olf.unlock();
  xmit(String::Str(ans)+"\n");
  return true;
}

bool ConnThread::doSetDataLogging(StringList &args)
{
  String name = args.front();
  args.pop_front();
  int dt = dl_dt_from_str(args.front());
  if (dt < 0) {
    Protocol::SendError(sock, (args.front() + " unknown log type -- must be one of cooked|raw|other|any.").c_str());
    return false;    
  }
  args.pop_front();
  bool ok;
  bool en = args.front().toInt(&ok);
  if (!ok) {
    Protocol::SendError(sock, (args.front() + " must be a boolean numer (0/1)").c_str());
    return false;    
  }  
  olf.lock();
  Component *c = olf.find(name);
  if (!c) {
      olf.unlock();
      Protocol::SendError(sock, (name + " not found.").c_str());
      return false;
  }
  DataLogable *d = dynamic_cast<DataLogable *>(c);
  if ( !d ) {
      olf.unlock();
      Protocol::SendError(sock, (name + " is not a data-logable component.").c_str());
      return false;
  }
  int ans = 0;
  while (dt) {
    int bit = ffs(dt) - 1, t = 0x1<<bit;
    dt &= ~t;
    ans |= d->setLoggingEnabled(en, static_cast<DataLogable::DataType>(t));
  }
  olf.unlock();
  if (!ans) {
      Protocol::SendError(sock, (name + " failed to change data logging state.").c_str());
      return false;
  }
  return true;
}

bool ConnThread::doDataLogCount(StringList &)
{
  ComediOlfactometer * colf = dynamic_cast<ComediOlfactometer *>(&olf);
  if (!colf || !colf->dataLog()) {    
    Protocol::SendError(sock, "INTERNAL ERROR: Olfactometer object has no datalog!");
    return false;
  }
  olf.lock();
  unsigned num = colf->dataLog()->numEvents();
  olf.unlock();
  xmit(String::Str(num) + "\n");
  return true;
}

bool ConnThread::doGetDataLog(StringList &args)
{
  ComediOlfactometer * colf = dynamic_cast<ComediOlfactometer *>(&olf);
  if (!colf || !colf->dataLog()) {    
    Protocol::SendError(sock, "INTERNAL ERROR: Olfactometer object has no datalog!");
    return false;
  }
  bool ok;
  unsigned first = args.front().toUInt(&ok);
  if (!ok) {
    Protocol::SendError(sock, args.front() + " is not a valid number.");
    return false;
  }
  args.pop_front();
  unsigned num = args.front().toUInt(&ok);
  if (!ok) {
    Protocol::SendError(sock, args.front() + " is not a valid number.");
    return false;
  } 
  args.pop_front();
  bool erase = false;
  if (!args.empty()) erase = args.front().toUInt();
  olf.lock();
  std::list<DataEvent> evts;
  colf->dataLog()->getEvents(evts, first, num, erase);
  olf.unlock();
  if (evts.empty()) {
    Protocol::SendError(sock, "No events found.");
    return false;
  }
  /* NB the below code is slightly ugly but it's optimized to minimize
     CPU load, etc.  
      - Using snprintf seems faster than ostringstream for some reason
      - Using an id cache for the slow component name lookup by id..   */
  std::map<unsigned, std::string> idCache;
  std::map<unsigned, std::string>::const_iterator idc_it;
  std::list<DataEvent>::iterator it;
  static const int bufsz = 1024*54;
  char buf[bufsz];
  unsigned bufpos = 0, nbytesSent = 0;
  double tsent = GetTime();
  for (it = evts.begin(); it != evts.end(); ++it) {
    const DataEvent & e = *it;
    if ( (idc_it = idCache.find(e.id)) == idCache.end() ) {
      idCache[e.id] = Component::nameFromId(e.id);
      idc_it = idCache.find(e.id);
    }
    bufpos += snprintf(buf+bufpos, bufsz-bufpos, "%f %s %s %f\n", 
                       (e.ts_ns / 1000000000.000000), 
                       idc_it->second.c_str(),
                       e.meta, e.datum);
    if (bufpos > bufsz/2) { xmitBuf(buf, bufpos, false, false); nbytesSent += bufpos; bufpos = 0; }
  }
  if (bufpos) { xmitBuf(buf, bufpos, false, false); nbytesSent += bufpos; }
  tsent = GetTime() - tsent;
  LOG() << "Sent data log: " << nbytesSent << " bytes in " << tsent << " secs (" << (double(nbytesSent)/1024.0/tsent) << " KB/s).\n";

  return true;
}

bool ConnThread::doClearDataLog(StringList &)
{
  ComediOlfactometer * colf = dynamic_cast<ComediOlfactometer *>(&olf);
  if (!colf || !colf->dataLog()) {    
    Protocol::SendError(sock, "INTERNAL ERROR: Olfactometer object has no datalog!");
    return false;
  }
  olf.lock();
  colf->dataLog()->clearEvents();
  olf.unlock();
  return true;
}
