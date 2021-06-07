// ConsoleClose.h

#ifndef __CONSOLE_CLOSE_H
#define __CONSOLE_CLOSE_H

namespace NConsoleClose {

extern unsigned g_BreakCounter;

inline bool TestBreakSignal()
{
  #ifdef UNDER_CE
  return false;
  #else
  return (g_BreakCounter != 0);
  #endif
}

class CCtrlHandlerSetter
{
public:
  CCtrlHandlerSetter();
  virtual ~CCtrlHandlerSetter();
};

class CCtrlBreakException
{};

// void CheckCtrlBreak();

}

#endif
