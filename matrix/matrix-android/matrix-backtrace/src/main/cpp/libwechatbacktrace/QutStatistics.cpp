//
// Created by Carl on 2020-10-29.
//

#include <map>
#include <vector>
#include <memory>
#include <iterator>
#include <string>
#include "QutStatistics.h"
#include "Log.h"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    using namespace std;

    typedef map<uint32_t, shared_ptr<vector<pair<uint64_t, uint64_t>>>> stat_info_map_t;

    DEFINE_STATIC_LOCAL(string, gCurrStatLib, );
//    DEFINE_STATIC_LOCAL(stat_info_map_t*, sStatisticInfo, );
//    DEFINE_STATIC_LOCAL(stat_info_map_t*, sStatisticTipsInfo, );

    static map<uint32_t, shared_ptr<vector<pair<uint64_t, uint64_t>>>> *sStatisticInfo = nullptr;
    static map<uint32_t, shared_ptr<vector<pair<uint64_t, uint64_t>>>> *sStatisticTipsInfo = nullptr;

    void SetCurrentStatLib(const string lib) {
        gCurrStatLib = lib;
        if (sStatisticInfo != nullptr) delete (sStatisticInfo);
        sStatisticInfo = new map<uint32_t, shared_ptr<vector<pair<uint64_t, uint64_t>>>>;
        if (sStatisticTipsInfo != nullptr) delete (sStatisticTipsInfo);
        sStatisticTipsInfo = new map<uint32_t, shared_ptr<vector<pair<uint64_t, uint64_t>>>>;
    }

    void QutStatistic(QutStatisticType type, uint64_t val1, uint64_t val2) {
        if (!sStatisticInfo) {
            return;
        }
        vector<pair<uint64_t, uint64_t>> *v;
        if (sStatisticInfo->find(type) == sStatisticInfo->end()) {
            (*sStatisticInfo)[type] = make_shared<vector<pair<uint64_t, uint64_t>>>();
        }

        v = ((*sStatisticInfo)[type]).get();
        v->push_back(make_pair(val1, val2));
    }

    void QutStatisticTips(QutStatisticType type, uint64_t val1, uint64_t val2) {
        if (!sStatisticTipsInfo) {
            return;
        }
        vector<pair<uint64_t, uint64_t>> *v;
        if (sStatisticTipsInfo->find(type) == sStatisticTipsInfo->end()) {
            (*sStatisticTipsInfo)[type] = make_shared<vector<pair<uint64_t, uint64_t>>>();
        }

        v = ((*sStatisticTipsInfo)[type]).get();
        v->push_back(make_pair(val1, val2));
    }

    void DumpQutStatResult(vector<uint32_t> &processed_result) {
#ifdef QUT_STATISTIC_ENABLE
        auto it = sStatisticInfo->begin();
        QUT_STAT_LOG("Dump Qut Statistic for elf file %s:", gCurrStatLib.c_str());
        while (it != sStatisticInfo->end()) {
            QutStatisticType type = (QutStatisticType) it->first;
            vector<pair<uint64_t, uint64_t>> *v = it->second.get();
            switch (type) {
                case InstructionEntriesArmExidx:
                case InstructionEntriesEhFrame:
                case InstructionEntriesDebugFrame:
                    QUT_STAT_LOG("\t Type %u, Value %llu:", type, (uint64_t) v->at(0).first);
                    processed_result.push_back(type);
                    processed_result.push_back((uint32_t) v->at(0).first);
                    break;
                default:
                    QUT_STAT_LOG("\t Type %u, Count %llu:", type, (uint64_t) v->size());
                    processed_result.push_back(type);
                    processed_result.push_back((uint32_t) v->size());
                    break;
            }

            it++;
        }
        QUT_STAT_LOG("Dump Qut Statistic End.\n\n");
#endif
    }

    QUT_EXTERN_C_BLOCK_END
} // namespace wechat_backtrace
