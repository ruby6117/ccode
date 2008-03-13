#ifndef ConnThread_H
#define ConnThread_H

#include <map>
#include <pthread.h>
#include <string>
#include "Common.h"

class Olfactometer;
class Bank;

class ConnThread
{
public:
  ConnThread(int sock, const std::string & remoteHost,
             Olfactometer & theOlf, 
             int timeout_seconds = 0 /* 0 means use default timeout of 1 hour, 
                                        negative means infinite timeout */);
  ~ConnThread();
  void setTimeout(int seconds); // negative for no timeout
  int timeout() const; // returns number of seconds for connection timeouts

  static void CancelAll();

protected:
  void doIt();

private:

  struct ProtocolHandler
  {
    static const int NOARGCHK;
    const char * cmd; ///< from Protocol.h
    int nArgs; ///< number of arguments the function takes, or set to NOARGCHK to just accept any number of arguments, or negative to accept *at least* ABS(nArgs) number of arguments
    const char * synopsis; ///< synopsis of arguments, if any
    bool (ConnThread::*handler)(StringList &); ///< function implementing protocol command
  };
  /// associate protocol commands to member function pointers, 
  /// array terminates with entry whose members point to null
  static const ProtocolHandler protocolHandlers[];

  static const ProtocolHandler *findProtocolHandler(const String &cmdline); 

  // *** PROTOCOL COMMAND HANDLERS -- they get put in protocolFunctions
  //     array above and dispatch from processCommand()

  // the below protocol functions may close the connection and exit
  // the thread on comm/socket failure!  
  bool doHelp(StringList &args_ignored); ///< always succeeds
  bool doNoop(StringList &args_ignored); ///< always succeeds
  bool doQuit(StringList &args_ignored); ///< always succeeds
  bool doGetVersion(StringList &args_ignored); ///< always succeeds
  bool doGetName(StringList &args_ignored); ///< always succeeds
  bool doGetDescription(StringList &args_ignored); ///< always succeeds
  bool doGetMixes(StringList &args_ignored);
  bool doGetBanks(StringList & args); ///< returns true on success, or on error sends the errortext to the socket (so called doesn't have to) and returns false, may quit thread on commfailure of course
  bool doGetNumOdors(StringList &args); ///< similar to above
  bool doGetOdorTable(StringList &args); ///< similar to above
  bool doSetOdorTable(StringList &args); ///< similar to above
  bool doGetOdor(StringList &args); ///< similar to above
  bool doSetOdor(StringList &args); ///< similar to above, takes 2 args
  bool doGetActualOdorFlow(StringList &args); ///< similar to above, takes 1 args mixname and returns a double (flow in ml/min)
  bool doGetCommandedOdorFlow(StringList &args); ///< similar to above, takes 1 args mixname and returns a double (flow in ml/min)
  bool doSetOdorFlow(StringList &args); ///< similar to above, takes 2 args mixname and double (flow in ml/min)
  bool doGetMixtureRatio(StringList &args); ///< similar to above, takes 1 arg, a mixname, returns 1 line for each bank
  bool doSetMixtureRatio(StringList &args); ///< similar to above, takes a mixname, plus BankName=ratio, 1 per bank
  bool doGetActualBankFlow(StringList &args);
  bool doGetCommandedBankFlow(StringList &args);
  bool doOverrideBankFlow(StringList &args);
  bool doGetActualCarrierFlow(StringList &args);
  bool doGetCommandedCarrierFlow(StringList &args);
  bool doOverrideCarrierFlow(StringList &args);
  bool doGetActualFlow(StringList &args);
  bool doGetCommandedFlow(StringList &args);
  bool doOverrideFlow(StringList &args);
  bool doEnable(StringList &args);
  bool doDisable(StringList &args);
  bool doIsEnabled(StringList &argv);
  bool doMonitor(StringList &argv);
  bool doSetDesiredTotalFlow(StringList &argv);
  bool doList(StringList &argv);
  bool doGetControlParams(StringList &argv);
  bool doSetControlParams(StringList &argv);
  bool doGetCoeffs(StringList &argv);
  bool doSetCoeffs(StringList &argv);
  bool doGetCalib(StringList &argv);
  bool doSetCalib(StringList &argv);
  bool doStart(StringList &args);
  bool doStop(StringList &args);
  bool doRunning(StringList &args);
  bool doRead(StringList &args);
  bool doReadRaw(StringList &args);
  bool doWrite(StringList &args);
  bool doWriteRaw(StringList &args);
  bool doSave(StringList &args);
  bool doIsDataLogging(StringList &);
  bool doSetDataLogging(StringList &);
  bool doDataLogCount(StringList &);
  bool doGetDataLog(StringList &);
  bool doClearDataLog(StringList &);
private:
  pthread_t thr;
  int sock;
  double startTime;
  std::string remoteHost;
  Olfactometer & olf;
  int timeout_ms;

  enum PollStatus { PollError = 0, PollAgain, PollTimedOut, PollOK, PollOk = PollOK };
  PollStatus poll() const;
  char *lineBuf; ///< used with recvLine to store remnants of last line
  unsigned sz; // size of above line
  enum RecvStatus { RecvError = 0, RecvAgain, RecvOK, RecvOk = RecvOK };
  RecvStatus recvLine(std::string & line_buf_out);
  
  // dispatches a protocol command to the appropriate handler
  void processCommand(const std::string & line);

  bool xmit(const std::string & str);
  bool xmitBuf(const void *buf, size_t num, bool isBinary = false, bool logXmission = true);

  // caller must hold olf. lock!!
  String dumpOdorTable(Bank *b) const;

  typedef std::map<pthread_t, ConnThread *> ThreadMap;
  static pthread_mutex_t mut; // mutex for 'threads' below..
  static ThreadMap threads;

  static void *ThreadFuncWrapper(void *);
  static void CleanupHandler(void *);
  static double GetTime();
 
};

#endif
