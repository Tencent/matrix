#ifdef __cplusplus
extern "C" {
#endif

#define UNWIND_TEST_TAG "Unwind-test"

//#define FRAME_MAX_SIZE 100 // Get all frames.
#define FRAME_MAX_SIZE 60 // Through native/JNI/AOT
//#define FRAME_MAX_SIZE 18 // Native only

#define _MIN_SIZE(a, b) (a > b ? b : a)
#define FRAME_ELEMENTS_MAX_SIZE _MIN_SIZE(FRAME_MAX_SIZE, 30)

enum UnwindTestMode {
    DWARF_UNWIND,                       // Baseline, libunwindstack
    FP_UNWIND,                          // FP
    WECHAT_QUICKEN_UNWIND,              // Quicken
    COMM_EH_UNWIND,                     // Directly call _Unwind_Backtrace
    FP_AND_JAVA_UNWIND,                 // FP + Java throwable object
    JAVA_UNWIND,                        // Java throwable object

    JAVA_UNWIND_PRINT_STACKTRACE,       // Java get StacktraceElements.
    QUICKEN_UNWIND_PRINT_STACKTRACE,    // Quicken unwind + find symbols.
};

bool switch_print_stack(bool enable);

void benchmark_warm_up();

void set_unwind_mode(UnwindTestMode mode);

void print_dwarf_unwind();

void print_java_unwind();

void print_quicken_unwind();

void print_java_unwind_formatted();

void print_quicken_unwind_stacktrace();

void reset_benchmark_counting();

void dump_benchmark_calculation(const char *tag);

void leaf_func(const char *testcase);

#ifdef __cplusplus
}
#endif