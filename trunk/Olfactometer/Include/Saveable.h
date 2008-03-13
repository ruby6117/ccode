#ifndef Saveable_H
#define Saveable_H

class Saveable
{
public:
  virtual ~Saveable() {}
  virtual bool save() = 0;
};

#endif
