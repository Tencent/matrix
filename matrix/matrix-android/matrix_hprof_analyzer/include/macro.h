#ifndef __matrix_hprof_analyzer_macro_h__
#define __matrix_hprof_analyzer_macro_h__

#define unwrap(optional, nullopt_action)            \
    ({                                              \
        const auto &result = optional;              \
        if (!result.has_value()) nullopt_action;    \
        result.value();                             \
    })

#define friend_test(test_case_name, test_name)\
friend class test_case_name##_##test_name##_Test

#endif