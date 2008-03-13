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
#ifndef Settings_H
#define Settings_H

#include <iterator>
#include <set>
#include <map>
#include <list>
#include <iostream>
#include <fstream>
#include <string>

class Settings
{
public:
  typedef std::map<std::string, std::string> Section;
  typedef std::set<std::string> DirtyKeys;  
  typedef std::map<std::string, DirtyKeys > DirtyMap;
  typedef std::map<std::string, Section> SettingsMap;

  Settings();
  Settings(const std::string & filename);
  ~Settings();

  bool parse(const std::string &fromString = ""); // NB: this clobbers the existing settings in this class!, if fromString is empty it uses the file instead

  bool save();
  /// spews forth what would have been written to the .ini file to a string
  std::string dump() const;

  /// functor that might be useful in some algorithms for keeping track of missing sections from an ini file
  struct SectionExists
  {
    SectionExists(const Settings &s);
    const Settings  *ini;
    std::list<std::string> missing; ///< the list of things missing
    int missing_ct;
    bool operator()(const std::string & s);
  };

  /// functor that might be useful in some algorithms for keeping track of missing keys within a section from an ini file
  struct KeyExists
  {
    KeyExists(const Settings &s, const std::string &section);
    const Settings *ini;
    std::string section;
    std::list<std::string> missing; ///< the list of things missing
    int missing_ct;
    bool operator()(const std::string & s);
  };

protected:
  /// just like parse, except doesn't read a file, but populates this
  /// class from a string containing the entire contents of the file..
  static SettingsMap fromString(const std::string & settingsDump);

public:

  std::string getFile() const; 
  void setFile(const std::string & name); /*  after setting this,
                                              typically you need
                                              to call
                                              parse() */

  unsigned long length() const; // length of underlying file

  std::string currentSection() const { return _currentSection; };
  void setSection(const std::string & section) { _currentSection = section; };

  std::string get(const std::string & key) const;
  void put(const std::string &key, const std::string &value);

  std::string get(const std::string & section, const std::string & key) const;
  void put(const std::string & section, const std::string &key, const std::string &value);

  void putSection(const std::string & section_name, const Section & section);
  void mergeSection(const std::string & section_name, const Section & section);
  Section getSection(const std::string & section_name) const;

  /* retrieves all the section names available in this settings instance */
  std::list<std::string> sections() const;

  /* two identical methods to merge one settings instance into this one */
  Settings & operator<<(const Settings & merge_from);
  Settings & merge(const Settings & merge_from);
  
protected:
  /* a derived class may optionally set masterSettings -- without it all settings in the file are read and acknowledged */
  const SettingsMap *masterSettings;

private:

  std::string _currentSection;

  DirtyMap dirtySettings;

  SettingsMap settingsMap;


/* reads all the name/value pairs in the settings file and throws them in a map and returns that */
  static SettingsMap readAll(std::iostream &);

  /* parses a VALID settings line and populates key and value */
  static
  void parseMatchedLine(std::string matchedline, 
                        std::string & key, std::string & value);

  /*
    returns a matched substring from line if line matches the perl-like re:
        \s*\S+\s*=\s*\S+

     otherwise returns std::string::null.
  */
  static std::string testLine(std::string line);
  /* 
     returns the section name if the line matches the form:
        \s*\[\S*\]\s*
     otherwise returns null
 */
  static std::string testSectionLine(std::string line);

  mutable std::fstream theFile;

  void init();
  std::string fname;
  static void simplifyWhiteSpace(std::string &l);
};


#endif
