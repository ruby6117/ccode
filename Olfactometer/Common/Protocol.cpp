#include "Protocol.h"

#include <string.h>
#if defined(UNIX) || defined(LINUX) || !defined(WIN32)
#  include <sys/types.h>
#  include <sys/socket.h>
#else 
/* windows headers */
#  include <winsock.h>
#endif

// just in case compiler decides newline is \r\n, etc..?
static const int NL_LEN = ::strlen("\n");

void Protocol::SendOK(int sock) 
{
  static int protolen = -1;
  if (protolen < 0) protolen = ::strlen(Protocol::OKText);
  ::send(sock, Protocol::OKText, protolen, MSG_NOSIGNAL);
  ::send(sock, "\n", NL_LEN, MSG_NOSIGNAL);
}

void Protocol::SendReady(int sock) 
{
  static int protolen = -1;
  if (protolen < 0) protolen = ::strlen(Protocol::ReadyText);
  ::send(sock, Protocol::ReadyText, protolen, MSG_NOSIGNAL);
  ::send(sock, "\n", NL_LEN, MSG_NOSIGNAL);
}

void Protocol::SendError(int sock, const char *msg)
{
  int protolen = ::strlen(Protocol::ErrorText);
  int len = ::strlen(msg);
  ::send(sock, Protocol::ErrorText, protolen, MSG_NOSIGNAL);
  ::send(sock, " ", 1, MSG_NOSIGNAL); 
  ::send(sock, msg, len, MSG_NOSIGNAL);
  if (!len || msg[len-1] != '\n') ::send(sock, "\n", 1, MSG_NOSIGNAL);
}

// text based messages
const char * const Protocol::GetVersion = "GET VERSION";
const char * const Protocol::GetName = "GET NAME";
const char * const Protocol::GetDescription = "GET DESCRIPTION";
const char * const Protocol::Quit = "QUIT";
const char * const Protocol::Noop = "NOOP";
const char * const Protocol::Help = "HELP";
const char * const Protocol::Help2 = "?";
const char * const Protocol::GetMixes = "GET MIXES";
const char * const Protocol::GetBanks = "GET BANKS"; 
const char * const Protocol::GetNumOdors = "GET NUM ODORS"; 
const char * const Protocol::GetOdorTable = "GET ODOR TABLE"; 
const char * const Protocol::SetOdorTable = "SET ODOR TABLE"; 
const char * const Protocol::GetOdor = "GET BANK ODOR"; 
const char * const Protocol::SetOdor = "SET BANK ODOR"; 
const char * const Protocol::GetActualOdorFlow = "GET ACTUAL ODOR FLOW"; ///< takes 1 arg, a valid mixname returns total flow for odors (double ml/min)
const char * const Protocol::GetCommandedOdorFlow = "GET COMMANDED ODOR FLOW"; ///< takes 1 arg, a valid mixname returns total flow for odors (double ml/min)
const char * const Protocol::SetOdorFlow = "SET ODOR FLOW"; ///< takes 2 args, a valid mixname and  a double which is total flow for all odors in mix
const char * const Protocol::GetMixtureRatio = "GET MIX RATIO"; ///< takes 1 args, a valid bankname. Returns, 1 per line, BankName ratio
const char * const Protocol::SetMixtureRatio = "SET MIX RATIO"; ///< similar to above, takes a mixname, plus BankName=ratio, 1 per bank
const char * const Protocol::GetActualBankFlow = "GET ACTUAL BANK FLOW"; ///< takes 1 arg, a valid bankname
const char * const Protocol::GetCommandedBankFlow = "GET COMMANDED BANK FLOW"; ///< takes 1 arg, a valid bankname
const char * const Protocol::OverrideBankFlow = "OVERRIDE BANK FLOW"; ///< takes 2 args, a valid bankname and an odor flow in ml/min
const char * const Protocol::GetActualCarrierFlow = "GET ACTUAL CARRIER FLOW"; ///< takes 1 args, a valid mixname
const char * const Protocol::GetCommandedCarrierFlow = "GET COMMANDED CARRIER FLOW"; ///< takes 1 args, a valid mixname
const char * const Protocol::OverrideCarrierFlow = "OVERRIDE CARRIER FLOW"; ///< takes 2 args, a valid mixname and an odor flow in ml/min
const char * const Protocol::GetCommandedFlow = "GET COMMANDED FLOW"; ///< takes 1 args, a valid mixname/bankname/flowname
const char * const Protocol::GetActualFlow = "GET ACTUAL FLOW"; ///< takes 1 args, a valid mixname/bankname/flowname
const char * const Protocol::OverrideFlow = "OVERRIDE FLOW"; ///< takes 2 args, a valid mixname/bankname/flowname and a flow in ml/min
const char * const Protocol::Enable = "ENABLE"; ///< takes 1 arg, a valid enableable
const char * const Protocol::Disable = "DISABLE"; ///< takes 1 arg, a valid enableable
const char * const Protocol::IsEnabled = "IS ENABLED"; ///< takes 1 arg, a valid enableable
const char * const Protocol::Monitor = "MONITOR"; ///< takes 3 args
const char * const Protocol::SetDesiredTotalFlow = "SET DESIRED TOTAL FLOW"; ///< takes 2 args, mixname and flow in ml/min
const char * const Protocol::List = "LIST";
const char * const Protocol::GetControlParams = "GET CONTROL PARAMS";///< takes 1 arg, a pidflow controller name
const char * const Protocol::SetControlParams = "SET CONTROL PARAMS";///< takes 2+ args:  controller_name args..
const char * const Protocol::GetCoeffs = "GET COEFFS";///< takes 1 arg, a pidflow controller name
const char * const Protocol::SetCoeffs = "SET COEFFS";///< takes 5 args, a pidflow controller name a b c d
const char * const Protocol::GetCalib = "GET CALIB";///< takes 1 arg, a pidflow controller name
const char * const Protocol::SetCalib = "SET CALIB";///< takes >1 args, a pidflow controller name v=flow ...
const char * const Protocol::Start = "START"; ///< takes 1 args, a StartStoppable name
const char * const Protocol::Stop = "STOP"; ///< takes 1 args, a StartStoppable name
const char * const Protocol::Running = "RUNNING"; ///< takes 1 args, a StartStoppable
const char * const Protocol::Read = "READ"; ///< takes 1 arg, a sensor or a component containing a sensor
const char * const Protocol::ReadRaw = "RREAD"; ///< takes 1 arg, a sensor or a component containing a sensor
const char * const Protocol::Write = "WRITE"; ///< takes 2 args, an actuator or a component containing an actuator and a value
const char * const Protocol::WriteRaw = "RWRITE"; ///< takes 2 args, an actuator or a component containing an actuator and a value
const char * const Protocol::Save = "SAVE"; ///< takes 1 arg, a Saveable component -- actual things saved are component-specific
const char * const Protocol::IsDataLogging = "IS DATA LOGGING"; ///< takes 1 args, a datalogable component
const char * const Protocol::SetDataLogging = "SET DATA LOGGING"; ///< takes 2 args, a datalogable component and a boolean
const char * const Protocol::DataLogCount = "DATA LOG COUNT"; ///< takes 0 args
const char * const Protocol::GetDataLog = "GET DATA LOG"; ///< takes 2 args, a from and to range
const char * const Protocol::ClearDataLog = "CLEAR DATA LOG"; ///< takes 0 args

const char * const Protocol::ErrorText = "ERROR: ";
const char * const Protocol::OKText = "OK";
const char * const Protocol::ReadyText = "READY";

// Port 3336
const unsigned short Protocol::DefaultPort = 3336;
