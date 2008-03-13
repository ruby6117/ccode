#ifndef ConsoleUI_H
#define ConsoleUI_H

#include <pthread.h>
#include "GenConf.h"

class Olfactometer;

class CursesWindow;

/** A class that implements the console UI for the olfactometer server */
class ConsoleUI
{
private:
  ConsoleUI();
  static ConsoleUI singleton;

public:
  ~ConsoleUI();
  /** call this to get a pointer to the singleton instance.  The first
      time this is called it actually creates the instance, so call it only
      when you are ready to initialize the console using curses, etc. */
  static ConsoleUI *instance();
  
  bool start(const GenConf &conf, Olfactometer *olf);
  void stop();

private: // data
  volatile bool isrunning, gotlog;
  pthread_mutex_t mut; // mutex for log, etc
  pthread_t thr;
  CursesWindow *masterWin, *frameWin, *mainWin, *workWin, *addrWin, *uptimeWin, *sepWin, *logWin, *statusBar, *saWin;
  GenConf conf;
  Olfactometer *olf;
  String ipAddresses;
  int stderr_cpy, devnull_fd;
  
private: // funcs
  static void *threadWrapper(void *arg);
  static void logCallback(void *arg, const String & logentry);
  void lock();
  void unlock();
  void threadFunc();
  void setup();
  void cleanup();
};

/// inherit from this class to make certain Components/Actuators/Etc not appear in the console UI screen
class UIHidden 
{
public:
  virtual ~UIHidden(){}
};

#endif
