// Windows/ErrorMsg.h

#ifndef __WINDOWS_ERROR_MSG_H
#define __WINDOWS_ERROR_MSG_H

#include "../Common/MyString.h"

namespace NWindows {
namespace NError {

UString MyFormatMessage(DWORD errorCode);

}}

#endif
