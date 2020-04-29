#include <unwindstack/DwarfError.h>

static bool gFastFlag = false;

namespace unwindstack {
    bool GetFastFlag() {
        return gFastFlag;
    }

    void SetFastFlag(bool flag) {
        gFastFlag = flag;
    }
}