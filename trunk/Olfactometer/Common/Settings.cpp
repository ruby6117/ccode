/*
 * Copyright (C) 2006 Calin Culianu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYRIGHT file); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA, or go to their website at
 * http://www.gnu.org.
 */
#include <ios>
#include <iterator>
#include <set>
#include <map>
#include <sstream>
#include "Common.h"
#include "Settings.h"


#ifndef __KERNEL__
    // one would *hope* 256kb lines are enough.  We need lines this long because some config file lines contain tons of data
#  define LINESZ (256*1024)
#else
#  define LINESZ 4096
#endif

Settings::Settings() 
{ init(); };

Settings::Settings(const std::string & filename)
{
  init();
  setFile(filename);
}

void Settings::init()
{
  _currentSection = "";
  masterSettings = 0;
}

Settings::~Settings()
{
}


unsigned long
Settings::length() const
{
  unsigned long ret = 0;
  theFile.close();
  theFile.open(fname.c_str(), std::fstream::in);
  if (theFile) {
    std::fstream::off_type oldpos = theFile.tellg();
    theFile.seekg(0, std::fstream::end);
    ret = theFile.tellg();
    theFile.seekg(oldpos, std::fstream::beg);    
  }
  return ret;
}

void
Settings::setFile(const std::string &fileName)
{
  theFile.close();
  fname = fileName;
  theFile.open(fname.c_str(), std::fstream::in);
  theFile.clear();
}

std::string 
Settings::getFile() const
{  
  return fname;
}

std::string
Settings::get(const std::string & key) const
{
  return get(_currentSection, key);
}

void
Settings::put(const std::string & key, const std::string & value)
{
  put(_currentSection, key, value);
}

std::string
Settings::get(const std::string & section, const std::string & key) const
{
  SettingsMap::const_iterator sec_it = settingsMap.find(section);

  if (sec_it != settingsMap.end() && (sec_it->second.count(key) > 0) )
    return sec_it->second.find(key)->second;

  return std::string();
}

void
Settings::put(const std::string & section, const std::string & key, const std::string & value)
{
  settingsMap[section][key] = value;
  dirtySettings[section].insert(key);
}

Settings::Section Settings::getSection(const std::string & section_name) const
{
  if (settingsMap.count(section_name) > 0)
    return settingsMap.find(section_name)->second;
  return Section();
}

std::list<std::string> Settings::sections() const
{
  SettingsMap::const_iterator it;
  std::list<std::string> ret;

  for (it = settingsMap.begin(); it != settingsMap.end(); it++) 
    ret.push_back(it->first);
  
  return ret;
}

Settings & Settings::operator<<(const Settings & from)
{
  return merge(from);
}

Settings & Settings::merge(const Settings & from)
{
  std::list<std::string> from_secs = from.sections();
  std::list<std::string>::iterator it;

  for ( it = from_secs.begin(); it != from_secs.end(); it++)
    mergeSection(*it, from.getSection(*it));

  return *this;
}

void Settings::putSection(const std::string & section_name, const Section & section)
{
  settingsMap[section_name] = section;
  for (Section::const_iterator it = section.begin(); it != section.end(); it++)
    dirtySettings[section_name].insert(it->first);
}

void Settings::mergeSection(const std::string & section_name, 
                            const Section & section)
{
  Section::const_iterator it;

  setSection(section_name);
  
  for (it = section.begin(); it != section.end(); it++)
    put(it->first, it->second);
}

bool
Settings::parse(const std::string & bigBuf)
{
  dirtySettings.clear();
  settingsMap.clear();

  if (!bigBuf.length()) {
    theFile.close();
    theFile.open(fname.c_str(), std::fstream::in);
    theFile.clear();
    if (!theFile.is_open()) return false;
    settingsMap = readAll(theFile);
  } else {
    std::stringstream ss(bigBuf);
    settingsMap = readAll(ss);
  }

  if (masterSettings) {
    SettingsMap::const_iterator master;
    Section::const_iterator subMaster;
    std::string section, key, value;

    // pick up the values
    for (master = masterSettings->begin(); master != masterSettings->end(); master++) {
      section = master->first;
      for (subMaster = master->second.begin(); subMaster != master->second.end(); subMaster++) {
          key = subMaster->first;
          value = subMaster->second;
          if ( settingsMap[section][key].length() == 0 ) {
            settingsMap[section][key] = value;
            dirtySettings[section].insert(key);
          }
      }
    }

  } 
  return true;
}

//static
Settings::SettingsMap
Settings::readAll(std::iostream &theStream)
{
    SettingsMap ret;
    std::string matchedLine, thisKey, thisValue, section;
    char lineBuf[LINESZ];

    while ( theStream.getline(lineBuf, sizeof(lineBuf), '\n') ) {
      std::string line (lineBuf);
      matchedLine = testSectionLine(line);
      if ( matchedLine.length() ) {
        section = matchedLine;
        continue;
      }
      matchedLine = testLine(line);
      if ( matchedLine.length() ) {
        parseMatchedLine(matchedLine, thisKey, thisValue);
        ret[section][thisKey] = thisValue;
      }
    }

    return ret;
}

std::string 
Settings::dump() const
{
  std::stringstream outBuf;
  
  for (SettingsMap::const_iterator sm_it = settingsMap.begin(); sm_it != settingsMap.end(); ++sm_it) {
    outBuf << "\n\n[ " << sm_it->first << " ]\n\n";
    for (Section::const_iterator it = sm_it->second.begin(); it != sm_it->second.end(); ++it) {
      outBuf << it->first << " = " << it->second << "\n";
    }
  }
  return outBuf.str();
}

bool
Settings::save()
{
  std::string matchedLine, key, value, outBuf("");
  DirtyKeys::iterator it;
  DirtyMap alreadySavedKeys;
  char linebuf[LINESZ] = {0};

  theFile.close();
  theFile.open(fname.c_str(), std::fstream::in);
  theFile.clear();
  theFile.seekg(0, std::fstream::beg);    
//   std::string theWholeThing(QTextStream(theFile).read());
//   theFile->close();
  
  _currentSection = std::string();
  while ( theFile.getline(linebuf, sizeof(linebuf)) ) {
    std::string line (linebuf);
    if (line.length() == 0) { outBuf += "\n"; continue; }
    if ( (matchedLine = testSectionLine(line)).length() ) {
      // we have a new section so append what's still 'dirty' for this section to the end of the section that is now ending
      int ct = 0;
      for (it = dirtySettings[currentSection()].begin(); it != dirtySettings[currentSection()].end(); it++) {
        outBuf += *it + " = " + settingsMap[currentSection()][*it] + " \n";
        alreadySavedKeys[currentSection()].insert(*it);
        ++ct;
      }
      if (ct) outBuf += "\n"; // add a blank line if we actually appended stuff before the start of this section
      dirtySettings[currentSection()].clear();
      _currentSection = matchedLine;

    } else if ( (matchedLine = testLine(line)).length()) {
      parseMatchedLine(matchedLine, key, value);
      if (dirtySettings[currentSection()].count(key)) {
        line = key + " = " + settingsMap[currentSection()][key];
        dirtySettings[currentSection()].erase(key);
        alreadySavedKeys[currentSection()].insert(key);
      } else if (alreadySavedKeys[currentSection()].count(key) 
                 || value.length()==0) {
        continue; // we already saved this key, so ignore this line with bogus
                  // info on it
      }
    }
    outBuf += line + "\n";
  }

  // if there are still any dirty settings, put what is left over at the end
  DirtyMap::const_iterator ds_it;
  for (ds_it = dirtySettings.begin(); ds_it != dirtySettings.end(); ds_it++) {
    if (ds_it->second.size() == 0) continue;
    std::string section = ds_it->first;
    if (section != currentSection()) outBuf += std::string("\n\n[ ") + section + " ]\n\n";
    for (it = ds_it->second.begin(); it != ds_it->second.end(); it++)
      outBuf += *it + " = " + settingsMap[section][*it] + "\n";
    _currentSection = section;
  }

  // now, commit changes, overwriting file
  theFile.close();
  theFile.open(fname.c_str(), std::fstream::out | std::fstream::trunc);
  if (!theFile.is_open()) return false;
  theFile.clear();
  theFile.write(outBuf.c_str(), outBuf.length());
  if (!theFile) return false;
  theFile.close();
  dirtySettings.clear();
  return true;
}

/* returns a matched substring from line if line matches the lineRE, 
   or otherwise a null std::string */
//static
std::string
Settings::testLine(std::string line) 
{
  simplifyWhiteSpace(line);

  int eq_pos = line.find("=");
  
  if (eq_pos > 0 && eq_pos < static_cast<int>(line.length())
      && line.length() && line[0] != ';') // comment chars are % # and ;
    return line;
  else 
    return std::string();
}

//static
void Settings::simplifyWhiteSpace(std::string &l)
{
  std::string::iterator fwdit;
  //std::string::size_type com;
  std::string::reverse_iterator revit;

  // strip comments NOTE: for now we don't allow comments within a line
  //if ( (com = l.find(';')) != l.npos )   l.erase(com);  

  // strip leading whitespace..
  for (fwdit = l.begin(); fwdit != l.end(); ) {
    if (*fwdit != ' ' && *fwdit != '\t') break;
    fwdit = l.erase(fwdit);
  }

  // strip trailing whitespace..
  for (revit = l.rbegin(); revit != l.rend(); ++revit) {
    if (*revit != ' ' && *revit != '\t') break;
    l.erase(l.length()-1);
  }  
}

std::string
Settings::testSectionLine(std::string line)
{
  std::string ret;

  simplifyWhiteSpace(line);
  
  if (line.begin() != line.end() && line[0] == '[' && *line.rbegin() == ']') {
    ret = line.substr(1,line.length()-2);
    simplifyWhiteSpace(ret);
  }
  
  return ret;  
}

void
Settings::parseMatchedLine(std::string line, 
                           std::string & key, std::string & value) 
{
  simplifyWhiteSpace(line);
  
  int eq_pos = line.find('=');
  if (eq_pos < int(line.length())) {
    key = line.substr(0, eq_pos);
    simplifyWhiteSpace(key);
    value = line.substr(eq_pos+1, line.length() - (eq_pos+1));
    simplifyWhiteSpace(value);
  }
}

Settings::SectionExists::SectionExists(const Settings &s) 
  : ini(&s), missing_ct(0)  {}

bool Settings::SectionExists::operator()(const std::string & s)  
{
  bool ret = true;
  if (ini->getSection(s).empty())  missing.push_back(s), ++missing_ct, ret = false;
  return ret;
}

Settings::KeyExists::KeyExists(const Settings &s, const std::string &sec) 
  : ini(&s), section(sec), missing_ct(0)  {}

bool Settings::KeyExists::operator()(const std::string & s)  
{
  bool ret = true;
  if (ini->get(section, s).empty())  missing.push_back(s), ++missing_ct, ret = false;
  return ret;
}
