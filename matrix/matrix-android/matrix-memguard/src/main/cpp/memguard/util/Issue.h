//
// Created by tomystang on 2020/11/24.
//

#ifndef __MEMGUARD_ISSUE_H__
#define __MEMGUARD_ISSUE_H__


#include <vector>
#include "Auxiliary.h"
#include "PagePool.h"

// This value is not exported by kernel headers.
// DoNot change this value since it's related to kernel limitations. It has not
// help to expand or truncate fetched thread name by changing this value.
#define MAX_TASK_COMM_LEN 16

namespace memguard {
    enum IssueType {
        UNKNOWN = -1,
        OVERFLOW = 1,
        UNDERFLOW = 2,
        USE_AFTER_FREE = 3,
        DOUBLE_FREE = 4,
    };

    namespace issue {
        extern IssueType TLSVAR gLastIssueType;

        extern void TriggerIssue(pagepool::slot_t accessing_slot, IssueType type);
        extern IssueType GetLastIssueType();

        extern bool Report(const void* accessing_addr, void* ucontext);
        extern const char* GetIssueTypeName(IssueType type);

        typedef char ThreadName[MAX_TASK_COMM_LEN];
        extern void GetSelfThreadName(ThreadName name_out);
    }
}


#endif //__MEMGUARD_ISSUE_H__
