#include "K_DAQTask.h"

namespace Kernel {

DAQTask::DAQTask(const char *name_in,
                 unsigned rate_hz,
                 unsigned minor, unsigned subdev, 
                 unsigned chan_mask, unsigned range, unsigned aref, DataLogger *l, const int *override_min, const int *override_max)
  : Thread(), pleaseStop(false), dev(0), subdev(subdev), chan_mask(chan_mask), range(range), aref(aref), changed_mask(0), req_chanmask(0), logger(l)
{
  for(unsigned i = 0; i < MAX_CHANS; ++i) datalogging[i] = -1;
  namestr = Strdup(name_in);
  rate = rate_hz;
  if (rate) timer.setPeriod(1000000000 / rate_hz);
  char buf [] = "/dev/comediX";
  buf[Strlen(buf)-1] = '0' + minor;
  dev = comedi_open(buf);
  if (!dev) return;
  if (((int)subdev) >= comedi_get_n_subdevices(dev))  {    
    uninitComedi();
    return;
  }
  nchans = comedi_get_n_channels(dev, subdev);
  unsigned mymask = 0;
  for (unsigned i = 0; i < nchans && i < MAX_CHANS; ++i)
    mymask |= chan_mask & (0x1<<i);
  this->chan_mask = mymask;
  if (!this->chan_mask) {
    uninitComedi();
    return;
  }
  int & t = subd_type = comedi_get_subdevice_type(dev, subdev);
  is_read =  (t == COMEDI_SUBD_AI || t == COMEDI_SUBD_DI || t == COMEDI_SUBD_DIO);
  is_write = (t == COMEDI_SUBD_AO || t == COMEDI_SUBD_DO || t == COMEDI_SUBD_DIO);
  is_dig = (t == COMEDI_SUBD_DO || t == COMEDI_SUBD_DI || t == COMEDI_SUBD_DIO);
  maxdata = comedi_get_maxdata(dev, subdev, 0);
  comedi_get_krange(dev, subdev, 0, range, &krange);
  need_range_calc = true;
  if (override_min) krange.min = *override_min;
  if (override_max) krange.max = *override_max;
}

lsampl_t DAQTask::getSample(unsigned chan) const
{
  if (chan >= nchans || !(chan_mask&(0x1<<chan))) return 0;
  lsampl_t ret;
  if (!rate) { // passive task, so signal it to acquire 
    mut.lock();
    req_chanmask |= 0x1<<chan;
    if (running()) { cond_req.signal();  cond_reply.wait(mut); }
    ret = scan[chan];
    mut.unlock();
  } else {
    // note in active daq task, we don't acquire the lock for sample reads
    // as an optimization -- hopefully this will be OK since I understand
    // word or halfword reads/write are atomic on most architectures
    ret = scan[chan];
  }
  return ret;
}

unsigned DAQTask::getScan(lsampl_t *buf, unsigned num_in_buf) const
{
  mut.lock();
  if (!rate) {
    // asynch task is passive, so signal it to acquire a sample(s)
    unsigned mask = chan_mask;
    for (unsigned i = 0; mask && i < num_in_buf && i < MAX_CHANS; ++i) {
      unsigned ch = Ffs(mask);
      req_chanmask |= 0x1<<ch;
      mask &= ~(0x1<<ch);
    }
    if (running()) { cond_req.signal();  cond_reply.wait(mut); }
  }
  unsigned mask = chan_mask, n = 0;
  while(mask && n < num_in_buf && n < MAX_CHANS) {
    unsigned b = Ffs(mask);
    mask &= ~(0x1<<b); // clear bit
    buf[n++] = scan[b];
  }
  mut.unlock();
  return n;
}

bool DAQTask::putSample(unsigned chan, lsampl_t sample) 
{
  if (!is_write || chan >= nchans || !(chan_mask&(0x1<<chan))) return false;
  mut.lock();  
  scan[chan] = sample;
  changed_mask |= 0x1<<chan;
  if (!rate) { // asynch task is not 'running', so signal it to do stuff 
      req_chanmask |= 0x1<<chan;
      if (running()) { cond_req.signal();  cond_reply.wait(mut); }
  }
  mut.unlock();
  return true;
}


bool DAQTask::putScan(const lsampl_t *buf, unsigned num_in_buf) 
{
  if (!is_write) return false;
  mut.lock();
  unsigned mask = chan_mask, n = 0;
  while(mask && n < num_in_buf && n < nchans) {
    unsigned b = Ffs(mask);
    mask &= ~(0x1<<b); // clear bit
    scan[b] = buf[n++];
    changed_mask |= 0x1<<b;
  }
  if (!rate) { // asynch task is not 'running', so signal it to do stuff 
      req_chanmask |= changed_mask;
      if (running()) { cond_req.signal();  cond_reply.wait(mut); }
  }
  mut.unlock();
  return n;
}

void DAQTask::uninitComedi()
{
  if (dev) comedi_close(dev);
  dev = 0;
  is_read = is_write = false;
}

DAQTask::~DAQTask()
{
  stop();
  uninitComedi();
  if (namestr) Strfree(namestr);
  namestr = 0;
}

void DAQTask::stop() 
{ 
  pleaseStop = true; 
  if (!rate) {
    cond_req.signal(); 
    cond_reply.broadcast();
    join();
  } else
    Thread::stop();
}

void DAQTask::run()
{
  if (rate) { // periodic mode
    timer.reset();
    while (!pleaseStop) {
      mut.lock();

      doIO(chan_mask);

      //RTPrint("%s %u chans\n", name(), n);
      mut.unlock();
      timer.waitNextPeriod();
    }
  } else { // passive mode, just a simple multiplexer
    Thread::setCancelState(false);
    mut.lock();
    while (!pleaseStop) {
      if (req_chanmask &= chan_mask) {
        doIO(req_chanmask);
        req_chanmask = 0;
        cond_reply.broadcast();
      }
      cond_req.timedWait(mut, Timer::absTime() + 250000000); // 250ms timeout
    }
    mut.unlock();
  }
}

void DAQTask::setDIOInput() 
{
  if (subd_type != COMEDI_SUBD_DIO) return;
  mut.lock();
  is_read = true; is_write = false;
  configDIOChans();
  mut.unlock();
}

void DAQTask::setDIOOutput() 
{ 
  if (subd_type != COMEDI_SUBD_DIO) return;
  mut.lock();
  is_write = true; is_read = false;
  configDIOChans();
  mut.unlock();
}

void DAQTask::configDIOChans()
{
  if (subd_type != COMEDI_SUBD_DIO) return;
  unsigned config_type = 0;
  bool doit = false;
  if (!is_read && is_write) {
    doit = true;
    config_type = COMEDI_OUTPUT;
  } else if (is_read && !is_write) {
    doit = true;
    config_type = COMEDI_INPUT;
  }
  if (doit) {
    unsigned m = chan_mask;
    while (m) {
      unsigned chan = Ffs(m);
      m &= ~(0x1<<chan); // clear bit
      if (chan > nchans) continue;
      comedi_dio_config(dev, subdev, chan, config_type);
    }
  }
  changed_mask = 0;
}

void DAQTask::doIO(unsigned mask)
{
      const unsigned mask_backup = mask;

      if (is_dig) { // DIGITAL I/O -- use bitfields for speed
        unsigned bits = 0;
        // first figure out write bits
        while(mask) {
          unsigned ch = Ffs(mask);
          mask &= ~(0x1<<ch); // clear bit
          if (ch >= nchans) continue;
          // if it's nonzero, set bit
          if (scan[ch])  bits |= 0x1 << ch;
        }
        // next do simultaneous read/write
        int ret = comedi_dio_bitfield(dev, subdev, changed_mask, &bits);
        if (ret < 0) {
          RTPrint("DAQTask *Error* returned from DIO bitfield call: %d\n", ret);
        }
        // now, retrieve values back into the scan
        mask = mask_backup;
        while(mask) {
          unsigned ch = Ffs(mask);
          mask &= ~(0x1<<ch); // clear bit
          if (ch >= nchans) continue;
          // if it's nonzero, set bit
          scan[ch] = (bits & (0x1 << ch)) ? 1 : 0;
        }
      } else { // ANALOG I/O
        while(mask) {
          unsigned ch = Ffs(mask);
          mask &= ~(0x1<<ch); // clear bit
          if (ch >= nchans) continue;

          // note that in most usual uses of this class, only one of read or write is performed

          // do write first
          if ( is_write && comedi_data_write(dev, subdev, ch, range, aref, scan[ch]) < 1 ) 
            RTPrint("DAQTask *Error* in data write chan %u\n", ch);
          // then do read 
          if ( is_read && comedi_data_read(dev, subdev, ch, range, aref, const_cast<lsampl_t *>(scan + ch)) < 1 ) 
            RTPrint("DAQTask *Error* in data read chan %u\n", ch);
        }
      }

      // do logging
      doDataLogging(mask_backup);
      changed_mask = 0; // clear our changed mask
}

void DAQTask::doDataLogging(unsigned mask)
{
  if (need_range_calc) {
    // this needs to happen only once in RT
    range_min = krange.min * 1e-6;
    range_max = krange.max * 1e-6;
    need_range_calc = false;
  }
  while (logger && mask) {
    unsigned ch = Ffs(mask);
    mask &= ~(1<<ch);    
    // 1. log the channel if logging is enabled for it *and*
    // 2. either the channel is a read channel (in which case we always log) 
    //    or the channel is a write channel *and* it changed since 
    //    the last time
    if (ch < nchans && ch < MAX_CHANS && datalogging[ch] > -1 // is loggin enabled?
        && (is_read || (is_write && (0x1<<ch)&changed_mask)) ) { // did it change or is it a read channel?
      char metabuf[8];
      double v = (double(scan[ch]) / double(maxdata)) * (range_max-range_min) + range_min;
      Snprintf(metabuf, 8, "RawCh%02u", ch);
      metabuf[7] = 0;
      logger->log(datalogging[ch], v, metabuf);
    }
  }
}

void DAQTask::setDataLogging(unsigned chan, int id) 
{ 
  if (chan < MAX_CHANS) {
    MutexLocker l(mut);
    datalogging[chan] = id; 
  }
}
int DAQTask::getDataLogging(unsigned chan) const 
{ 
  if (chan < MAX_CHANS) {
    MutexLocker l(mut);
    return datalogging[chan]; 
  }
  return -1; 
}

}
