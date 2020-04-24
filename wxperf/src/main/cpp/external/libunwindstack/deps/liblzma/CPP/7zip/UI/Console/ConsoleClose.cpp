// ConsoleClose.cpp

#include "StdAfx.h"

#include "ConsoleClose.h"

#if !defined(UNDER_CE) && defined(_WIN32)
#include "../../../Common/MyWindows.h"
#endif

namespace NConsoleClose {

unsigned g_BreakCounter = 0;
static const unsigned kBreakAbortThreshold = 2;

#if !defined(UNDER_CE) && defined(_WIN32)
static BOOL WINAPI HandlerRoutine(DWORD ctrlType)
{
  if (ctrlType == CTRL_LOGOFF_EVENT)
  {
    // printf("\nCTRL_LOGOFF_EVENT\n");
    return TRUE;
  }

  g_BreakCounter++;
  if (g_BreakCounter < kBreakAbortThreshold)
    return TRUE;
  return FALSE;
  /*
  switch (ctrlType)
  {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
      if (g_BreakCounter < kBreakAbortThreshold)
      return TRUE;
  }
  return FALSE;
  */
}
#endif

/*
void CheckCtrlBreak()
{
  if (TestBreakSignal())
    throw CCtrlBreakException();
}
*/

CCtrlHandlerSetter::CCtrlHandlerSetter()
{
  #if !defined(UNDER_CE) && defined(_WIN32)
  if (!SetConsoleCtrlHandler(HandlerRoutine, TRUE))
    throw "SetConsoleCtrlHandler fails";
  #endif
}

CCtrlHandlerSetter::~CCtrlHandlerSetter()
{
  #if !defined(UNDER_CE) && defined(_WIN32)
  if (!SetConsoleCtrlHandler(HandlerRoutine, FALSE))
  {
    // warning for throw in destructor.
    // throw "SetConsoleCtrlHandler fails";
  }
  #endif
}

}
