#ifndef StartStoppable_H
#define StartStoppable_H

class StartStoppable
{
public:
  StartStoppable() : started(false) {}
  virtual ~StartStoppable() {}

  virtual bool start() { return false; }
  virtual bool stop() { return false; }
  virtual bool isStarted() const { return started; }
  virtual bool isStopped() const { return !started; }
protected:
  void setStarted(bool b) { started = b; }
  void setStopped(bool b) { started = !b; }
private:
  bool started;
};

#endif
