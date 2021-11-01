//
// Created by tomystang on 2020/11/6.
//

#include <cstring>
#include <util/Allocation.h>
#include <util/Auxiliary.h>
#include <util/SignalHandler.h>
#include <util/Interception.h>
#include <util/PagePool.h>
#include <util/Log.h>
#include <sstream>
#include <util/Paths.h>
#include <common/SoLoadMonitor.h>
#include "MemGuard.h"

using namespace memguard;

#define LOG_TAG "MemGuard.Native"

memguard::Options memguard::gOpts;
static std::atomic_flag sInstalled(false);

bool memguard::Install(const Options* opts) {
    if (opts == nullptr) {
        LOGE(LOG_TAG, "opts == nullptr");
        return false;
    }

    if (sInstalled.test_and_set()) {
        LOGW(LOG_TAG, "Already installed.");
        return true;
    }

    gOpts = *opts;

    if (!paths::Exists(gOpts.issueDumpFilePath)) {
        if (!paths::MakeDirs(paths::GetParent(gOpts.issueDumpFilePath))) {
            LOGE(LOG_TAG, "Fail to create directory for issue dump file: %s", gOpts.issueDumpFilePath.c_str());
            return false;
        }
    }

    if (!matrix::InstallSoLoadMonitor()) {
        LOGE(LOG_TAG, "Fail to install SoLoadMonitor.");
        return false;
    }

    if (!pagepool::Prepare()) {
        LOGE(LOG_TAG, "Fail to prepare page pool.");
        return false;
    }

    if (!unwind::Initialize()) {
        LOGE(LOG_TAG, "Fail to initialize stack unwinder.");
        return false;
    }

    if (!signalhandler::Install()) {
        LOGE(LOG_TAG, "Fail to install signal handler.");
        return false;
    }

    if (!allocation::Prepare()) {
        LOGE(LOG_TAG, "Fail to prepare allocation logic.");
        return false;
    }

    if (!interception::Install()) {
        LOGE(LOG_TAG, "Fail to install interceptors.");
        return false;
    }

    LOGI(LOG_TAG, "MemGuard was installed.");
    return true;
}
