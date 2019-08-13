// Windows/System.h

#ifndef __WINDOWS_SYSTEM_H
#define __WINDOWS_SYSTEM_H

#include "../Common/MyTypes.h"

namespace NWindows {
namespace NSystem {

UInt32 GetNumberOfProcessors();

bool GetRamSize(UInt64 &size); // returns false, if unknown ram size

}}

#endif
