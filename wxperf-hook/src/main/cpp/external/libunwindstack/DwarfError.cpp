#include <unwindstack/DwarfError.h>

static bool gFastFlag = false;

namespace unwindstack {

    // Only for test currently.
    bool GetFastFlag() {
        return gFastFlag;
    }

    void SetFastFlag(bool flag) {
        gFastFlag = flag;
    }
}