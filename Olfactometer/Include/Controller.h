#ifndef Controller_H
#define Controller_H

#include <vector>

class Controller
{
public:
  virtual ~Controller() {}

  bool setControlParams(const std::vector<double> & params) 
  {
    if (params.size() < numControlParams()) return false;
    return controller_SetControlParams(params);
  }

  virtual std::vector<double> controlParams() const = 0;
  virtual unsigned numControlParams() const = 0;

protected:
  virtual bool controller_SetControlParams(const std::vector<double> &) = 0;
};
#endif
