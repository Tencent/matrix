//
//  KSStackCursor_SelfThread.h
//
//  Copyright (c) 2016 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#ifndef KSStackCursor_SelfThread_h
#define KSStackCursor_SelfThread_h

#ifdef __cplusplus
extern "C" {
#endif
    
    
#include "KSStackCursor.h"

/** Initialize a stack cursor for the current thread.
 *  You may want to skip some entries to account for the trace immediately leading
 *  up to this init function.
 *
 * @param cursor The stack cursor to initialize.
 *
 * @param skipEntries The number of stack entries to skip.
 */
void kssc_initSelfThread(KSStackCursor *cursor, int skipEntries);
    
int kssc_backtraceCurrentThread(KSThread currentThread, uintptr_t* backtraceBuffer, int maxEntries);

#ifdef __cplusplus
}
#endif

#endif // KSStackCursor_SelfThread_h
