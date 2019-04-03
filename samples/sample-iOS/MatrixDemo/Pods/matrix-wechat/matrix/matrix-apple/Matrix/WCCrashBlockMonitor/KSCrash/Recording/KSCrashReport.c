//
//  KSCrashReport.m
//
//  Created by Karl Stenerud on 2012-01-28.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
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


#include "KSCrashReport.h"

#include "KSCrashReportFields.h"
#include "KSCrashReportWriter.h"
#include "KSDynamicLinker.h"
#include "KSFileUtils.h"
#include "KSJSONCodec.h"
#include "KSCPU.h"
#include "KSMemory.h"
#include "KSMach.h"
#include "KSThread.h"
#include "KSObjC.h"
#include "KSSignalInfo.h"
#include "KSCrashMonitor_Zombie.h"
#include "KSString.h"
#include "KSCrashReportVersion.h"
#include "KSStackCursor_Backtrace.h"
#include "KSStackCursor_MachineContext.h"
#include "KSSystemCapabilities.h"
#include "KSCrashCachedData.h"

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// ============================================================================
#pragma mark - Constants -
// ============================================================================

/** Default number of objects, subobjects, and ivars to record from a memory loc */
#define kDefaultMemorySearchDepth 15

/** How far to search the stack (in pointer sized jumps) for notable data. */
#define kStackNotableSearchBackDistance 20
#define kStackNotableSearchForwardDistance 10

/** How much of the stack to dump (in pointer sized jumps). */
#define kStackContentsPushedDistance 20
#define kStackContentsPoppedDistance 10
#define kStackContentsTotalDistance (kStackContentsPushedDistance + kStackContentsPoppedDistance)

/** The minimum length for a valid string. */
#define kMinStringLength 4


// ============================================================================
#pragma mark - JSON Encoding -
// ============================================================================

#define getJsonContext(REPORT_WRITER) ((KSJSONEncodeContext*)((REPORT_WRITER)->context))

/** Used for writing hex string values. */
static const char g_hexNybbles[] =
{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

// ============================================================================
#pragma mark - Runtime Config -
// ============================================================================

typedef struct
{
    /** If YES, introspect memory contents during a crash.
     * Any Objective-C objects or C strings near the stack pointer or referenced by
     * cpu registers or exceptions will be recorded in the crash report, along with
     * their contents.
     */
    bool enabled;
    
    /** List of classes that should never be introspected.
     * Whenever a class in this list is encountered, only the class name will be recorded.
     */
    const char** restrictedClasses;
    int restrictedClassesCount;
} KSCrash_IntrospectionRules;

static const char* g_userInfoJSON;
static KSCrash_IntrospectionRules g_introspectionRules;
static KSReportWriteCallback g_userSectionWriteCallback;
static KSReportWritePointThreadCallback g_pointThreadWriteCallback;

#pragma mark Callbacks

static void addBooleanElement(const KSCrashReportWriter* const writer, const char* const key, const bool value)
{
    ksjson_addBooleanElement(getJsonContext(writer), key, value);
}

static void addFloatingPointElement(const KSCrashReportWriter* const writer, const char* const key, const double value)
{
    ksjson_addFloatingPointElement(getJsonContext(writer), key, value);
}

static void addIntegerElement(const KSCrashReportWriter* const writer, const char* const key, const int64_t value)
{
    ksjson_addIntegerElement(getJsonContext(writer), key, value);
}

static void addUIntegerElement(const KSCrashReportWriter* const writer, const char* const key, const uint64_t value)
{
    ksjson_addIntegerElement(getJsonContext(writer), key, (int64_t)value);
}

static void addStringElement(const KSCrashReportWriter* const writer, const char* const key, const char* const value)
{
    ksjson_addStringElement(getJsonContext(writer), key, value, KSJSON_SIZE_AUTOMATIC);
}

static void addTextFileElement(const KSCrashReportWriter* const writer, const char* const key, const char* const filePath)
{
    const int fd = open(filePath, O_RDONLY);
    if(fd < 0)
    {
        KSLOG_ERROR("Could not open file %s: %s", filePath, strerror(errno));
        return;
    }

    if(ksjson_beginStringElement(getJsonContext(writer), key) != KSJSON_OK)
    {
        KSLOG_ERROR("Could not start string element");
        goto done;
    }

    char buffer[512];
    int bytesRead;
    for(bytesRead = (int)read(fd, buffer, sizeof(buffer));
        bytesRead > 0;
        bytesRead = (int)read(fd, buffer, sizeof(buffer)))
    {
        if(ksjson_appendStringElement(getJsonContext(writer), buffer, bytesRead) != KSJSON_OK)
        {
            KSLOG_ERROR("Could not append string element");
            goto done;
        }
    }

done:
    ksjson_endStringElement(getJsonContext(writer));
    close(fd);
}

static void addDataElement(const KSCrashReportWriter* const writer,
                           const char* const key,
                           const char* const value,
                           const int length)
{
    ksjson_addDataElement(getJsonContext(writer), key, value, length);
}

static void beginDataElement(const KSCrashReportWriter* const writer, const char* const key)
{
    ksjson_beginDataElement(getJsonContext(writer), key);
}

static void appendDataElement(const KSCrashReportWriter* const writer, const char* const value, const int length)
{
    ksjson_appendDataElement(getJsonContext(writer), value, length);
}

static void endDataElement(const KSCrashReportWriter* const writer)
{
    ksjson_endDataElement(getJsonContext(writer));
}

static void addUUIDElement(const KSCrashReportWriter* const writer, const char* const key, const unsigned char* const value)
{
    if(value == NULL)
    {
        ksjson_addNullElement(getJsonContext(writer), key);
    }
    else
    {
        char uuidBuffer[37];
        const unsigned char* src = value;
        char* dst = uuidBuffer;
        for(int i = 0; i < 4; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 2; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 2; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 2; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }
        *dst++ = '-';
        for(int i = 0; i < 6; i++)
        {
            *dst++ = g_hexNybbles[(*src>>4)&15];
            *dst++ = g_hexNybbles[(*src++)&15];
        }

        ksjson_addStringElement(getJsonContext(writer), key, uuidBuffer, (int)(dst - uuidBuffer));
    }
}

static void addJSONElement(const KSCrashReportWriter* const writer,
                           const char* const key,
                           const char* const jsonElement,
                           bool closeLastContainer)
{
    int jsonResult = ksjson_addJSONElement(getJsonContext(writer),
                                           key,
                                           jsonElement,
                                           (int)strlen(jsonElement),
                                           closeLastContainer);
    if(jsonResult != KSJSON_OK)
    {
        char errorBuff[100];
        snprintf(errorBuff,
                 sizeof(errorBuff),
                 "Invalid JSON data: %s",
                 ksjson_stringForError(jsonResult));
        ksjson_beginObject(getJsonContext(writer), key);
        ksjson_addStringElement(getJsonContext(writer),
                                KSCrashField_Error,
                                errorBuff,
                                KSJSON_SIZE_AUTOMATIC);
        ksjson_addStringElement(getJsonContext(writer),
                                KSCrashField_JSONData,
                                jsonElement,
                                KSJSON_SIZE_AUTOMATIC);
        ksjson_endContainer(getJsonContext(writer));
    }
}

static void addJSONElementFromFile(const KSCrashReportWriter* const writer,
                                   const char* const key,
                                   const char* const filePath,
                                   bool closeLastContainer)
{
    ksjson_addJSONFromFile(getJsonContext(writer), key, filePath, closeLastContainer);
}

static void beginObject(const KSCrashReportWriter* const writer, const char* const key)
{
    ksjson_beginObject(getJsonContext(writer), key);
}

static void beginArray(const KSCrashReportWriter* const writer, const char* const key)
{
    ksjson_beginArray(getJsonContext(writer), key);
}

static void endContainer(const KSCrashReportWriter* const writer)
{
    ksjson_endContainer(getJsonContext(writer));
}


static void addTextLinesFromFile(const KSCrashReportWriter* const writer, const char* const key, const char* const filePath)
{
    char readBuffer[1024];
    KSBufferedReader reader;
    if(!ksfu_openBufferedReader(&reader, filePath, readBuffer, sizeof(readBuffer)))
    {
        return;
    }
    char buffer[1024];
    beginArray(writer, key);
    {
        for(;;)
        {
            int length = sizeof(buffer);
            ksfu_readBufferedReaderUntilChar(&reader, '\n', buffer, &length);
            if(length <= 0)
            {
                break;
            }
            buffer[length - 1] = '\0';
            ksjson_addStringElement(getJsonContext(writer), NULL, buffer, KSJSON_SIZE_AUTOMATIC);
        }
    }
    endContainer(writer);
    ksfu_closeBufferedReader(&reader);
}

static int addJSONData(const char* restrict const data, const int length, void* restrict userData)
{
    KSBufferedWriter* writer = (KSBufferedWriter*)userData;
    const bool success = ksfu_writeBufferedWriter(writer, data, length);
    return success ? KSJSON_OK : KSJSON_ERROR_CANNOT_ADD_DATA;
}


// ============================================================================
#pragma mark - Utility -
// ============================================================================

/** Check if a memory address points to a valid null terminated UTF-8 string.
 *
 * @param address The address to check.
 *
 * @return true if the address points to a string.
 */
static bool isValidString(const void* const address)
{
    if((void*)address == NULL)
    {
        return false;
    }

    char buffer[500];
    if((uintptr_t)address+sizeof(buffer) < (uintptr_t)address)
    {
        // Wrapped around the address range.
        return false;
    }
    if(!ksmem_copySafely(address, buffer, sizeof(buffer)))
    {
        return false;
    }
    return ksstring_isNullTerminatedUTF8String(buffer, kMinStringLength, sizeof(buffer));
}

/** Get the backtrace for the specified machine context.
 *
 * This function will choose how to fetch the backtrace based on the crash and
 * machine context. It may store the backtrace in backtraceBuffer unless it can
 * be fetched directly from memory. Do not count on backtraceBuffer containing
 * anything. Always use the return value.
 *
 * @param crash The crash handler context.
 *
 * @param machineContext The machine context.
 *
 * @param cursor The stack cursor to fill.
 *
 * @return True if the cursor was filled.
 */
static bool getStackCursor(const KSCrash_MonitorContext* const crash,
                           const struct KSMachineContext* const machineContext,
                           KSStackCursor *cursor)
{
    if(ksmc_getThreadFromContext(machineContext) == ksmc_getThreadFromContext(crash->offendingMachineContext))
    {
        *cursor = *((KSStackCursor*)crash->stackCursor);
        return true;
    }

    kssc_initWithMachineContext(cursor, KSSC_STACK_OVERFLOW_THRESHOLD, machineContext);
    return true;
}


// ============================================================================
#pragma mark - Report Writing -
// ============================================================================

/** Write the contents of a memory location.
 * Also writes meta information about the data.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param address The memory address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeMemoryContents(const KSCrashReportWriter* const writer,
                                const char* const key,
                                const uintptr_t address,
                                int* limit);

/** Write a string to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeNSStringContents(const KSCrashReportWriter* const writer,
                                  const char* const key,
                                  const uintptr_t objectAddress,
                                  __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    char buffer[200];
    if(ksobjc_copyStringContents(object, buffer, sizeof(buffer)))
    {
        writer->addStringElement(writer, key, buffer);
    }
}

/** Write a URL to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeURLContents(const KSCrashReportWriter* const writer,
                             const char* const key,
                             const uintptr_t objectAddress,
                             __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    char buffer[200];
    if(ksobjc_copyStringContents(object, buffer, sizeof(buffer)))
    {
        writer->addStringElement(writer, key, buffer);
    }
}

/** Write a date to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeDateContents(const KSCrashReportWriter* const writer,
                              const char* const key,
                              const uintptr_t objectAddress,
                              __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    writer->addFloatingPointElement(writer, key, ksobjc_dateContents(object));
}

/** Write a number to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeNumberContents(const KSCrashReportWriter* const writer,
                                const char* const key,
                                const uintptr_t objectAddress,
                                __unused int* limit)
{
    const void* object = (const void*)objectAddress;
    writer->addFloatingPointElement(writer, key, ksobjc_numberAsFloat(object));
}

/** Write an array to the report.
 * This will only print the first child of the array.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeArrayContents(const KSCrashReportWriter* const writer,
                               const char* const key,
                               const uintptr_t objectAddress,
                               int* limit)
{
    const void* object = (const void*)objectAddress;
    uintptr_t firstObject = 0;
    if(ksobjc_arrayContents(object, &firstObject, 1) == 1)
    {
        writeMemoryContents(writer, key, firstObject, limit);
    }
}

/** Write out ivar information about an unknown object.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param objectAddress The object's address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeUnknownObjectContents(const KSCrashReportWriter* const writer,
                                       const char* const key,
                                       const uintptr_t objectAddress,
                                       int* limit)
{
    (*limit)--;
    const void* object = (const void*)objectAddress;
    KSObjCIvar ivars[10];
    int8_t s8 = 0;
    int16_t s16 = 0;
    int sInt = 0;
    int32_t s32 = 0;
    int64_t s64 = 0;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    unsigned int uInt = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;
    float f32 = 0.;
    double f64 = 0.;
    bool b = false;
    void* pointer;
    
    
    writer->beginObject(writer, key);
    {
        if(ksobjc_isTaggedPointer(object))
        {
            writer->addIntegerElement(writer, "tagged_payload", (int64_t)ksobjc_taggedPointerPayload(object));
        }
        else
        {
            const void* class = ksobjc_isaPointer(object);
            int ivarCount = ksobjc_ivarList(class, ivars, sizeof(ivars)/sizeof(*ivars));
            *limit -= ivarCount;
            for(int i = 0; i < ivarCount; i++)
            {
                KSObjCIvar* ivar = &ivars[i];
                switch(ivar->type[0])
                {
                    case 'c':
                        ksobjc_ivarValue(object, ivar->index, &s8);
                        writer->addIntegerElement(writer, ivar->name, s8);
                        break;
                    case 'i':
                        ksobjc_ivarValue(object, ivar->index, &sInt);
                        writer->addIntegerElement(writer, ivar->name, sInt);
                        break;
                    case 's':
                        ksobjc_ivarValue(object, ivar->index, &s16);
                        writer->addIntegerElement(writer, ivar->name, s16);
                        break;
                    case 'l':
                        ksobjc_ivarValue(object, ivar->index, &s32);
                        writer->addIntegerElement(writer, ivar->name, s32);
                        break;
                    case 'q':
                        ksobjc_ivarValue(object, ivar->index, &s64);
                        writer->addIntegerElement(writer, ivar->name, s64);
                        break;
                    case 'C':
                        ksobjc_ivarValue(object, ivar->index, &u8);
                        writer->addUIntegerElement(writer, ivar->name, u8);
                        break;
                    case 'I':
                        ksobjc_ivarValue(object, ivar->index, &uInt);
                        writer->addUIntegerElement(writer, ivar->name, uInt);
                        break;
                    case 'S':
                        ksobjc_ivarValue(object, ivar->index, &u16);
                        writer->addUIntegerElement(writer, ivar->name, u16);
                        break;
                    case 'L':
                        ksobjc_ivarValue(object, ivar->index, &u32);
                        writer->addUIntegerElement(writer, ivar->name, u32);
                        break;
                    case 'Q':
                        ksobjc_ivarValue(object, ivar->index, &u64);
                        writer->addUIntegerElement(writer, ivar->name, u64);
                        break;
                    case 'f':
                        ksobjc_ivarValue(object, ivar->index, &f32);
                        writer->addFloatingPointElement(writer, ivar->name, f32);
                        break;
                    case 'd':
                        ksobjc_ivarValue(object, ivar->index, &f64);
                        writer->addFloatingPointElement(writer, ivar->name, f64);
                        break;
                    case 'B':
                        ksobjc_ivarValue(object, ivar->index, &b);
                        writer->addBooleanElement(writer, ivar->name, b);
                        break;
                    case '*':
                    case '@':
                    case '#':
                    case ':':
                        ksobjc_ivarValue(object, ivar->index, &pointer);
                        writeMemoryContents(writer, ivar->name, (uintptr_t)pointer, limit);
                        break;
                    default:
                        KSLOG_DEBUG("%s: Unknown ivar type [%s]", ivar->name, ivar->type);
                }
            }
        }
    }
    writer->endContainer(writer);
}

static bool isRestrictedClass(const char* name)
{
    if(g_introspectionRules.restrictedClasses != NULL)
    {
        for(int i = 0; i < g_introspectionRules.restrictedClassesCount; i++)
        {
            if(strcmp(name, g_introspectionRules.restrictedClasses[i]) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

static void writeZombieIfPresent(const KSCrashReportWriter* const writer,
                                 const char* const key,
                                 const uintptr_t address)
{
#if KSCRASH_HAS_OBJC
    const void* object = (const void*)address;
    const char* zombieClassName = kszombie_className(object);
    if(zombieClassName != NULL)
    {
        writer->addStringElement(writer, key, zombieClassName);
    }
#endif
}

static bool writeObjCObject(const KSCrashReportWriter* const writer,
                            const uintptr_t address,
                            int* limit)
{
#if KSCRASH_HAS_OBJC
    const void* object = (const void*)address;
    switch(ksobjc_objectType(object))
    {
        case KSObjCTypeClass:
            writer->addStringElement(writer, KSCrashField_Type, KSCrashMemType_Class);
            writer->addStringElement(writer, KSCrashField_Class, ksobjc_className(object));
            return true;
        case KSObjCTypeObject:
        {
            writer->addStringElement(writer, KSCrashField_Type, KSCrashMemType_Object);
            const char* className = ksobjc_objectClassName(object);
            writer->addStringElement(writer, KSCrashField_Class, className);
            if(!isRestrictedClass(className))
            {
                switch(ksobjc_objectClassType(object))
                {
                    case KSObjCClassTypeString:
                        writeNSStringContents(writer, KSCrashField_Value, address, limit);
                        return true;
                    case KSObjCClassTypeURL:
                        writeURLContents(writer, KSCrashField_Value, address, limit);
                        return true;
                    case KSObjCClassTypeDate:
                        writeDateContents(writer, KSCrashField_Value, address, limit);
                        return true;
                    case KSObjCClassTypeArray:
                        if(*limit > 0)
                        {
                            writeArrayContents(writer, KSCrashField_FirstObject, address, limit);
                        }
                        return true;
                    case KSObjCClassTypeNumber:
                        writeNumberContents(writer, KSCrashField_Value, address, limit);
                        return true;
                    case KSObjCClassTypeDictionary:
                    case KSObjCClassTypeException:
                        // TODO: Implement these.
                        if(*limit > 0)
                        {
                            writeUnknownObjectContents(writer, KSCrashField_Ivars, address, limit);
                        }
                        return true;
                    case KSObjCClassTypeUnknown:
                        if(*limit > 0)
                        {
                            writeUnknownObjectContents(writer, KSCrashField_Ivars, address, limit);
                        }
                        return true;
                }
            }
            break;
        }
        case KSObjCTypeBlock:
            writer->addStringElement(writer, KSCrashField_Type, KSCrashMemType_Block);
            const char* className = ksobjc_objectClassName(object);
            writer->addStringElement(writer, KSCrashField_Class, className);
            return true;
        case KSObjCTypeUnknown:
            break;
    }
#endif

    return false;
}

/** Write the contents of a memory location.
 * Also writes meta information about the data.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param address The memory address.
 *
 * @param limit How many more subreferenced objects to write, if any.
 */
static void writeMemoryContents(const KSCrashReportWriter* const writer,
                                const char* const key,
                                const uintptr_t address,
                                int* limit)
{
    (*limit)--;
    const void* object = (const void*)address;
    writer->beginObject(writer, key);
    {
        writer->addUIntegerElement(writer, KSCrashField_Address, address);
        writeZombieIfPresent(writer, KSCrashField_LastDeallocObject, address);
        if(!writeObjCObject(writer, address, limit))
        {
            if(object == NULL)
            {
                writer->addStringElement(writer, KSCrashField_Type, KSCrashMemType_NullPointer);
            }
            else if(isValidString(object))
            {
                writer->addStringElement(writer, KSCrashField_Type, KSCrashMemType_String);
                writer->addStringElement(writer, KSCrashField_Value, (const char*)object);
            }
            else
            {
                writer->addStringElement(writer, KSCrashField_Type, KSCrashMemType_Unknown);
            }
        }
    }
    writer->endContainer(writer);
}

static bool isValidPointer(const uintptr_t address)
{
    if(address == (uintptr_t)NULL)
    {
        return false;
    }

#if KSCRASH_HAS_OBJC
    if(ksobjc_isTaggedPointer((const void*)address))
    {
        if(!ksobjc_isValidTaggedPointer((const void*)address))
        {
            return false;
        }
    }
#endif

    return true;
}

static bool isNotableAddress(const uintptr_t address)
{
    if(!isValidPointer(address))
    {
        return false;
    }
    
    const void* object = (const void*)address;

#if KSCRASH_HAS_OBJC
    if(kszombie_className(object) != NULL)
    {
        return true;
    }

    if(ksobjc_objectType(object) != KSObjCTypeUnknown)
    {
        return true;
    }
#endif

    if(isValidString(object))
    {
        return true;
    }

    return false;
}

/** Write the contents of a memory location only if it contains notable data.
 * Also writes meta information about the data.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param address The memory address.
 */
static void writeMemoryContentsIfNotable(const KSCrashReportWriter* const writer,
                                         const char* const key,
                                         const uintptr_t address)
{
    if(isNotableAddress(address))
    {
        int limit = kDefaultMemorySearchDepth;
        writeMemoryContents(writer, key, address, &limit);
    }
}

/** Look for a hex value in a string and try to write whatever it references.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param string The string to search.
 */
static void writeAddressReferencedByString(const KSCrashReportWriter* const writer,
                                           const char* const key,
                                           const char* string)
{
    uint64_t address = 0;
    if(string == NULL || !ksstring_extractHexValue(string, (int)strlen(string), &address))
    {
        return;
    }
    
    int limit = kDefaultMemorySearchDepth;
    writeMemoryContents(writer, key, (uintptr_t)address, &limit);
}

#pragma mark Backtrace

static uintptr_t g_lastFirstAddr = 0;
static uintptr_t g_lastSecndAddr = 0;
static uintptr_t g_lastThirdAddr = 0;
static uintptr_t g_lastForthAddr = 0;
static uintptr_t g_lastFifthAddr = 0;
static uint32_t g_tailPoint = 0;

/** Write a backtrace to the report.
 *
 * @param writer The writer to write the backtrace to.
 *
 * @param key The object key, if needed.
 *
 * @param stackCursor The stack cursor to read from.
 */
static void writeBacktrace(const KSCrashReportWriter* const writer,
                           const char* const key,
                           KSStackCursor* stackCursor)
{
    writer->beginObject(writer, key);
    {
        writer->beginArray(writer, KSCrashField_Contents);
        {
            g_tailPoint = 0;
            while(stackCursor->advanceCursor(stackCursor))
            {
                if (g_tailPoint == 100) {
                    g_lastFirstAddr = stackCursor->stackEntry.address;
                }
                if (g_tailPoint == 101) {
                    g_lastSecndAddr = stackCursor->stackEntry.address;
                }
                if (g_tailPoint == 102) {
                    g_lastThirdAddr = stackCursor->stackEntry.address;
                }
                if (g_tailPoint == 103) {
                    g_lastForthAddr = stackCursor->stackEntry.address;
                }
                if (g_tailPoint == 104) {
                    g_lastFifthAddr = stackCursor->stackEntry.address;
                }
                g_tailPoint++;
                if (g_tailPoint > 105) {
                    if (stackCursor->stackEntry.address == g_lastFirstAddr ||
                        stackCursor->stackEntry.address == g_lastSecndAddr ||
                        stackCursor->stackEntry.address == g_lastThirdAddr ||
                        stackCursor->stackEntry.address == g_lastForthAddr ||
                        stackCursor->stackEntry.address == g_lastFifthAddr) {
                        continue;
                    }
                }
                writer->beginObject(writer, NULL);
                {
                    if(stackCursor->symbolicate(stackCursor))
                    {
                        if(stackCursor->stackEntry.imageName != NULL)
                        {
                            writer->addStringElement(writer, KSCrashField_ObjectName, ksfu_lastPathEntry(stackCursor->stackEntry.imageName));
                        }
                        writer->addUIntegerElement(writer, KSCrashField_ObjectAddr, stackCursor->stackEntry.imageAddress);
                        if(stackCursor->stackEntry.symbolName != NULL)
                        {
                            writer->addStringElement(writer, KSCrashField_SymbolName, stackCursor->stackEntry.symbolName);
                        }
                        writer->addUIntegerElement(writer, KSCrashField_SymbolAddr, stackCursor->stackEntry.symbolAddress);
                    }
                    writer->addUIntegerElement(writer, KSCrashField_InstructionAddr, stackCursor->stackEntry.address);
                }
                writer->endContainer(writer);
            }
        }
        writer->endContainer(writer);
        writer->addIntegerElement(writer, KSCrashField_Skipped, 0);
    }
    writer->endContainer(writer);
}
                              

#pragma mark Stack

/** Write a dump of the stack contents to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the stack from.
 *
 * @param isStackOverflow If true, the stack has overflowed.
 */
static void writeStackContents(const KSCrashReportWriter* const writer,
                               const char* const key,
                               const struct KSMachineContext* const machineContext,
                               const bool isStackOverflow)
{
    uintptr_t sp = kscpu_stackPointer(machineContext);
    if((void*)sp == NULL)
    {
        return;
    }

    uintptr_t lowAddress = sp + (uintptr_t)(kStackContentsPushedDistance * (int)sizeof(sp) * kscpu_stackGrowDirection() * -1);
    uintptr_t highAddress = sp + (uintptr_t)(kStackContentsPoppedDistance * (int)sizeof(sp) * kscpu_stackGrowDirection());
    if(highAddress < lowAddress)
    {
        uintptr_t tmp = lowAddress;
        lowAddress = highAddress;
        highAddress = tmp;
    }
    writer->beginObject(writer, key);
    {
        writer->addStringElement(writer, KSCrashField_GrowDirection, kscpu_stackGrowDirection() > 0 ? "+" : "-");
        writer->addUIntegerElement(writer, KSCrashField_DumpStart, lowAddress);
        writer->addUIntegerElement(writer, KSCrashField_DumpEnd, highAddress);
        writer->addUIntegerElement(writer, KSCrashField_StackPtr, sp);
        writer->addBooleanElement(writer, KSCrashField_Overflow, isStackOverflow);
        uint8_t stackBuffer[kStackContentsTotalDistance * sizeof(sp)];
        int copyLength = (int)(highAddress - lowAddress);
        if(ksmem_copySafely((void*)lowAddress, stackBuffer, copyLength))
        {
            writer->addDataElement(writer, KSCrashField_Contents, (void*)stackBuffer, copyLength);
        }
        else
        {
            writer->addStringElement(writer, KSCrashField_Error, "Stack contents not accessible");
        }
    }
    writer->endContainer(writer);
}

/** Write any notable addresses near the stack pointer (above and below).
 *
 * @param writer The writer.
 *
 * @param machineContext The context to retrieve the stack from.
 *
 * @param backDistance The distance towards the beginning of the stack to check.
 *
 * @param forwardDistance The distance past the end of the stack to check.
 */
static void writeNotableStackContents(const KSCrashReportWriter* const writer,
                                      const struct KSMachineContext* const machineContext,
                                      const int backDistance,
                                      const int forwardDistance)
{
    uintptr_t sp = kscpu_stackPointer(machineContext);
    if((void*)sp == NULL)
    {
        return;
    }

    uintptr_t lowAddress = sp + (uintptr_t)(backDistance * (int)sizeof(sp) * kscpu_stackGrowDirection() * -1);
    uintptr_t highAddress = sp + (uintptr_t)(forwardDistance * (int)sizeof(sp) * kscpu_stackGrowDirection());
    if(highAddress < lowAddress)
    {
        uintptr_t tmp = lowAddress;
        lowAddress = highAddress;
        highAddress = tmp;
    }
    uintptr_t contentsAsPointer = 0;
    char nameBuffer[40];
    for(uintptr_t address = lowAddress; address < highAddress; address += sizeof(address))
    {
        if(ksmem_copySafely((void*)address, &contentsAsPointer, sizeof(contentsAsPointer)))
        {
            sprintf(nameBuffer, "stack@%p", (void*)address);
            writeMemoryContentsIfNotable(writer, nameBuffer, contentsAsPointer);
        }
    }
}


#pragma mark Registers

/** Write the contents of all regular registers to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeBasicRegisters(const KSCrashReportWriter* const writer,
                                const char* const key,
                                const struct KSMachineContext* const machineContext)
{
    char registerNameBuff[30];
    const char* registerName;
    writer->beginObject(writer, key);
    {
        const int numRegisters = kscpu_numRegisters();
        for(int reg = 0; reg < numRegisters; reg++)
        {
            registerName = kscpu_registerName(reg);
            if(registerName == NULL)
            {
                snprintf(registerNameBuff, sizeof(registerNameBuff), "r%d", reg);
                registerName = registerNameBuff;
            }
            writer->addUIntegerElement(writer, registerName,
                                       kscpu_registerValue(machineContext, reg));
        }
    }
    writer->endContainer(writer);
}

/** Write the contents of all exception registers to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeExceptionRegisters(const KSCrashReportWriter* const writer,
                                    const char* const key,
                                    const struct KSMachineContext* const machineContext)
{
    char registerNameBuff[30];
    const char* registerName;
    writer->beginObject(writer, key);
    {
        const int numRegisters = kscpu_numExceptionRegisters();
        for(int reg = 0; reg < numRegisters; reg++)
        {
            registerName = kscpu_exceptionRegisterName(reg);
            if(registerName == NULL)
            {
                snprintf(registerNameBuff, sizeof(registerNameBuff), "r%d", reg);
                registerName = registerNameBuff;
            }
            writer->addUIntegerElement(writer,registerName,
                                       kscpu_exceptionRegisterValue(machineContext, reg));
        }
    }
    writer->endContainer(writer);
}

/** Write all applicable registers.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeRegisters(const KSCrashReportWriter* const writer,
                           const char* const key,
                           const struct KSMachineContext* const machineContext)
{
    writer->beginObject(writer, key);
    {
        writeBasicRegisters(writer, KSCrashField_Basic, machineContext);
        if(ksmc_hasValidExceptionRegisters(machineContext))
        {
            writeExceptionRegisters(writer, KSCrashField_Exception, machineContext);
        }
    }
    writer->endContainer(writer);
}

/** Write any notable addresses contained in the CPU registers.
 *
 * @param writer The writer.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeNotableRegisters(const KSCrashReportWriter* const writer,
                                  const struct KSMachineContext* const machineContext)
{
    char registerNameBuff[30];
    const char* registerName;
    const int numRegisters = kscpu_numRegisters();
    for(int reg = 0; reg < numRegisters; reg++)
    {
        registerName = kscpu_registerName(reg);
        if(registerName == NULL)
        {
            snprintf(registerNameBuff, sizeof(registerNameBuff), "r%d", reg);
            registerName = registerNameBuff;
        }
        writeMemoryContentsIfNotable(writer,
                                     registerName,
                                     (uintptr_t)kscpu_registerValue(machineContext, reg));
    }
}

#pragma mark Thread-specific

/** Write any notable addresses in the stack or registers to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param machineContext The context to retrieve the registers from.
 */
static void writeNotableAddresses(const KSCrashReportWriter* const writer,
                                  const char* const key,
                                  const struct KSMachineContext* const machineContext)
{
    writer->beginObject(writer, key);
    {
        writeNotableRegisters(writer, machineContext);
        writeNotableStackContents(writer,
                                  machineContext,
                                  kStackNotableSearchBackDistance,
                                  kStackNotableSearchForwardDistance);
    }
    writer->endContainer(writer);
}

/** Write information about a thread to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param crash The crash handler context.
 *
 * @param machineContext The context whose thread to write about.
 *
 * @param shouldWriteNotableAddresses If true, write any notable addresses found.
 */
static void writeThread(const KSCrashReportWriter* const writer,
                        const char* const key,
                        const KSCrash_MonitorContext* const crash,
                        const struct KSMachineContext* const machineContext,
                        const int threadIndex,
                        const bool shouldWriteNotableAddresses)
{
    bool isCrashedThread = ksmc_isCrashedContext(machineContext);
    KSThread thread = ksmc_getThreadFromContext(machineContext);
    KSLOG_DEBUG("Writing thread %x (index %d). is crashed: %d", thread, threadIndex, isCrashedThread);

    KSStackCursor stackCursor;
    bool hasBacktrace = getStackCursor(crash, machineContext, &stackCursor);

    writer->beginObject(writer, key);
    {
        if(hasBacktrace)
        {
            writeBacktrace(writer, KSCrashField_Backtrace, &stackCursor);
        }
        if(ksmc_canHaveCPUState(machineContext))
        {
            writeRegisters(writer, KSCrashField_Registers, machineContext);
        }
        writer->addIntegerElement(writer, KSCrashField_Index, threadIndex);
        const char* name = ksccd_getThreadName(thread);
        if(name != NULL)
        {
            writer->addStringElement(writer, KSCrashField_Name, name);
        }
        name = ksccd_getQueueName(thread);
        if(name != NULL)
        {
            writer->addStringElement(writer, KSCrashField_DispatchQueue, name);
        }
        writer->addBooleanElement(writer, KSCrashField_Crashed, isCrashedThread);
        writer->addBooleanElement(writer, KSCrashField_CurrentThread, thread == ksthread_self());
        if(isCrashedThread)
        {
            writeStackContents(writer, KSCrashField_Stack, machineContext, stackCursor.state.hasGivenUp);
            if(shouldWriteNotableAddresses)
            {
                writeNotableAddresses(writer, KSCrashField_NotableAddresses, machineContext);
            }
        }
    }
    writer->endContainer(writer);
}

/** Write information about all threads to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param crash The crash handler context.
 */
static void writeAllThreads(const KSCrashReportWriter* const writer,
                            const char* const key,
                            const KSCrash_MonitorContext* const crash,
                            bool writeNotableAddresses)
{
    const struct KSMachineContext* const context = crash->offendingMachineContext;
    KSThread offendingThread = ksmc_getThreadFromContext(context);
    int threadCount = ksmc_getThreadCount(context);
    KSMC_NEW_CONTEXT(machineContext);

    // Fetch info for all threads.
    writer->beginArray(writer, key);
    {
        KSLOG_DEBUG("Writing %d threads.", threadCount);
        
        if (g_pointThreadWriteCallback != NULL) {
            KSStackCursor *stackCursor = g_pointThreadWriteCallback();
            if (stackCursor != NULL) {
                writer->beginObject(writer, NULL);
                {
                    writeBacktrace(writer, KSCrashField_Backtrace, stackCursor);
                    writer->addIntegerElement(writer, KSCrashField_Index, 0);
                    const char* name = "HandledThread";
                    if(name != NULL)
                    {
                        writer->addStringElement(writer, KSCrashField_Name, name);
                    }
                    writer->addBooleanElement(writer, KSCrashField_Crashed, false);
                    writer->addBooleanElement(writer, KSCrashField_CurrentThread, false);
                }
                writer->endContainer(writer);
            }
        }
        int threadIndex = 0;
        for(int i = 0; i < threadCount; i++)
        {
            KSThread thread = ksmc_getThreadAtIndex(context, i);
            if(thread == offendingThread)
            {
                writeThread(writer, NULL, crash, context, threadIndex, writeNotableAddresses);
            }
            else
            {
                ksmc_getContextForThread(thread, machineContext, false);
                writeThread(writer, NULL, crash, machineContext, threadIndex, writeNotableAddresses);
            }
            threadIndex++;
        }
    }
    writer->endContainer(writer);
}

#pragma mark Global Report Data

/** Write information about a binary image to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param index Which image to write about.
 */
static void writeBinaryImage(const KSCrashReportWriter* const writer,
                             const char* const key,
                             const int index)
{
    KSBinaryImage image = {0};
    if(!ksdl_getBinaryImage(index, &image))
    {
        return;
    }

    writer->beginObject(writer, key);
    {
        writer->addUIntegerElement(writer, KSCrashField_ImageAddress, image.address);
        writer->addUIntegerElement(writer, KSCrashField_ImageVmAddress, image.vmAddress);
        writer->addUIntegerElement(writer, KSCrashField_ImageSize, image.size);
        writer->addStringElement(writer, KSCrashField_Name, image.name);
        writer->addUUIDElement(writer, KSCrashField_UUID, image.uuid);
        writer->addIntegerElement(writer, KSCrashField_CPUType, image.cpuType);
        writer->addIntegerElement(writer, KSCrashField_CPUSubType, image.cpuSubType);
        writer->addUIntegerElement(writer, KSCrashField_ImageMajorVersion, image.majorVersion);
        writer->addUIntegerElement(writer, KSCrashField_ImageMinorVersion, image.minorVersion);
        writer->addUIntegerElement(writer, KSCrashField_ImageRevisionVersion, image.revisionVersion);
    }
    writer->endContainer(writer);
}

/** Write information about all images to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 */
static void writeBinaryImages(const KSCrashReportWriter* const writer, const char* const key)
{
    const int imageCount = ksdl_imageCount();

    writer->beginArray(writer, key);
    {
        for(int iImg = 0; iImg < imageCount; iImg++)
        {
            writeBinaryImage(writer, NULL, iImg);
        }
    }
    writer->endContainer(writer);
}

/** Write information about system memory to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 */
static void writeMemoryInfo(const KSCrashReportWriter* const writer,
                            const char* const key,
                            const KSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        writer->addUIntegerElement(writer, KSCrashField_Size, monitorContext->System.memorySize);
        writer->addUIntegerElement(writer, KSCrashField_Usable, monitorContext->System.usableMemory);
        writer->addUIntegerElement(writer, KSCrashField_Free, monitorContext->System.freeMemory);
    }
    writer->endContainer(writer);
}

/** Write information about the error leading to the crash to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param crash The crash handler context.
 */
static void writeError(const KSCrashReportWriter* const writer,
                       const char* const key,
                       const KSCrash_MonitorContext* const crash)
{
    writer->beginObject(writer, key);
    {
#if KSCRASH_HOST_APPLE
        writer->beginObject(writer, KSCrashField_Mach);
        {
            const char* machExceptionName = ksmach_exceptionName(crash->mach.type);
            const char* machCodeName = crash->mach.code == 0 ? NULL : ksmach_kernelReturnCodeName(crash->mach.code);
            writer->addUIntegerElement(writer, KSCrashField_Exception, (unsigned)crash->mach.type);
            if(machExceptionName != NULL)
            {
                writer->addStringElement(writer, KSCrashField_ExceptionName, machExceptionName);
            }
            writer->addUIntegerElement(writer, KSCrashField_Code, (unsigned)crash->mach.code);
            if(machCodeName != NULL)
            {
                writer->addStringElement(writer, KSCrashField_CodeName, machCodeName);
            }
            writer->addUIntegerElement(writer, KSCrashField_Subcode, (unsigned)crash->mach.subcode);
        }
        writer->endContainer(writer);
#endif
        writer->beginObject(writer, KSCrashField_Signal);
        {
            const char* sigName = kssignal_signalName(crash->signal.signum);
            const char* sigCodeName = kssignal_signalCodeName(crash->signal.signum, crash->signal.sigcode);
            writer->addUIntegerElement(writer, KSCrashField_Signal, (unsigned)crash->signal.signum);
            if(sigName != NULL)
            {
                writer->addStringElement(writer, KSCrashField_Name, sigName);
            }
            writer->addUIntegerElement(writer, KSCrashField_Code, (unsigned)crash->signal.sigcode);
            if(sigCodeName != NULL)
            {
                writer->addStringElement(writer, KSCrashField_CodeName, sigCodeName);
            }
        }
        writer->endContainer(writer);

        writer->addUIntegerElement(writer, KSCrashField_Address, crash->faultAddress);
        if(crash->crashReason != NULL)
        {
            writer->addStringElement(writer, KSCrashField_Reason, crash->crashReason);
        }

        // Gather specific info.
        switch(crash->crashType)
        {
            case KSCrashMonitorTypeMainThreadDeadlock:
                writer->addStringElement(writer, KSCrashField_Type, KSCrashExcType_Deadlock);
                break;
                
            case KSCrashMonitorTypeMachException:
                writer->addStringElement(writer, KSCrashField_Type, KSCrashExcType_Mach);
                break;

            case KSCrashMonitorTypeCPPException:
            {
                writer->addStringElement(writer, KSCrashField_Type, KSCrashExcType_CPPException);
                writer->beginObject(writer, KSCrashField_CPPException);
                {
                    writer->addStringElement(writer, KSCrashField_Name, crash->CPPException.name);
                }
                writer->endContainer(writer);
                break;
            }
            case KSCrashMonitorTypeNSException:
            {
                writer->addStringElement(writer, KSCrashField_Type, KSCrashExcType_NSException);
                writer->beginObject(writer, KSCrashField_NSException);
                {
                    writer->addStringElement(writer, KSCrashField_Name, crash->NSException.name);
                    writeAddressReferencedByString(writer, KSCrashField_ReferencedObject, crash->crashReason);
                }
                writer->endContainer(writer);
                break;
            }
            case KSCrashMonitorTypeSignal:
                writer->addStringElement(writer, KSCrashField_Type, KSCrashExcType_Signal);
                break;

            case KSCrashMonitorTypeUserReported:
            {
                writer->addStringElement(writer, KSCrashField_Type, KSCrashExcType_User);
                writer->beginObject(writer, KSCrashField_UserReported);
                {
                    writer->addStringElement(writer, KSCrashField_Name, crash->userException.name);
                    if(crash->userException.userDumpType != 0)
                    {
                        writer->addIntegerElement(writer, KSCrashField_DumpType, crash->userException.userDumpType);
                    }
                    if(crash->userException.language != NULL)
                    {
                        writer->addStringElement(writer, KSCrashField_Language, crash->userException.language);
                    }
                    if(crash->userException.lineOfCode != NULL)
                    {
                        writer->addStringElement(writer, KSCrashField_LineOfCode, crash->userException.lineOfCode);
                    }
                    if(crash->userException.customStackTrace != NULL)
                    {
                        writer->addJSONElement(writer, KSCrashField_Backtrace, crash->userException.customStackTrace, true);
                    }
                }
                writer->endContainer(writer);
                break;
            }
            case KSCrashMonitorTypeSystem:
            case KSCrashMonitorTypeApplicationState:
            case KSCrashMonitorTypeZombie:
                KSLOG_ERROR("Crash monitor type 0x%x shouldn't be able to cause events!", crash->crashType);
                break;
        }
    }
    writer->endContainer(writer);
}

/** Write information about app runtime, etc to the report.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param monitorContext The event monitor context.
 */
static void writeAppStats(const KSCrashReportWriter* const writer,
                          const char* const key,
                          const KSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        writer->addBooleanElement(writer, KSCrashField_AppActive, monitorContext->AppState.applicationIsActive);
        writer->addBooleanElement(writer, KSCrashField_AppInFG, monitorContext->AppState.applicationIsInForeground);

        writer->addIntegerElement(writer, KSCrashField_LaunchesSinceCrash, monitorContext->AppState.launchesSinceLastCrash);
        writer->addIntegerElement(writer, KSCrashField_SessionsSinceCrash, monitorContext->AppState.sessionsSinceLastCrash);
        writer->addFloatingPointElement(writer, KSCrashField_ActiveTimeSinceCrash, monitorContext->AppState.activeDurationSinceLastCrash);
        writer->addFloatingPointElement(writer, KSCrashField_BGTimeSinceCrash, monitorContext->AppState.backgroundDurationSinceLastCrash);

        writer->addIntegerElement(writer, KSCrashField_SessionsSinceLaunch, monitorContext->AppState.sessionsSinceLaunch);
        writer->addFloatingPointElement(writer, KSCrashField_ActiveTimeSinceLaunch, monitorContext->AppState.activeDurationSinceLaunch);
        writer->addFloatingPointElement(writer, KSCrashField_BGTimeSinceLaunch, monitorContext->AppState.backgroundDurationSinceLaunch);
        
        /* The app launch time info added */
        writer->addUIntegerElement(writer, KSCrashField_AppLaunchTime, monitorContext->AppState.appLaunchTimeStamp);
    }
    writer->endContainer(writer);
}

/** Write information about this process.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 */
static void writeProcessState(const KSCrashReportWriter* const writer,
                              const char* const key,
                              const KSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        if(monitorContext->ZombieException.address != 0)
        {
            writer->beginObject(writer, KSCrashField_LastDeallocedNSException);
            {
                writer->addUIntegerElement(writer, KSCrashField_Address, monitorContext->ZombieException.address);
                writer->addStringElement(writer, KSCrashField_Name, monitorContext->ZombieException.name);
                writer->addStringElement(writer, KSCrashField_Reason, monitorContext->ZombieException.reason);
                writeAddressReferencedByString(writer, KSCrashField_ReferencedObject, monitorContext->ZombieException.reason);
            }
            writer->endContainer(writer);
        }
    }
    writer->endContainer(writer);
}

/** Write basic report information.
 *
 * @param writer The writer.
 *
 * @param key The object key, if needed.
 *
 * @param type The report type.
 *
 * @param reportID The report ID.
 */
static void writeReportInfo(const KSCrashReportWriter* const writer,
                            const char* const key,
                            const char* const type,
                            const char* const reportID,
                            const char* const processName)
{
    writer->beginObject(writer, key);
    {
        writer->addStringElement(writer, KSCrashField_Version, KSCRASH_REPORT_VERSION);
        writer->addStringElement(writer, KSCrashField_ID, reportID);
        writer->addStringElement(writer, KSCrashField_ProcessName, processName);
        writer->addIntegerElement(writer, KSCrashField_Timestamp, time(NULL));
        writer->addStringElement(writer, KSCrashField_Type, type);
    }
    writer->endContainer(writer);
}

static void writeRecrash(const KSCrashReportWriter* const writer,
                         const char* const key,
                         const char* crashReportPath)
{
    writer->addJSONFileElement(writer, key, crashReportPath, true);
}


#pragma mark Setup

/** Prepare a report writer for use.
 *
 * @oaram writer The writer to prepare.
 *
 * @param context JSON writer contextual information.
 */
static void prepareReportWriter(KSCrashReportWriter* const writer, KSJSONEncodeContext* const context)
{
    writer->addBooleanElement = addBooleanElement;
    writer->addFloatingPointElement = addFloatingPointElement;
    writer->addIntegerElement = addIntegerElement;
    writer->addUIntegerElement = addUIntegerElement;
    writer->addStringElement = addStringElement;
    writer->addTextFileElement = addTextFileElement;
    writer->addTextFileLinesElement = addTextLinesFromFile;
    writer->addJSONFileElement = addJSONElementFromFile;
    writer->addDataElement = addDataElement;
    writer->beginDataElement = beginDataElement;
    writer->appendDataElement = appendDataElement;
    writer->endDataElement = endDataElement;
    writer->addUUIDElement = addUUIDElement;
    writer->addJSONElement = addJSONElement;
    writer->beginObject = beginObject;
    writer->beginArray = beginArray;
    writer->endContainer = endContainer;
    writer->context = context;
}


// ============================================================================
#pragma mark - Main API -
// ============================================================================

void kscrashreport_writeRecrashReport(const KSCrash_MonitorContext* const monitorContext, const char* const path)
{
    char writeBuffer[1024];
    KSBufferedWriter bufferedWriter;
    static char tempPath[KSFU_MAX_PATH_LENGTH];
    strncpy(tempPath, path, sizeof(tempPath) - 10);
    strncpy(tempPath + strlen(tempPath) - 5, ".old", 5);
    KSLOG_INFO("Writing recrash report to %s", path);

    if(rename(path, tempPath) < 0)
    {
        KSLOG_ERROR("Could not rename %s to %s: %s", path, tempPath, strerror(errno));
    }
    if(!ksfu_openBufferedWriter(&bufferedWriter, path, writeBuffer, sizeof(writeBuffer)))
    {
        return;
    }

    ksccd_freeze();

    KSJSONEncodeContext jsonContext;
    jsonContext.userData = &bufferedWriter;
    KSCrashReportWriter concreteWriter;
    KSCrashReportWriter* writer = &concreteWriter;
    prepareReportWriter(writer, &jsonContext);

    ksjson_beginEncode(getJsonContext(writer), true, addJSONData, &bufferedWriter);

    writer->beginObject(writer, KSCrashField_Report);
    {
        writeRecrash(writer, KSCrashField_RecrashReport, tempPath);
        ksfu_flushBufferedWriter(&bufferedWriter);
        if(remove(tempPath) < 0)
        {
            KSLOG_ERROR("Could not remove %s: %s", tempPath, strerror(errno));
        }
        writeReportInfo(writer,
                        KSCrashField_Report,
                        KSCrashReportType_Minimal,
                        monitorContext->eventID,
                        monitorContext->System.processName);
        ksfu_flushBufferedWriter(&bufferedWriter);

        writer->beginObject(writer, KSCrashField_Crash);
        {
            writeError(writer, KSCrashField_Error, monitorContext);
            ksfu_flushBufferedWriter(&bufferedWriter);
            int threadIndex = ksmc_indexOfThread(monitorContext->offendingMachineContext,
                                                 ksmc_getThreadFromContext(monitorContext->offendingMachineContext));
            writeThread(writer,
                        KSCrashField_CrashedThread,
                        monitorContext,
                        monitorContext->offendingMachineContext,
                        threadIndex,
                        false);
            ksfu_flushBufferedWriter(&bufferedWriter);
        }
        writer->endContainer(writer);
    }
    writer->endContainer(writer);

    ksjson_endEncode(getJsonContext(writer));
    ksfu_closeBufferedWriter(&bufferedWriter);
    ksccd_unfreeze();
}

static void writeSystemInfo(const KSCrashReportWriter* const writer,
                            const char* const key,
                            const KSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        writer->addStringElement(writer, KSCrashField_SystemName, monitorContext->System.systemName);
        writer->addStringElement(writer, KSCrashField_SystemVersion, monitorContext->System.systemVersion);
        writer->addStringElement(writer, KSCrashField_Machine, monitorContext->System.machine);
        writer->addStringElement(writer, KSCrashField_Model, monitorContext->System.model);
        writer->addStringElement(writer, KSCrashField_KernelVersion, monitorContext->System.kernelVersion);
        writer->addStringElement(writer, KSCrashField_OSVersion, monitorContext->System.osVersion);
        writer->addBooleanElement(writer, KSCrashField_Jailbroken, monitorContext->System.isJailbroken);
        writer->addStringElement(writer, KSCrashField_BootTime, monitorContext->System.bootTime);
        writer->addStringElement(writer, KSCrashField_AppStartTime, monitorContext->System.appStartTime);
        writer->addStringElement(writer, KSCrashField_ExecutablePath, monitorContext->System.executablePath);
        writer->addStringElement(writer, KSCrashField_Executable, monitorContext->System.executableName);
        writer->addStringElement(writer, KSCrashField_BundleID, monitorContext->System.bundleID);
        writer->addStringElement(writer, KSCrashField_BundleName, monitorContext->System.bundleName);
        if (monitorContext->UserDefinedVersion.bundleVersion != NULL && monitorContext->UserDefinedVersion.bundleShortVersion != NULL) {
            writer->addStringElement(writer, KSCrashField_BundleVersion, monitorContext->UserDefinedVersion.bundleVersion);
            writer->addStringElement(writer, KSCrashField_BundleShortVersion, monitorContext->UserDefinedVersion.bundleShortVersion);
        } else {
            writer->addStringElement(writer, KSCrashField_BundleVersion, monitorContext->System.bundleVersion);
            writer->addStringElement(writer, KSCrashField_BundleShortVersion, monitorContext->System.bundleShortVersion);
        }
        writer->addStringElement(writer, KSCrashField_AppUUID, monitorContext->System.appID);
        writer->addStringElement(writer, KSCrashField_CPUArch, monitorContext->System.cpuArchitecture);
        writer->addIntegerElement(writer, KSCrashField_CPUType, monitorContext->System.cpuType);
        writer->addIntegerElement(writer, KSCrashField_CPUSubType, monitorContext->System.cpuSubType);
        writer->addIntegerElement(writer, KSCrashField_BinaryCPUType, monitorContext->System.binaryCPUType);
        writer->addIntegerElement(writer, KSCrashField_BinaryCPUSubType, monitorContext->System.binaryCPUSubType);
        writer->addStringElement(writer, KSCrashField_TimeZone, monitorContext->System.timezone);
        writer->addStringElement(writer, KSCrashField_ProcessName, monitorContext->System.processName);
        writer->addIntegerElement(writer, KSCrashField_ProcessID, monitorContext->System.processID);
        writer->addIntegerElement(writer, KSCrashField_ParentProcessID, monitorContext->System.parentProcessID);
        writer->addStringElement(writer, KSCrashField_DeviceAppHash, monitorContext->System.deviceAppHash);
        writer->addStringElement(writer, KSCrashField_BuildType, monitorContext->System.buildType);
        writer->addIntegerElement(writer, KSCrashField_Storage, (int64_t)monitorContext->System.storageSize);

        writeMemoryInfo(writer, KSCrashField_Memory, monitorContext);
        writeAppStats(writer, KSCrashField_AppStats, monitorContext);
    }
    writer->endContainer(writer);

}

static void writeDebugInfo(const KSCrashReportWriter* const writer,
                            const char* const key,
                            const KSCrash_MonitorContext* const monitorContext)
{
    writer->beginObject(writer, key);
    {
        if(monitorContext->consoleLogPath != NULL)
        {
            addTextLinesFromFile(writer, KSCrashField_ConsoleLog, monitorContext->consoleLogPath);
        }
    }
    writer->endContainer(writer);
    
}

void kscrashreport_writeStandardReport(const KSCrash_MonitorContext* const monitorContext, const char* const path)
{
    KSLOG_INFO("Writing crash report to %s", path);
    char writeBuffer[1024];
    KSBufferedWriter bufferedWriter;

    if(!ksfu_openBufferedWriter(&bufferedWriter, path, writeBuffer, sizeof(writeBuffer)))
    {
        return;
    }

    ksccd_freeze();
    
    KSJSONEncodeContext jsonContext;
    jsonContext.userData = &bufferedWriter;
    KSCrashReportWriter concreteWriter;
    KSCrashReportWriter* writer = &concreteWriter;
    prepareReportWriter(writer, &jsonContext);

    ksjson_beginEncode(getJsonContext(writer), true, addJSONData, &bufferedWriter);

    writer->beginObject(writer, KSCrashField_Report);
    {
        writeReportInfo(writer,
                        KSCrashField_Report,
                        KSCrashReportType_Standard,
                        monitorContext->eventID,
                        monitorContext->System.processName);
        ksfu_flushBufferedWriter(&bufferedWriter);

        writeBinaryImages(writer, KSCrashField_BinaryImages);
        ksfu_flushBufferedWriter(&bufferedWriter);

        writeProcessState(writer, KSCrashField_ProcessState, monitorContext);
        ksfu_flushBufferedWriter(&bufferedWriter);

        writeSystemInfo(writer, KSCrashField_System, monitorContext);
        ksfu_flushBufferedWriter(&bufferedWriter);

        writer->beginObject(writer, KSCrashField_Crash);
        {
            writeError(writer, KSCrashField_Error, monitorContext);
            ksfu_flushBufferedWriter(&bufferedWriter);
            writeAllThreads(writer,
                            KSCrashField_Threads,
                            monitorContext,
                            g_introspectionRules.enabled);
            ksfu_flushBufferedWriter(&bufferedWriter);
        }
        writer->endContainer(writer);

        if(g_userInfoJSON != NULL)
        {
            addJSONElement(writer, KSCrashField_User, g_userInfoJSON, false);
            ksfu_flushBufferedWriter(&bufferedWriter);
        }
        else
        {
            writer->beginObject(writer, KSCrashField_User);
        }
        if(g_userSectionWriteCallback != NULL)
        {
            ksfu_flushBufferedWriter(&bufferedWriter);
            g_userSectionWriteCallback(writer);
        }
        writer->endContainer(writer);
        ksfu_flushBufferedWriter(&bufferedWriter);

        writeDebugInfo(writer, KSCrashField_Debug, monitorContext);
    }
    writer->endContainer(writer);
    
    ksjson_endEncode(getJsonContext(writer));
    ksfu_closeBufferedWriter(&bufferedWriter);
    ksccd_unfreeze();
}



void kscrashreport_setUserInfoJSON(const char* const userInfoJSON)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    KSLOG_TRACE("set userInfoJSON to %p", userInfoJSON);

    pthread_mutex_lock(&mutex);
    if(g_userInfoJSON != NULL)
    {
        free((void*)g_userInfoJSON);
    }
    if(userInfoJSON == NULL)
    {
        g_userInfoJSON = NULL;
    }
    else
    {
        g_userInfoJSON = strdup(userInfoJSON);
    }
    pthread_mutex_unlock(&mutex);
}

void kscrashreport_setIntrospectMemory(bool shouldIntrospectMemory)
{
    g_introspectionRules.enabled = shouldIntrospectMemory;
}

void kscrashreport_setDoNotIntrospectClasses(const char** doNotIntrospectClasses, int length)
{
    const char** oldClasses = g_introspectionRules.restrictedClasses;
    int oldClassesLength = g_introspectionRules.restrictedClassesCount;
    const char** newClasses = NULL;
    int newClassesLength = 0;
    
    if(doNotIntrospectClasses != NULL && length > 0)
    {
        newClassesLength = length;
        newClasses = malloc(sizeof(*newClasses) * (unsigned)newClassesLength);
        if(newClasses == NULL)
        {
            KSLOG_ERROR("Could not allocate memory");
            return;
        }
        
        for(int i = 0; i < newClassesLength; i++)
        {
            newClasses[i] = strdup(doNotIntrospectClasses[i]);
        }
    }
    
    g_introspectionRules.restrictedClasses = newClasses;
    g_introspectionRules.restrictedClassesCount = newClassesLength;
    
    if(oldClasses != NULL)
    {
        for(int i = 0; i < oldClassesLength; i++)
        {
            free((void*)oldClasses[i]);
        }
        free(oldClasses);
    }
}

void kscrashreport_setUserSectionWriteCallback(const KSReportWriteCallback userSectionWriteCallback)
{
    KSLOG_TRACE("Set userSectionWriteCallback to %p", userSectionWriteCallback);
    g_userSectionWriteCallback = userSectionWriteCallback;
}

void kscrashreport_setPointThreadWriteCallback(const KSReportWritePointThreadCallback pointThreadWriteCallback)
{
    KSLOG_TRACE("Set pointThreadWriteCallback to %p", pointThreadWriteCallback);
    g_pointThreadWriteCallback = pointThreadWriteCallback;
}
