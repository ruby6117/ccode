#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <netinet/tcp.h> 
#include <arpa/inet.h>
#include <netdb.h>
extern int h_errno;
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <comedilib.h>

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>

#include "ProbeComedi.h"
#include "ComediDevice.h"
#include "Log.h"
#include "Common.h"
#include "ConnThread.h"
#include "Protocol.h"
#include "Settings.h"
#include "Olfactometer.h"
#include "ComediOlfactometer.h"
#include "Conf.h"
#include "Monitor.h"
#include "ConsoleUI.h"
#include "GenConf.h"
#include "RTLCoprocess.h"

#define DEFAULT_LISTEN "0.0.0.0" // listen on all interfaces by default
#define DEFAULT_CONF_FILE "olfactometer.ini"

static u_int32_t parseAddress(const std::string & a)
{
  struct hostent *he = gethostbyname(a.c_str());
  if (!he) return 0;
  return *(u_int32_t *)he->h_addr;
}

static volatile bool stopItAll = false;
static int stopSignal = -1;

void sighandler(int sig)
{
    (void) sig;
    stopItAll = true;
    stopSignal = sig;
}


static int BindSocket(const Settings & conf)
{
   unsigned short port = 0;
   std::string hostArg;
   std::string portArg;
   //char portArg[10];

   int s = ::socket(PF_INET, SOCK_STREAM, 0);

   if (s < 0) {
     Perror("socket");
     exit(1);
   }

   int parm = 1;
   ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &parm, sizeof(parm));

   portArg = conf.get(Conf::Sections::General, Conf::Keys::listen_port);
   if (!portArg.length()) portArg = Str(Protocol::DefaultPort);
   hostArg = conf.get(Conf::Sections::General, Conf::Keys::listen_address);
   if (!hostArg.length()) hostArg = DEFAULT_LISTEN;

   port = ToUShort(portArg);
   
   if (!port) {
       Critical() << "Unacceptable port argument \"" << portArg << "\", exiting.\n";
       exit(1);
   }
   
   struct sockaddr_in sin;
   sin.sin_family = AF_INET;
   sin.sin_port = htons(port);
   sin.sin_addr.s_addr = parseAddress(hostArg);
   if (!sin.sin_addr.s_addr) hostArg = DEFAULT_LISTEN;

   int ret = ::bind(s, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin));
   if (ret) {
     Perror("bind");
     exit(1);
   }

   Log() << "Bound socket to " << hostArg << ":" << port << "\n";  
   
   return s;
}

static void printUsage(const String &progname = "OlfactometerServer")
{
  std::cout << "\nProgram Command-Line Usage:\n\n"
            << "\t" << progname << " [-v|--verbose] [conf]\n" 
            << "\t\t-v\tprint/log verbose debug messages\n"
            << "\t\tconf\tthe config file to use, (olfactometer.ini by default)\n"

            << "\n"

            << "\t" << progname << " -h (or --help)\n"
            << "\t\t\tPrint this message\n"

            << "\n";
}

static bool ParseConfig(Settings &conf, int argc, char *argv[])
{
  std::string confFile = DEFAULT_CONF_FILE;
  String progName = argv[0];
  while (--argc) {
    String arg = *++argv;
    if (!arg.startsWith("-")) confFile = *argv;
    else {
      if (arg == "-v" || arg == "--verbose") Debug::enabled = true;
      else {
        printUsage(progName);
        std::exit(1);
      }
    }
  }
  conf.setFile(confFile);
  if (!conf.parse()) {
    Error() << "Could not open config file: " << confFile << ", exiting!\n";
    return false;
  }
 
  return true;
}



static void DoGeneralSetup(GenConf &conf,
                           const Settings & settings)
{
  conf.name = settings.get(Conf::Sections::General,
                           Conf::Keys::name);
  conf.description = settings.get(Conf::Sections::General,
                                  Conf::Keys::description);

  // grab the connection timeout setting from the config file.
  conf.connectionTimeoutSecs 
    = String::toInt(settings.get(Conf::Sections::General, 
                                 Conf::Keys::connection_timeout_seconds));
}

// instead of using the static keyword, we use the anonymous namespace 
namespace {

    /* 'Static' Global Objects:
       order of these objects in this file is important due to rules
       about the order of destruction -- and we definitely want the
       olfactometer destroyed *before* the coprocess as part of its
       destruction might involve calling into the coprocess, etc.
       Also the monitor calls into the olf. so it needs to be
       destroyed first.. */

     RTLCoprocess coprocess("OlfCoprocess"), *coprocess_ptr = &coprocess;
     Settings settings;
     GenConf conf;
     ComediOlfactometer olf;
     Monitor monitor;
    /** Purpoe of this object is to have its destructor invoked at program exit
        before the above object instances so that it can cancel all 
        active connections to the server. */        
     struct ConnThreadCanceller {
         ~ConnThreadCanceller() { ConnThread::CancelAll(); }
     } connThreadCanceller;

     int serverSocket = -1;

    /** Class to kill the server socket.  Preferably done  before slave 
        connections are cancelled so as to prevent new slave connections and 
        maybe avoid a race condition. */
     struct SocketReleaser {
         ~SocketReleaser() { ::close(serverSocket); serverSocket = -1; }
     } socketReleaser;

  /** Just cancels the console ui in its destructor -- preferably
      done as the first thing at destruction time. */
     struct ConsoleUIStopper {
         ~ConsoleUIStopper() {  ConsoleUI::instance()->stop(); }
     } consoleUIStopper;
}; // end anonymous (local) namespace


int main(int argc, char *argv[])
{
   if ( !ParseConfig(settings, argc, argv) ) {
     Error() << "Config error, exiting.\n";
     return 2;
   }

   if ( !coprocess.reload() ) {
       Warning() << "Could not load or attach to the coprocess kernel module: " << coprocess.modName() << "\n";
       coprocess_ptr = 0;
   }

   olf.doProbe();
   
   if (olf.nDevs() == 0) {
     Error() << "No comedi devices found\n";
     return 1;
   }
   
   Log() << "Found " << olf.nDevs() << " comedi devices.\n";

   DoGeneralSetup(conf, settings); // misc setup items, sets some global vars

   if ( !olf.doSetup(settings, coprocess_ptr) ) {
     Error() << "Could not initialize olfactometer, exiting.\n";
     return 3;
   }
   
   {
     Log l;
     l << olf.name() << " created with " << olf.numMixes() << " mixing units: ";
     std::list<Mix *> mixes = olf.mixes();
     for (std::list<Mix *>::iterator it = mixes.begin(); it != mixes.end(); ++it) {
       Mix *m = *it;
       if (it != mixes.begin()) l << ", ";
       l << "mix \"" << m->name() << "\" with " << m->numBanks() << " banks";
     }
     l << "\n";       
   }

   if (!monitor.start(settings, &olf)) {
     Error() << "Could not start system monitor thread, exiting.\n";     
     return 4;
   }

   serverSocket = BindSocket(settings);

   if (::listen(serverSocket, 1)) {
     Perror("listen");
     return 5;
   }
   
   if ( ! ConsoleUI::instance()->start(conf, &olf) ) {
     Error() << "Could not start the console UI.\n";
     return 6;
   }

   ::signal(SIGINT, sighandler);
   ::signal(SIGQUIT, sighandler);
   ::signal(SIGTERM, sighandler);   

   // for now we only handle one connection at a time
   while (!stopItAll) {

     struct pollfd pfd = { fd: serverSocket, events: POLLIN|POLLPRI, 
                           revents: 0 };
     
     int tmp = ::poll(&pfd, 1, 1000); // wakeup every 1000 ms..
     if (tmp < 0) { if (errno == EINTR) continue; else stopItAll = true; }
     else if (tmp > 0) {
       struct sockaddr_in addr;
       socklen_t len = sizeof(addr);
       int connSock = ::accept(serverSocket, reinterpret_cast<sockaddr *>(&addr), &len); 
       if (connSock < 0) {
         Perror("accept");
         return 6;
       }
       
       new ConnThread(connSock, inet_ntoa(addr.sin_addr), olf, conf.connectionTimeoutSecs); // note ConnThread deletes itself when done
     }

   }
   
   // explicitly call this here so below message appears on regular console
   ConsoleUI::instance()->stop();   

   if (stopSignal > -1)
     Log() << "Caught signal " << stopSignal << ", cleaning up and exiting...";

   return 0; // note: global d'tors funcs called after this point!
}
