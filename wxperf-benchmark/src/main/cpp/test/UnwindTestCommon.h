#ifdef __cplusplus
extern "C" {
#endif

#define UNWIND_TEST_TAG "Unwind-test"

enum UnwindTestMode {
    DWARF_UNWIND, // baseline
    FP_UNWIND,
    WECHAT_QUICKEN_UNWIND,
//    FP_UNWIND_WITH_FALLBACK,
//    FAST_DWARF_UNWIND,
//    WECHAT_QUICKEN_UNWIND_V2_WIP,
};

void set_unwind_mode(UnwindTestMode mode);

void leaf_func(const char * testcase);

#ifdef __cplusplus
}
#endif