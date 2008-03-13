#include "Common.h"
#include <string>
#include <boost/regex.hpp>
#include <algorithm>

//static
StrList String::split(const String & str, const String &re, bool permitEmpty)
{
  using namespace boost; using namespace std;
  const regex rex(re);
  sregex_token_iterator i(str.begin(), str.end(), rex, -1), j;
  StrList ret;
  while (i != j) {
    std::string tok = *i++;
    if (!tok.length() && !permitEmpty) continue;
    ret.push_back(tok);
  }
  return ret;
}

//static
String String::trimWS(const String &str)
{
  String ret = boost::regex_replace(str, boost::regex("^\\s*"), "");
  ret = boost::regex_replace(ret, boost::regex("\\s*$"), "");
  return ret;
}

//static
String String::join(const StringList &sl, const String & delim)
{
  String ret;
  StringList::const_iterator it;
  for (it = sl.begin(); it != sl.end(); ++it) {
    if (it != sl.begin()) ret += delim;
    ret += *it;
  }
  return ret;
}

//static 
String String::replace(const String &str, const String &re, const String & fmt)
{
  return boost::regex_replace(str, boost::regex(re), fmt);
}

/// regex search -- returns the index of the first occurrence of a regex 
/// or negative if not found
//static 
int String::search(const String &str, const String & regex, unsigned startpos)
{
  if (startpos >= str.length()) return -1;
  boost::match_results<std::string::const_iterator> results;
  if ( !boost::regex_search(str.substr(startpos), results, boost::regex(regex))  || results.empty() ) return -1;
  return results.position();
}

#include <ctype.h>

String String::lower() const
{
  String ret = *this;
  std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
  return ret;
}
String String::upper() const
{
  String ret = *this;
  std::transform(ret.begin(), ret.end(), ret.begin(), ::toupper);
  return ret;
}

bool String::startsWith(const String &str) const
{
  return find(str) == 0;
}

bool String::endsWith(const String &str) const
{
  return rfind(str) == length()-str.length();
}

bool String::contains(const String &str) const
{
  return find(str) != npos;
}

