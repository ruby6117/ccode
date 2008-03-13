#ifndef Protocol_H
#define Protocol_H


// the client/server protocol we are using
namespace Protocol
{
  // 3336
  extern const unsigned short DefaultPort;

  // text based messages
  extern const char * const GetVersion; ///< takes no args, returns version string, OKText
  extern const char * const GetName; ///< takes no args, returns name string, OKText
  extern const char * const GetDescription; ///< takes no args, returns descr string, OKText
  extern const char * const Quit; ///< takes no args
  extern const char * const Help; ///< takes no args, lists all commands
  extern const char * const Help2; ///< takes no args, lists all commands
  extern const char * const Noop; ///< noop command, takes no args, does nothing, returns OKText
  extern const char * const GetMixes; ///< takes no args, returns a comma-separated list of mix names \n OKText
  extern const char * const GetBanks; ///< takes 1 arg, a valid mixname returns a comma-separated list of bank names \n OKText
  extern const char * const GetNumOdors; ///< takes 1 arg, a valid bankname returns the number of odors this banks supports \n OKText
  extern const char * const GetOdorTable; ///< takes 1 arg, a valid bankname returns the odor table, 1 per line, tab-delimited \n OKText
  extern const char * const SetOdorTable; ///< takes 1 arg, a valid bankname, then it reads the socket, expecting NUMODORS of lines, each tab-delimited odorid\todorname\tdescrption
  extern const char * const GetOdor; ///< takes 1 arg, a valid bankname, returns the odor number of the current odor for the bank
  extern const char * const SetOdor; ///< takes 2 args, a valid bankname, and an odor number  
  extern const char * const GetActualOdorFlow; ///< takes 1 arg, a valid mixname returns total *from hardware* flow for odors (double ml/min)
  extern const char * const GetCommandedOdorFlow; ///< takes 1 arg, a valid mixname returns total commanded flow for odors (double ml/min)
  extern const char * const SetOdorFlow; ///< takes 2 args, a valid mixname and  a double which is total flow for all odors in mix
  extern const char * const GetMixtureRatio; ///< takes 1 args, a valid bankname. Returns, 1 per line, BankName ratio
  extern const char * const SetMixtureRatio; ///< similar to above, takes a mixname, plus BankName=ratio, 1 per bank
  extern const char * const GetActualBankFlow; ///< takes 1 arg, a valid bankname
  extern const char * const GetCommandedBankFlow; ///< takes 1 arg, a valid bankname
  extern const char * const OverrideBankFlow; ///< takes 2 args, a valid bankname and an odor flow in ml/min
  extern const char * const GetActualCarrierFlow; ///< takes 1 args, a valid mixname
  extern const char * const GetCommandedCarrierFlow; ///< takes 1 args, a valid mixname
  extern const char * const OverrideCarrierFlow; ///< takes 2 args, a valid mixname and an odor flow in ml/min
  extern const char * const GetActualFlow; ///< takes 1 args a valid mix/bank/flow_controller
  extern const char * const GetCommandedFlow; ///< takes 1 args a valid mix/bank/flow_controller
  extern const char * const OverrideFlow; ///< takes 2 args a valid mix/bank/flow_controller and a flow in ml/min
  extern const char * const Enable; ///< takes 1 arg, a valid enableable
  extern const char * const Disable; ///< takes 1 arg, a valid enableable
  extern const char * const IsEnabled; ///< takes 1 arg, a valid enableable
  extern const char * const Monitor; ///< takes 3 args, rate in hz, bank|mix name, param
  extern const char * const SetDesiredTotalFlow; ///< takes 2 args, mixname and flow in ml/min
  extern const char * const List; ///< takes varags
  extern const char * const GetControlParams; ///< takes 1 args
  extern const char * const SetControlParams; ///< takes 2+ args
  extern const char * const GetCoeffs; ///< takes 1 args
  extern const char * const SetCoeffs; ///< takes 5 args
  extern const char * const GetCalib; ///< takes 1 args
  extern const char * const SetCalib; ///< takes 1 args, but also some text input
  extern const char * const IsDataLogging; ///< takes 2 args, a datalogable component and one of 'cooked' 'raw' 'other'
  extern const char * const SetDataLogging; ///< takes 3 args, a datalogable component, one of cooked, raw, other,  and a boolean
  extern const char * const DataLogCount; ///< takes 0 args
  extern const char * const GetDataLog; ///< takes 2 args, a from and to range
  extern const char * const ClearDataLog; ///< takes 0 args
  extern const char * const Read; ///< takes 1 arg, a sensor or a component containing a sensor
  extern const char * const ReadRaw; ///< takes 1 arg, a sensor or a component containing a sensor
  extern const char * const Write; ///< takes 1 arg, an actuator or a component containing an actuator
  extern const char * const WriteRaw; ///< takes 1 arg, an actuator or a component containing an actuator  
  extern const char * const Start; ///< takes 1 args, a StartStoppable name
  extern const char * const Stop; ///< takes 1 args, a StartStoppable name
  extern const char * const Running; ///< takes 1 args, a StartStoppable name
  extern const char * const Save; ///< takes 1 arg, a Saveable component -- actual things saved are component-specific

  extern const char * const ErrorText;
  extern const char * const OKText;
  extern const char * const ReadyText;

  extern void SendError(int sock, const char *msg);
  extern void SendOK(int sock);
  extern void SendReady(int sock); ///< used to indicate server is ready and waiting for more data -- used for commands that require client to send data after the initial command
};


#endif
