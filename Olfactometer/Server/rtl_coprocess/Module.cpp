#include "Module.h"
#include "SysDep.h"
#include "RTFifo.h"
#include "Cmd.h"
#include "K_PIDFlowController.h"
#include "K_PWMValve.h"
#include "RTShm.h"
#include "Shm.h"
#include "K_DAQTask.h"
#include "Mutex.h"
#include "K_DataLogger.h"
#include "K_DataLogable.h"

class CmdFifo: public RTFifo
{
public:
  CmdFifo(const char *n) 
    : RTFifo(sizeof(struct Cmd)), cmd_count(0), delname(true), c(0)
  {
    name = Strdup(n);
    if (!name) name = "unnamed fifo", delname = false;
  }
  ~CmdFifo();
  unsigned long cmd_count;
protected:
  int handler();
private:
  const char *name;
  bool delname;
  Cmd *c;
  Mutex mut;
};

CmdFifo::~CmdFifo()
{
  if (cmd_count) 
    Msg("%s destroyed after %lu commands processed\n", name, cmd_count);
  
  if (name && delname) Strfree(name), name = 0;
  if (c) delete c, c = 0;
}

static void DoCmd(Cmd *); 

int CmdFifo::handler() 
{
    struct F { F() { ModuleIncUseCount(); } ~F() { ModuleDecUseCount(); } } modCtr; // increment the use count while in this scope
    MutexLocker mutlocker(mut); // auto-unlocks on scope exit
    ++cmd_count;
    //RTPrint("%s handler called!\n", name);
    int num = fionread();
    if (num) {
        if (num == sizeof(struct Cmd)) {
          if (!c && !(c = new Cmd)) {
            Error("CmdFifo::handler() -- failed to allocate Cmd buffer\n");
            return -1;
          }
          if ( !c->readFifo(this) ) {
            Error("Cmd::readFifo returned error\n");
            return -1;
          }

          DoCmd(c);

        } else {
          Error("number of bytes available in fifo %s is %d != %d\n", name, num, sizeof(Cmd));
          return -1;
        }
    } else {
      //RTPrint("Nothing to read!\n");
    }
    return 0;
}

#define HANDLE_MAX (sizeof(unsigned long)*8)
static RTFifo *getfifo = 0, *putfifo = 0;
static unsigned long pidsAllocated = 0; ///< bitmask for below array
static Kernel::PIDFlowController *pids[HANDLE_MAX] = { 0 };
static unsigned long pwmsAllocated = 0; ///< bitmask for below array
static Kernel::PWMValve *pwms[HANDLE_MAX] = { 0 };
static Kernel::DAQTask *daqs[HANDLE_MAX] = { 0 };
static unsigned long daqsAllocated = 0;
static int ReservePID();
static void FreePID(int idx);
static int ReservePWM();
static void FreePWM(int idx);
static int ReserveDAQ();
static void FreeDAQ(int idx);
static void DestroyAllPWMPIDDAQ();
static Kernel::DAQTask *FindDAQ(const char *name);
static RTShm<OlfCoprocessShm> *rt_shm = 0;
static OlfCoprocessShm *shm = 0;
static Kernel::DataLogger *dataLogger = 0;

int ModuleInit(void)
{
  Msg("Initializing...\n");

  // fifos
  getfifo = new CmdFifo("getcmdfifo");
  putfifo = new CmdFifo("putcmdfifo");
  dataLogger = new Kernel::DataLogger;
  if (!getfifo || !putfifo || !dataLogger || !RTFifo::makeUserPair(getfifo, putfifo)) {
    ModuleCleanup();
    return 1;
  }
  Msg("Fifos created --  get: %d  put: %d datalogging %d\n", static_cast<int>(*getfifo), static_cast<int>(*putfifo), static_cast<int>(*dataLogger));

  // shm
  rt_shm = new RTShm<OlfCoprocessShm>;
  if (!rt_shm || !rt_shm->attach(OlfCoprocessShm_NAME) || !(shm = *rt_shm) ) {
    Error("Error attaching to SHM region \""OlfCoprocessShm_NAME"\"\n");
    ModuleCleanup();
    return 1;
  }
  shm->magic = OlfCoprocessShm_MAGIC;
  shm->cmd_fifo = *getfifo;
  shm->datalog_fifo = *dataLogger;
  Msg("%s attached at: 0x%p\n", OlfCoprocessShm_NAME, shm);

  getfifo->enableHandler();
  return 0;
}

void ModuleCleanup(void)
{
  DestroyAllPWMPIDDAQ();
  if (getfifo) delete getfifo, getfifo = 0;
  if (putfifo) delete putfifo, putfifo = 0;
  if (dataLogger) delete dataLogger, dataLogger = 0;
  if (rt_shm) delete rt_shm, rt_shm = 0;
  Msg("Cleaned up!\n"); 
}


static void getDataLogableBits(Cmd *c, DataLogable *d);
static void setDataLogableFromBits(Cmd *c, DataLogable *d);

void DoCmd(Cmd *c)
{
  c->status = Cmd::Ok;

  switch (c->cmd) {
  case Cmd::Create: 
    if (c->object == Cmd::PID) {
      int idx = ReservePID();
      
      Kernel::DAQTask *daqin = FindDAQ(c->daqname_in), *daqout = FindDAQ(c->daqname_out);
      if (idx < 0) {
        Error("Too many PID flow controllers already allocated!\n");
        c->status = Cmd::Error;
      } else if ( !(pids[idx] = new Kernel::PIDFlowController(dataLogger, c->datalog.id, c->pidParams, daqin, daqout)) ) {
        FreePID(idx);
        Error("Failed memory allocation for PID flow controller!\n");
        c->status = Cmd::Error;        
      } else if ( !pids[idx]->isOk() ) {
        FreePID(idx);
        Error("Failed to construct PID flow controller -- are comedi params correct?\n");
        c->status = Cmd::Error;        
      } else {
        c->handle = idx;
      }
    } else if (c->object == Cmd::PWM) {
      int idx = ReservePWM();
      if (idx < 0) {
        Error("Too many PWM valves already allocated!\n");
        c->status = Cmd::Error;
      }
      Kernel::DAQTask *daq = FindDAQ(c->daqname_out);
      if ( !(pwms[idx] = new Kernel::PWMValve(dataLogger, c->datalog.id, c->pwmParams, daq)) ) {
        FreePWM(idx);
        Error("Failed memory allocation for PWM valve!\n");
        c->status = Cmd::Error;        
      } else if ( !pwms[idx]->isOk() ) {
        FreePWM(idx);
        Error("Failed to construct PWM valve -- are comedi params correct?\n");
        c->status = Cmd::Error;        
      } else {
        c->handle = idx;
      }
    } else if (c->object == Cmd::DAQ) {
      int idx = ReserveDAQ();
      if (idx < 0) {
        Error("Too many DAQ tasks already allocated!\n");
        c->status = Cmd::Error;
      }
      int *override_min = 0, *override_max = 0;
      if (c->daqParams.use_override) 
        override_min = &c->daqParams.override_min, override_max = &c->daqParams.override_max;
      if ( !(daqs[idx] = new Kernel::DAQTask(c->daqname_in,
                                             c->daqParams.rate_hz,
                                             c->daqParams.minor, 
                                             c->daqParams.subdev,
                                             c->daqParams.chanmask,
                                             c->daqParams.range, 
                                             c->daqParams.aref,
                                             dataLogger,
                                             override_min, 
                                             override_max)) ) {
        FreeDAQ(idx);
        Error("Failed memory allocation for DAQ task!\n");
        c->status = Cmd::Error;        
      } else if ( !daqs[idx]->isOk() ) {
        FreeDAQ(idx);
        Error("Failed to construct DAQ task -- are params correct?\n");
        c->status = Cmd::Error;        
      } else {
        c->handle = idx;

        // if they specified a DIO input/output mode.. note this has no effect
        // on non-dio subdevices
        if (c->daqParams.dioMode == Cmd::Input)
          daqs[idx]->setDIOInput();
        else if (c->daqParams.dioMode == Cmd::Output)
          daqs[idx]->setDIOOutput();

      }
    } else { // object != PID or PWM
      Error("Invalid object specified in RTFifo Command\n");
      c->status = Cmd::Error;        
    }
    break;
  case Cmd::Destroy:
    if (c->object == Cmd::PID) {
      if (c->handle >= HANDLE_MAX || !pids[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        FreePID(c->handle);
      }
    } else if (c->object == Cmd::PWM) {
      if (c->handle >= HANDLE_MAX || !pwms[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        FreePWM(c->handle);
      }
    } else if (c->object == Cmd::DAQ) {
      if (c->handle >= HANDLE_MAX || !daqs[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        FreeDAQ(c->handle);
      }
    } else { // object != PID or PWM
      Error("Invalid object specified in RTFifo Command\n");
      c->status = Cmd::Error;        
    }
    break;
  case Cmd::Start:
    if (c->object == Cmd::PID) {
      if (c->handle >= HANDLE_MAX || !pids[c->handle]) {
        Error("Invalid PID handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PIDFlowController *p = pids[c->handle];
        p->start();
      }
    } else if (c->object == Cmd::PWM) {
      if (c->handle >= HANDLE_MAX || !pwms[c->handle]) {
        Error("Invalid PWM handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PWMValve *p = pwms[c->handle];
        p->start();
      }
    } else if (c->object == Cmd::DAQ) {
      if (c->handle >= HANDLE_MAX || !daqs[c->handle]) {
        Error("Invalid DAQ handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::DAQTask *d = daqs[c->handle];
        d->start();
      }
    } else { // object != PID or PWM or DAQ
      Error("Invalid object specified in RTFifo Command\n");
      c->status = Cmd::Error;        
    }
    break;
  case Cmd::Stop:
    if (c->object == Cmd::PID) {
      if (c->handle >= HANDLE_MAX || !pids[c->handle]) {
        Error("Invalid PID handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PIDFlowController *p = pids[c->handle];
        p->stop();
      }
    } else if (c->object == Cmd::PWM) {
      if (c->handle >= HANDLE_MAX || !pwms[c->handle]) {
        Error("Invalid PWM handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PWMValve *p = pwms[c->handle];
        p->stop();
      }
    } else if (c->object == Cmd::DAQ) {
      if (c->handle >= HANDLE_MAX || !daqs[c->handle]) {
        Error("Invalid DAQ handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::DAQTask *d = daqs[c->handle];
        d->stop();
      }
    } else { // object != PID or PWM
      Error("Invalid object specified in RTFifo Command\n");
      c->status = Cmd::Error;        
    }
    break;
    case Cmd::Modify:
    if (c->object == Cmd::PID) {
      if (c->handle >= sizeof(long)*8 || !pids[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PIDFlowController *p = pids[c->handle];
        bool ret = p->setParams(c->pidParams);
        if (!ret) {
          Error("Error setting PID params to for PID handle %lu\n",  c->handle);
          c->status = Cmd::Error;
        } else 
          setDataLogableFromBits(c, p);
      }
    } else if (c->object == Cmd::PWM) {
      if (c->handle >= sizeof(long)*8 || !pwms[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PWMValve *p = pwms[c->handle];
        bool ret = p->setParams(c->pwmParams);
        if (!ret) {
          Error("Error setting PWM params to for PWM handle %lu\n",  c->handle);
          c->status = Cmd::Error;
        } else
          setDataLogableFromBits(c, p);
      }
    } else if (c->object == Cmd::DAQ) {
      if (c->handle >= HANDLE_MAX || !daqs[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::DAQTask *d = daqs[c->handle];
        if (c->daqGetPut.doit) { // they wanted to put a sample
          bool ret = d->putSample(c->daqGetPut.chan, c->daqGetPut.sample);
          if (!ret) {
            Error("Error status when writing to daq task %lu chan %u\n", 
                  c->handle, c->daqGetPut.chan);
            c->status = Cmd::Error;
          }
        } else { // they wanted to set datalog stuff
          for (unsigned i = 0; i < d->numChans() && i < 32; ++i) {
            int id = c->datalog.ids[i];            
            if (!(c->datalog.mask & 1<<i)) id = -1;
            d->setDataLogging(i, id);
            Debug("Set data logging ch %u id %d\n", i, id);
          }
        }
      }
    } else { // object != PID or PWM
      Error("Invalid object specified in RTFifo Command\n");
      c->status = Cmd::Error;        
    }
    break;
  case Cmd::Query:
    if (c->object == Cmd::PID) {
      if (c->handle >= HANDLE_MAX || !pids[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PIDFlowController *p = pids[c->handle];
        c->pidParams = p->getParams();
        getDataLogableBits(c, p);
      }
    } else if (c->object == Cmd::PWM) {
      if (c->handle >= HANDLE_MAX || !pwms[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::PWMValve *p = pwms[c->handle];
        c->pwmParams = p->getParams();
        getDataLogableBits(c, p);
      }
    } else if (c->object == Cmd::DAQ) {
      if (c->handle >= HANDLE_MAX || !daqs[c->handle]) {
        Error("Invalid handle %lu\n", c->handle);
        c->status = Cmd::Error;
      } else {
        Kernel::DAQTask *d = daqs[c->handle];
        if (c->daqGetPut.doit) { // they wanted a sample
          c->daqGetPut.sample = d->getSample(c->daqGetPut.chan);
        } else { // they wanted datalog id stuff
          c->datalog.mask = 0;
          for (unsigned i = 0; i < d->numChans(); ++i) {
            int id = d->getDataLogging(i);
            c->datalog.ids[i] = id;
            if (id > -1) c->datalog.mask |= 1<<i;
            Debug("Get data logging ch %u id %d\n", i, id);
          }         
        }
      }
    } else { // object != PID or PWM
      Error("Invalid object specified in RTFifo Command\n");
      c->status = Cmd::Error;        
    }
    break;
  case Cmd::DestroyAll:
    DestroyAllPWMPIDDAQ();
    break;
  default:
    Error("Got invalid command from userspace.\n");
    c->status = Cmd::Error;
    break;
  }

  if ( !c->writeFifo(putfifo) ) {
    Error("Error writing userspace reply to putfifo\n");
  }
}

static int ReservePID()
{
  int ret = Ffz(pidsAllocated);
  if (ret >= 0) 
    pidsAllocated |= 0x1<<ret; // set/mark as allocated
  
  return ret;
}

static void FreePID(int idx)
{
  if (pids[idx]) delete pids[idx];
  pids[idx] = 0;
  pidsAllocated &= ~(0x1<<idx); // clear allocated  
}

static int ReservePWM()
{
  int ret = Ffz(pwmsAllocated);
  if (ret >= 0) 
    pwmsAllocated |= 0x1<<ret; // set/mark as allocated
  
  return ret;
}

static void FreePWM(int idx)
{
  if (pwms[idx]) delete pwms[idx];
  pwms[idx] = 0;
  pwmsAllocated &= ~(0x1<<idx); // clear allocated  
}


static int ReserveDAQ()
{
  int ret = Ffz(daqsAllocated);
  if (ret >= 0) 
    daqsAllocated |= 0x1<<ret; // set/mark as allocated
  
  return ret;
}

static void FreeDAQ(int idx)
{
  if (daqs[idx]) delete daqs[idx];
  daqs[idx] = 0;
  daqsAllocated &= ~(0x1<<idx); // clear allocated  
}

static void DestroyAllPWMPIDDAQ()
{
  while(pidsAllocated) {
    int idx = Ffs(pidsAllocated);
    FreePID(idx);
  }
  while(pwmsAllocated) {
    int idx = Ffs(pwmsAllocated);
    FreePWM(idx);
  }
  while(daqsAllocated) {
    int idx = Ffs(daqsAllocated);
    FreeDAQ(idx);
  }
}

static Kernel::DAQTask * FindDAQ(const char *name)
{
  if (!name || !name[0]) return 0;
  unsigned da = daqsAllocated;
  while(da) {
    int idx = Ffs(da);
    da &= ~(0x1<<idx);
    if (daqs[idx] && !Strcmp(daqs[idx]->name(), name)) return daqs[idx];
  }
  return 0;  
}

static void getDataLogableBits(Cmd *c, DataLogable *d)
{
  if (!c || !d) return;
  DataLogable::DataType arr [] = { DataLogable::Cooked, DataLogable::Raw, DataLogable::Other };
  c->datalog.mask = 0;
  for (int i = 0; i < 3; ++i) 
    if (d->loggingEnabled(arr[i]))
      c->datalog.mask |= unsigned(arr[i]);
}

static void setDataLogableFromBits(Cmd *c, DataLogable *d)
{
  if (!c || !d) return;
  DataLogable::DataType arr [] = { DataLogable::Cooked, DataLogable::Raw, DataLogable::Other };
  for (int i = 0; i < 3; ++i) 
    d->setLoggingEnabled(unsigned(arr[i]) & c->datalog.mask, arr[i]);
}
