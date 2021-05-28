#ifdef __cplusplus
extern "C" {
#endif

#define UNWIND_TEST_TAG "Unwind-test"

#define FRAME_MAX_SIZE 60 // Through native/JNI/AOT
//#define FRAME_MAX_SIZE 18 // Native only

enum UnwindTestMode {
    DWARF_UNWIND, // baseline
    FP_UNWIND,
    FP_AND_JAVA_UNWIND,
    WECHAT_QUICKEN_UNWIND,
    JAVA_UNWIND,
    COMM_EH_UNWIND,
};

void benchmark_warm_up();

void set_unwind_mode(UnwindTestMode mode);

void print_dwarf_unwind();

void print_java_unwind();

void print_wechat_quicken_unwind();

void reset_benchmark_counting();

void dump_benchmark_calculation(const char* tag);

void leaf_func(const char * testcase);

#ifdef __cplusplus
}
#endif