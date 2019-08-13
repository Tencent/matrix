// Common/NewHandler.h

#ifndef __COMMON_NEW_HANDLER_H
#define __COMMON_NEW_HANDLER_H

/*
This file must be included before any code that uses operators "delete" or "new".
Also you must compile and link "NewHandler.cpp", if you use MSVC 6.0.
The operator "new" in MSVC 6.0 doesn't throw exception "bad_alloc".
So we define another version of operator "new" that throws "CNewException" on failure.

If you use compiler that throws exception in "new" operator (GCC or new version of MSVC),
you can compile without "NewHandler.cpp". So standard exception "bad_alloc" will be used.

It's still allowed to use redefined version of operator "new" from "NewHandler.cpp"
with any compiler. 7-Zip's code can work with "bad_alloc" and "CNewException" exceptions.
But if you use some additional code (outside of 7-Zip's code), you must check
that redefined version of operator "new" (that throws CNewException) is not
problem for your code.

Also we declare delete(void *p) throw() that creates smaller code.
*/

#include <stddef.h>

class CNewException {};

#ifdef WIN32
// We can compile my_new and my_delete with _fastcall
/*
void * my_new(size_t size);
void my_delete(void *p) throw();
// void * my_Realloc(void *p, size_t newSize, size_t oldSize);
*/
#endif

#ifdef _WIN32

void *
#ifdef _MSC_VER
__cdecl
#endif
operator new(size_t size);

void
#ifdef _MSC_VER
__cdecl
#endif
operator delete(void *p) throw();

#endif

/*
#ifdef _WIN32
void *
#ifdef _MSC_VER
__cdecl
#endif
operator new[](size_t size);

void
#ifdef _MSC_VER
__cdecl
#endif
operator delete[](void *p) throw();
#endif
*/

#endif
