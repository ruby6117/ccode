#ifndef Common_H
#define Common_H

#include "Version.h"
#define PRG_NAME "CSHL Olfactometer"

#ifdef __cplusplus
#  include <list>
#  include <sstream>
#  include <string>

class String;
class StringList;
typedef std::list<std::string> StrList;

class String : public std::string
{
public:
  String() {}
  String(const std::string &s) : std::string(s) {}
  String(const char *s) : std::string(s) {}

  //String & operator=(const StringList &rhs) { return *this = join(rhs, " "); }
  operator const char *() const { return c_str(); }
  
  template<typename T> String operator+(const T & t) const {
    return static_cast<const std::string &>(*this) + Str(t);
  }

private:
  template <typename N> N Num(bool * ok = 0) const { return Num<N>(*this, ok); }
  template<typename N> static N Num(const String & s, bool *ok = 0)
  {
    N ret = 0;
    std::stringstream ss(s);
    ss >> ret;
    if (ok) { *ok = !!ss; }
    return ret;
  }

public:

  template<typename T> static String Str(const T &t)
  {
    std::stringstream ss;
    ss << t;
    return ss.str();
  }

  unsigned int toUInt(bool *ok = 0) const { return toUInt(*this, ok); }
  int toInt(bool *ok = 0) const { return toInt(*this, ok); }  
  short toShort(bool *ok = 0) const { return toShort(*this, ok); }
  unsigned short toUShort(bool *ok = 0) const { return toUShort(*this, ok); }
  double toDouble(bool *ok = 0) const { return toDouble(*this, ok); }

  static unsigned int toUInt(const String &s, bool *ok = 0) { return Num<unsigned int>(s, ok); }
  static int toInt(const String &s, bool *ok = 0) { return Num<int>(s, ok); }
  static unsigned short toUShort(const String &s, bool *ok = 0) { return Num<unsigned short>(s, ok); }
  static short toShort(const String &s, bool *ok = 0) { return Num<short>(s, ok); }
  static double toDouble(const String &s, bool *ok = 0) { return Num<double>(s, ok); }
 
  String lower() const;
  String upper() const;
  bool startsWith(const String &) const;
  bool endsWith(const String &) const;
  bool contains(const String &) const;

  String &trimWS() { return *this = trimWS(*this); }
  StrList split(const String &re) const { return split(*this, re); }
  String &replace(const String & re, const String &replacement) { return *this = replace(*this, re, replacement); }
  int search(const String &regex, unsigned startpos = 0) const { return search(*this, regex, startpos); }
  static String trimWS(const String & s);
  static StrList split(const String & str, const String &re, bool permitEmptyTokens = false);
  static String join(const StringList &, const String &delim);  
  static String replace(const String &, const String &re, const String & repl_fmt);
  /// regex search -- returns the index of the first occurrence of a regex 
  /// or negative if not found
  static int search(const String &, const String & regex, unsigned startpos = 0);
};

class StringList : public std::list<String>
{
public:
  StringList() : std::list<String>() {}
  StringList(const std::list<std::string> &in) { *this = in; }
  StringList & operator=(const StringList & in) {
    std::list<String>::operator=(in);
    return *this;
  }
  StringList &operator=(const std::list<std::string> &in) 
  {
    clear();
    for(std::list<std::string>::const_iterator it = in.begin(); it != in.end(); ++it) push_back(*it);
    return *this;
  }
};

#define Str(x) String::Str(x)
#define ToInt(x) String::toInt(x)
#define ToUInt(x) String::toUInt(x)
#define ToShort(x) String::toShort(x)
#define ToUShort(x) String::toUShort(x)
#define ToDouble(x) String::toDouble(x)


#endif /* __cpluplus */

#endif /* Common_H */
