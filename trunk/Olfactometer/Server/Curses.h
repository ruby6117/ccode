#ifndef Curses_H
#define Curses_H

#include "Component.h"
#include <ncurses.h>

class Size
{
public:
  Size() : r(0), c(0) {}
  Size(int r, int c) : r(r), c(c) {}
  Size(const Size &rhs) { *this = rhs; }

  static Size null;
  
  bool isNull() const { return *this == null; }
  void setNull() { *this = null; }
  int rows() const { return r; }
  int cols() const { return c; }
  void setRows(int newr) { r = newr; }  
  void setCols(int newc) { c = newc; }

  bool operator<(const Size & rhs) const { return r < rhs.r && c < rhs.c; }
  bool operator>(const Size & rhs) const { return r > rhs.r && c > rhs.c; }
  bool operator==(const Size & rhs) const { return r == rhs.r && c == rhs.c; }
  Size & operator=(const Size & rhs)  { r = rhs.r, c = rhs.c; return *this; }
  bool operator>=(const Size & rhs) const { return *this > rhs || *this == rhs; }
  bool operator<=(const Size & rhs) const { return *this < rhs || *this == rhs; }
  Size & operator+=(const Size &rhs) { r += rhs.r; c += rhs.c; return *this; }
  Size operator+(const Size & rhs) const { Size s(*this); return s += rhs; }

private:
  int r, c;
};

class Point : public Size
{
public:
  Point() : Size() {}
  Point(int y, int x) : Size(y, x) {}
  Point(const Size & rhs) : Size(rhs) {}
  int x() const { return cols(); }
  int y() const { return rows(); }
  void setX(int newx) { setCols(newx); }
  void setY(int newy) { setRows(newy); }
};

class CursesWindow : public Component
{
public:
  CursesWindow(CursesWindow *parent = 0, 
               const std::string & name = NAMELESS, 
               const Size & size = Size(LINES, COLS), 
               const Point & topleft_parent_relative = Point(0,0), 
               int default_color_pair = -1, int background_cp = -1, 
               bool leaveok = true);
  virtual ~CursesWindow();
  
  operator WINDOW *() const { return m_win; }
  WINDOW * win() const { return m_win; }

  const Point & pos() const { return topLeft; }
  const Point & absPos() const { return absTopLeft; }
  const Size  & size() const { return sz; }
  
  int rows() const { return size().rows(); }  
  int cols() const { return size().cols(); }
  /// returns current cursor position
  Point curs() const;

  int defaultColor() const { return def_cp; }
  /** sets the default color when writing text -- if <0 inherit from parent
      window, if 0 don't set any color property when writing text.  Positive
      value indicates a color pair previously defined with init_pair() */
  void setDefaultColor(int color_pair);
  int backgroundColor() const { return bkgnd_cp; }
  /** sets the erase (fille) color for the background -- if <0 inherit from
      parent window, if 0 don't set any color property for bkgnd (usually
      means we get default color?).  Positive value indicates a color pair
      previously defined with init_pair(). */      
  void setBackgroundColor(int color_pair);

  bool leaveOK() const { return lok; }
  void setLeaveOK(bool b);

  /**
     Write a string to the screen. If preserveOldLoc is true, 
     the cursor is moved back to its original location in this window
     otherwise it is updated to the new position at the end of the string.
     @param str the C-style string to write
     @param pos the position relative to this window. Use Point::null to just use the current cursos position
     @param color the color pair id to use or 0 to use the window default
     @param attr the character attribute (A_BOLD, etc) to use
     @param preserveOldLoc if true, the cursor position is restored to what it was before the string was written, otherwise it is updated to just after the string
     @returns new cursor location, which is either past the string or if preserveOldLoc is true, unchanged
  */
  Point putStr(const char *str, const Point & pos = Point::null, int color = 0, int attr = 0, bool preserveOldLoc = true);
  /// just like the above function but operates on one character only
  Point putCh(int ch, const Point & pos = Point::null, int color = 0, int attr = 0, bool preserveOldLoc = true);

  /// calls wrefresh on this window and all its children
  void refresh(); 
  /** calls touchwin on this window and all its children, thus causing the
      next call to refresh for this window (and its children) to fully 
      output all characters to the terminal, bypassing optimization */
  void touch();
  /** Combines touch and refresh for this window and all its children. */
  void repaint(); 

  // todo implement move, resize, and other curses functions

protected:
  /// refreshes/repaints, overriden from parent class
  virtual void childRemoved(Component *child); 
  
private:
  CursesWindow(const CursesWindow &o) : Component() {(void)o;}
  CursesWindow & operator=(const CursesWindow &o) {(void)o; return *this;}

  mutable WINDOW *m_win;
  Point topLeft, absTopLeft;
  Size  sz;
  int def_cp, bkgnd_cp;
  bool lok;
  
};

#endif
