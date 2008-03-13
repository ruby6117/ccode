#ifndef Lockable_H
#define Lockable_H

class Lockable
{
public:
  Lockable();
  virtual ~Lockable();

  void lock();
  void unlock();

private:
  struct Private;
  Private *p;
};

#endif
