#include "ConsoleUI.h"
#include <pthread.h>
#include <ncurses.h>
#include <map>
#include "Common.h"
#include "Log.h"
#include "System.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "Curses.h"
#include "Olfactometer.h"
#include "StartStoppable.h"
#include "Controller.h"
#include <limits.h>

// static data
ConsoleUI ConsoleUI::singleton;

static String uptimeFMT(double secs);

ConsoleUI::ConsoleUI() 
  : isrunning (false), gotlog(false), mainWin(0), workWin(0), addrWin(0), uptimeWin(0), sepWin(0), logWin(0), statusBar(0), saWin(0), olf(0)
{
  pthread_mutex_init(&mut, 0);
  devnull_fd = ::open("/dev/null", O_RDWR);
  stderr_cpy = dup(2);
}

ConsoleUI::~ConsoleUI()
{
  stop();
  pthread_mutex_destroy(&mut);
  ::close(devnull_fd);
  ::close(stderr_cpy);
}

bool ConsoleUI::start(const GenConf &c, Olfactometer *olf)
{
  if (isrunning) return true;
  conf = c;
  this->olf = olf;
  int ret = ::pthread_create(&thr, 0, &threadWrapper, reinterpret_cast<void *>(this));
  isrunning = ret == 0;
  return isrunning;
}

void ConsoleUI::stop()
{
  if (isrunning) {
    ::pthread_cancel(thr);
    ::pthread_join(thr, 0);
    isrunning = false;
  }
  cleanup();
}

void setupColorPairs()
{
  if (has_colors()) {
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_WHITE, COLOR_RED);
    init_pair(3, COLOR_GREEN, COLOR_RED);    
    init_pair(4, COLOR_YELLOW, COLOR_WHITE);    
    init_pair(5, COLOR_YELLOW, COLOR_BLUE);    
    init_pair(6, COLOR_MAGENTA, COLOR_BLUE);    
  }
}

void ConsoleUI::setup()
{
  ::dup2(devnull_fd, 2); // make stderr not do anything..

  initscr();
  start_color(); // try and use color mode
  setupColorPairs();

  raw(); // turn off cooked mode and signals
  noecho(); // do not echo typed characters to the screen
  nonl(); // nl does not go to the next line, just does a CR without the NL  

  int rows = ::LINES,  cols = ::COLS;

  String titleStr = String(PRG_NAME) + " " PRG_VERSION_STR;
  int titleCol = (cols/2 - titleStr.length()/2) - 2;

  masterWin = new CursesWindow( 0, "MasterCursesWindow", Size(rows,cols), Point(0,0) , 1, 1);

  // framewin.. encapsulates mainWin + border
  frameWin = new CursesWindow( masterWin, "FrameForMainWindowArea", Size(masterWin->rows()-1, masterWin->cols()), Point(0,0) );
  // the main window, fits inside framewin
  mainWin = new CursesWindow( frameWin, "MainWindowArea", Size(frameWin->rows()-2, frameWin->cols()-2), Point(1,1) );
  int splitrows = (mainWin->rows()/2)-1;
  workWin = new CursesWindow( mainWin, "WorkWindowArea", Size(splitrows, mainWin->cols()) );
  sepWin = new CursesWindow( mainWin, "HorizSeparator", Size(1, mainWin->cols()), Point(workWin->rows(), 0) );
  Point pos = Point(workWin->rows()+sepWin->rows(), 0);
  logWin = new CursesWindow( mainWin, "LogWindow", Size(mainWin->rows()-pos.y(), mainWin->cols()), pos, 6 );
  
  // status bar at the bottom
  statusBar = new CursesWindow( masterWin, "StatusBar", Size(1, masterWin->cols()), Point(masterWin->rows()-1, 0) , 2, 2 );
  
  keypad(masterWin->win(), true); // enable keypad mode, to get special function key input

  wborder(frameWin->win(), 0, 0, 0, 0, 0, 0, 0, 0);
  whline(sepWin->win(), ACS_HLINE, mainWin->cols());
  frameWin->putStr(titleStr, Point(0, titleCol), 2, A_BOLD);
  sepWin->putStr("Operational Log:", Point(0, 0), 5, A_BOLD);


  // put stats in workwin
  Point cur = workWin->putStr("IP Address: ", Point(0, 0), 0, A_BOLD, false);
  cur += Point(0,1); // slide the cursor over 1
  addrWin = new CursesWindow(workWin, "IPAddressLoc", Size(1, workWin->cols()-cur.x()-1), cur);
  ipAddresses = "";

  // uptime stat
  cur = workWin->putStr("Uptime: ", Point(1, 0), 0, A_BOLD, false);
  cur += Point(0,1); // slide the curses over 1 char
  uptimeWin = new CursesWindow(workWin, "UpTimeArea", Size(1, workWin->cols()-cur.x()-1), cur);
  cur += Point(1,0); // next line..

  // sensors/actuators stats
  saWin = new CursesWindow(workWin, "SensorActuatorArea", Size(workWin->rows()-cur.y(), workWin->cols()-1), Point(cur.y(), 0));
  saWin->putStr("Initializing...", Point(0,0));

  masterWin->refresh();                                    

  Log::addNotifyFunc(logCallback, this);
  gotlog = true; // force the log to refresh
}

void ConsoleUI::cleanup()
{
  Log::removeNotifyFunc(logCallback);

  // TODO: delete all allocated windows here..
  if (masterWin) {
    delete masterWin; // should delete all children too
    masterWin = 0;
    endwin();
    ::dup2(stderr_cpy, 2); // make stderr work again
  }
}

// static
void *ConsoleUI::threadWrapper(void *arg)
{
  ConsoleUI *self = reinterpret_cast<ConsoleUI *>(arg);
  self->threadFunc();
  return 0;
}

// static member
ConsoleUI *
ConsoleUI::instance()
{
  return &singleton;  
}


void ConsoleUI::threadFunc()
{
  setup();

  int ch, breakout = 0;
  int scroll_line = 0;
  wtimeout(*masterWin, 1000); // 1000 ms timeouts on keyb reads..
  while ( !breakout && (ch = wgetch(*masterWin)) ) {
    pthread_testcancel();
    String prest = "";
    switch (ch) {
    case KEY_LEFT:  prest = "<Left Arrow>"; break;
    case KEY_RIGHT:  prest = "<Right Arrow>"; break;
    case KEY_UP: --scroll_line;  prest = "<Up Arrow>"; break;
    case KEY_DOWN: ++scroll_line;  prest = "<Down Arrow>"; break;
    case KEY_NPAGE: scroll_line += saWin->rows(); prest = "<PgDn>"; break;
    case KEY_PPAGE: scroll_line -= saWin->rows(); prest = "<PgUp>"; break;
    case KEY_END: scroll_line = INT_MAX; prest = "<End>"; break;
    case KEY_HOME: scroll_line = 0; prest = "<Home>"; break;
    case ERR:  prest = " "; break;
//     case KEY_BREAK: 
//     case KEY_CANCEL:
//     case KEY_CLOSE:
//     case KEY_EXIT:
//       breakout = 1; break;
    default:
      if (ch >= KEY_F0 && ch <= KEY_F0+63)
        prest = String("F") + static_cast<int>(ch-KEY_F0);
      break;
    }
    if (scroll_line < 0) scroll_line = 0;

    // update the work window
    {
      // check if ip addresses changed, if so update
      StringList ips = System::ipAddressList();  String tmp;
      if ( (tmp = String::join(ips , " ")) != ipAddresses ) {
        werase(*addrWin);
        ipAddresses = tmp;
        addrWin->putStr(ipAddresses, Point(0, 0));
      }

      // update uptime
      werase(*uptimeWin);
      String uptime = uptimeFMT(System::uptime());
      uptimeWin->putStr(uptime, Point(0, 0));

      // update sensor/actuator status area
      werase(*saWin);
      {
        Point cur(0,0);
        olf->lock();
        std::list<Component *> chld = olf->children(true);
        olf->unlock();

        unsigned secondCol = 0, ncomps = 0;
        for(std::list<Component *>::iterator it = chld.begin(); it != chld.end(); ++it) {
          Component *c = *it; 
          if (!c || dynamic_cast<UIHidden *>(c)) continue;
          Readable *r = dynamic_cast<Readable *>(c);
          Controller *ct = dynamic_cast<Controller *>(c);
          StartStoppable *ss = dynamic_cast<StartStoppable *>(c);
          // figure out the longest pos
          if ( (r || ct || ss) ) {
            ++ncomps;
            if ( c->name().length() > secondCol) 
              secondCol = c->name().length();
          }
        }

        // if we scrolled too far, force scroll_line pos to be at last 'page'
        if (ncomps < unsigned(scroll_line) + saWin->rows()) 
          scroll_line = ncomps - saWin->rows();

        int comp_num = 0;
        secondCol += 2;
        for(std::list<Component *>::iterator it = chld.begin(); cur.y() < saWin->rows() && it != chld.end(); ++it) {
          Component *c = *it;
          if (!c || dynamic_cast<UIHidden *>(c)) continue;
          Readable *r = dynamic_cast<Readable *>(c);
          StartStoppable *ss = dynamic_cast<StartStoppable *>(c);
          Controller *ct = dynamic_cast<Controller *>(c);
          Enableable *en = dynamic_cast<Enableable *>(c);
          if ( (r || ct || ss || en) && comp_num++ < scroll_line) continue;
          if (r) { // list readables too
            String str("");
            str = str + c->name() + ": ";            
            cur = Point(cur.y(), secondCol - str.length()); // right justify
            cur = saWin->putStr(str, cur, 0, A_BOLD, false);
            cur = Point(cur.y(), secondCol);
            UnitRangeObject *o = dynamic_cast<UnitRangeObject *>(c);
            str = String() + r->read() + " " + ( o ? o->unit() : "") + " (" + r->readRaw() + "V) " ;
            cur = saWin->putStr(str, cur, 0, 0, false);            
          } else if (ct) { // list controllables too
            String str("");
            str = str + c->name() + ": ";            
            cur = Point(cur.y(), secondCol - str.length()); // right justify
            cur = saWin->putStr(str, cur, 0, A_BOLD, false);
            cur = Point(cur.y(), secondCol);
            str = "";
            std::vector<double> params = ct->controlParams();
            for (unsigned i = 0; i < params.size(); ++i)
              str += String() + params[i] + String(" ");
            cur = saWin->putStr(str, cur, 0, 0, false);            
          } else if (en) { // list enableable components too
            String str("");
            str = str + c->name() + ": ";            
            cur = Point(cur.y(), secondCol - str.length()); // right justify
            cur = saWin->putStr(str, cur, 0, A_BOLD, false);
            cur = Point(cur.y(), secondCol);
          } else continue;
          if (ss) {
            cur = saWin->putStr(ss->isStopped() ? "*Stopped* " : "Running ", Point::null, 5, ss->isStarted() ? A_BOLD : 0, false);
          }
          if (en) {
            cur = saWin->putStr(!en->isEnabled() ? "*Disabled* " : "Enabled ", Point::null, 5, en->isEnabled() ? A_BOLD : 0, false);
          }
          cur = Point(cur.y()+1, 0);
        }
      }
      
    }

    // update the log window
    if (gotlog) {
      lock();
      gotlog = false;
      unlock();      
      StringList lines;
      int numlines = Log::tail(lines, logWin->rows());
      werase(*logWin);
      StringList::iterator it = lines.begin();
      for (int i = 0; it != lines.end() && i < numlines && i < logWin->rows(); ++i, ++it)
        logWin->putStr(*it, Point(i, 0), 6, A_BOLD, false);
    }

    // update status bar
    if (ch != ERR) {
      werase(*statusBar);
      statusBar->putStr("Pressed: ", Point(0, 0), 2, 0, false);
      if (prest.length()) {
        statusBar->putStr(prest, Point::null, 3, A_BOLD, false);
      } else 
        statusBar->putCh(ch, Point::null, 3, A_BOLD, false);
    }

    masterWin->refresh();
  }
  cleanup();
  isrunning = false;
}

//static
void ConsoleUI::logCallback(void *arg, const String & logentry)
{
  (void)logentry;
  ConsoleUI *self = reinterpret_cast<ConsoleUI *>(arg);
  self->lock();
  self->gotlog = true;
  //self->logQ.push_back(logentry);
  self->unlock();
}

void ConsoleUI::lock()
{
  pthread_mutex_lock(&mut);
}
void ConsoleUI::unlock()
{
  pthread_mutex_unlock(&mut);
}

String uptimeFMT(double secs)
{
  String ret = "";
  unsigned long isec = static_cast<unsigned long>(secs);
  unsigned h = isec / 60 / 60;
  unsigned m = (isec - h*60*60) / 60;
  unsigned s = isec  - (h*60*60 + m*60);
  unsigned frac = static_cast<unsigned>(100.0 * (secs - static_cast<double>(static_cast<unsigned long>(secs))));
  unsigned d = h / 24;
  h %= 24;
  char buf[64];
  ::snprintf(buf, 63, "%03u days %02u hrs %02u mins %02u.%02u secs", d, h, m, s, frac);
  buf[63] = 0;
  ret = buf;
  return ret;
}
