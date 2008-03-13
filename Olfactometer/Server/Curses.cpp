#include "Curses.h"

Size Size::null(-99999,-99999);

CursesWindow::CursesWindow(CursesWindow *parent, const std::string & name, const Size & s, const Point & p, int default_cp, int bk_cp, bool lvok)
  : Component(parent, name), m_win(0), topLeft(p), absTopLeft(p), sz(s)
{
  if (parent) absTopLeft += parent->absPos();
  m_win = newwin(size().rows(), size().cols(), absPos().y(), absPos().x());
  setDefaultColor(default_cp);
  setBackgroundColor(bk_cp);
  setLeaveOK(lvok);
}

CursesWindow::~CursesWindow()
{
  werase(m_win);
  if (m_win) delwin(m_win), m_win = 0;  
}

void CursesWindow::childRemoved(Component *c)
{
  Component::childRemoved(c);
  repaint();
}

void CursesWindow::refresh()
{
  // refresh self first
  wrefresh(m_win);
  // now, refresh all children.. NEXT so they overwrite us
  std::list<Component *> chlds = children();
  for (std::list<Component *>::iterator it = chlds.begin(); it != chlds.end(); ++it)
    {
      CursesWindow *c = dynamic_cast<CursesWindow *>(*it);
      if (c)  c->refresh();      
    }
}

void CursesWindow::touch()
{
  // touch self first
  touchwin(m_win);
  // now, touch all children.. NEXT so they overwrite us
  std::list<Component *> chlds = children();
  for (std::list<Component *>::iterator it = chlds.begin(); it != chlds.end(); ++it)
    {
      CursesWindow *c = dynamic_cast<CursesWindow *>(*it);
      if (c)  c->touch();      
    }
}

void CursesWindow::repaint()
{
  touch();
  refresh();
}

Point CursesWindow::putStr(const char *str, const Point & pos, int color, int attr, bool preserveOldLoc)
{
  int x = pos.x(), y = pos.y();
  wstandend(m_win);
  if (!color) color = defaultColor();
  if (color) wcolor_set(m_win, color, 0);
  if (attr) wattr_on(m_win, attr, 0);
  int oldy, oldx;
  getyx(m_win, oldy, oldx);
  if (pos.isNull()) y = oldy;
  if (pos.isNull()) x = oldx;
  if (y < 0) y = oldy+y;
  if (x < 0) x = oldx+x;
  wmove(m_win, y, x);
  waddstr(m_win, str);
  if (preserveOldLoc)
    wmove(m_win, oldy, oldx);
  wstandend(m_win);
  return curs();
}

Point CursesWindow::putCh(int ch, const Point & pos, int color, int attr, bool preserveOldLoc)
{
  int x = pos.x(), y = pos.y();
  wstandend(m_win);
  if (!color) color = defaultColor();
  if (color) wcolor_set(m_win, color, 0);
  if (attr) wattr_on(m_win, attr, 0);
  int oldy, oldx;
  getyx(m_win, oldy, oldx);
  if (pos.isNull()) y = oldy;
  if (pos.isNull()) x = oldx;
  if (y < 0) y = oldy+y;
  if (x < 0) x = oldx+x;
  wmove(m_win, y, x);
  waddch(m_win, ch);
  if (preserveOldLoc)
    wmove(m_win, oldy, oldx);
  wstandend(m_win);  
  return curs();
}

void CursesWindow::setLeaveOK(bool b) { lok = b; leaveok(m_win, lok); }

void CursesWindow::setBackgroundColor(int c) 
{ 
  bkgnd_cp = c; 
  if (bkgnd_cp < 0) {
    CursesWindow *p;
    if (parent() && ( p = dynamic_cast<CursesWindow *>(parent())))
      bkgnd_cp = p->backgroundColor();
  }
  if (bkgnd_cp > 0) wbkgd(m_win, COLOR_PAIR(bkgnd_cp));  
}

void CursesWindow::setDefaultColor(int color_pair) 
{
  def_cp = color_pair;
  if (def_cp < 0) {
    CursesWindow *p;
    if (parent() && ( p = dynamic_cast<CursesWindow *>(parent())))
      def_cp = p->defaultColor();
  }
}

Point CursesWindow::curs() const
{
  int y,x;
  getyx(m_win, y, x);
  return Point(y, x);  
}
