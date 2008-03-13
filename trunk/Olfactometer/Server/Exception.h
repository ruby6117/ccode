#ifndef Exception_H
#define Exception_H

#include <string>

class Exception
{
public:
  Exception(const std::string & msg) : msg(msg) {}
  const std::string & what() const { return msg; }
protected:
  std::string msg;
};

#endif
