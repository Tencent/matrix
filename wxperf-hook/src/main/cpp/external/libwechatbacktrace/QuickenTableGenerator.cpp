//
// Created by Carl on 2020-09-21.
//

#include <cstdint>
#include <deps/android-base/include/android-base/logging.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/Memory.h>
#include <vector>
#include <utility>
#include <memory>
#include "QuickenTableGenerator.h"
#include "MinimalRegs.h"
#include "../../common/Log.h"
#include "DwarfEhFrameWithHdrDecoder.h"
#include "DwarfDebugFrameDecoder.h"
#include "DwarfEhFrameDecoder.h"
#include "ExidxDecoder.h"

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;

struct TempEntryPair {
    uptr entry_point = 0;
    deque<uint8_t> encoded_instructions;
};

template <typename AddressType>
void QuickenTableGenerator<AddressType>::DecodeEhFrameEntriesInstr(FrameInfo eh_frame_hdr_info,
        FrameInfo eh_frame_info, QutInstructionsOfEntries* entries_instructions,
        uint16_t regs_total) {

    shared_ptr<DwarfSectionDecoder<AddressType>> eh_frame;

    if (eh_frame_hdr_info.offset_ != 0) {

        QUT_DEBUG_LOG("QuickenTableGenerator::DecodeEhFrameEntriesInstr eh_frame_hdr_info.offset_ != 0");

        DwarfEhFrameWithHdrDecoder<AddressType>* eh_frame_hdr = new DwarfEhFrameWithHdrDecoder<AddressType>(memory_);
        eh_frame.reset(eh_frame_hdr);
        if (!eh_frame_hdr->EhFrameInit(eh_frame_info.offset_, eh_frame_info.size_, eh_frame_info.section_bias_) ||
            !eh_frame_hdr->Init(eh_frame_hdr_info.offset_, eh_frame_hdr_info.size_, eh_frame_hdr_info.section_bias_)) {
            eh_frame = nullptr;
        }
    }

    if (eh_frame.get() == nullptr && eh_frame_info.offset_ != 0) {

        QUT_DEBUG_LOG("QuickenTableGenerator::DecodeEhFrameEntriesInstr eh_frame_info.offset_ != 0");

        // If there is an eh_frame section without an eh_frame_hdr section,
        // or using the frame hdr object failed to init.
        eh_frame.reset(new DwarfEhFrameDecoder<AddressType>(memory_));
        if (!eh_frame->Init(eh_frame_info.offset_, eh_frame_info.size_, eh_frame_info.section_bias_)) {
            eh_frame = nullptr;
        }
    }

    if (eh_frame) {
        eh_frame->IterateAllEntries(regs_total, process_memory_, entries_instructions);
    }

    return;
}

template <typename AddressType>
void QuickenTableGenerator<AddressType>::DecodeExidxEntriesInstr(FrameInfo arm_exidx_info,
        QutInstructionsOfEntries* entries_instructions) {

    uint64_t start_offset = arm_exidx_info.offset_;
    uint64_t total_entries = arm_exidx_info.size_;

    QUT_DEBUG_LOG("DecodeExidxEntriesInstr start_offset %llu, total_entries %llu", start_offset, total_entries);

    if (total_entries <= 1) {
        // TODO How about this.
        return;
    }

    shared_ptr<deque<uint64_t>> curr_instructions;
    uint32_t start_addr = 0;

    for (size_t i = 0; i < total_entries; i++) { // TODO How about last entry.

        uint32_t addr;
        uint32_t entry_offset = start_offset + i * 8;

        // Read entry
        if (!GetPrel31Addr(entry_offset, &addr)) {
            QUT_DEBUG_LOG("DecodeExidxEntriesInstr GetPrel31Addr bad entry");
            // TODO bad entry
            continue;
        }

        if (i == total_entries - 1) {
            if (curr_instructions) {
                (*entries_instructions)[start_addr] = std::make_pair(addr, curr_instructions);
                continue;
            }
        }

        ExidxDecoder decoder(memory_, process_memory_);

//        if (addr == 0x102348) {
//            decoder.log = true;
//        }
        // Extract data, evaluate instructions and re-encode it.
        if (decoder.ExtractEntryData(entry_offset) && decoder.Eval()) {

            if (start_addr == 0) {
                start_addr = addr;
                curr_instructions = move(decoder.instructions_);
                continue;
            }

            // Well done.
            bool same_instr = false;
            if (curr_instructions) {
                // Compare two instructions
                if (curr_instructions->size() == decoder.instructions_->size()) {
                    same_instr = true;
                    for (size_t i = 0; i < curr_instructions->size(); i++) {
                        if (curr_instructions->at(i) != decoder.instructions_->at(i)) {
                            same_instr = false;
                            break;
                        };
                    }
                }
            }

            // Merge same entry
            if (same_instr) {
                // No need save this entry
                continue;
            } else if (!curr_instructions) {
                start_addr = addr;
                curr_instructions = move(decoder.instructions_);
                continue;
            } else {
                (*entries_instructions)[start_addr] = std::make_pair(addr, curr_instructions);
//                QUT_DEBUG_LOG("DecodeExidxEntriesInstr addr: %x, instructions: %u", addr, curr_instructions->size());
//                if (addr == 0x1025ac) {
//                    for (uint64_t instr : *curr_instructions.get()) {
//                        QUT_DEBUG_LOG("DecodeExidxEntriesInstr 0x1025ac, instructions: %llx", instr);
//                    }
//                }
                start_addr = addr;
                curr_instructions = move(decoder.instructions_);
            }

        } else {
            QUT_DEBUG_LOG("DecodeExidxEntriesInstr bad entry 3");
            // TODO bad entry
            continue;
        }
    }
    return;
}

template <typename AddressType>
void QuickenTableGenerator<AddressType>::DecodeDebugFrameEntriesInstr(FrameInfo debug_frame_info,
                                                         QutInstructionsOfEntries* entries_instructions, uint16_t regs_total) {

    if (debug_frame_info.offset_ != 0) {
        auto debug_frame_ = make_shared<DwarfDebugFrameDecoder<AddressType>>(memory_);
        if (!debug_frame_->Init(debug_frame_info.offset_, debug_frame_info.size_, debug_frame_info.section_bias_)) {
            return;
        }
        debug_frame_->IterateAllEntries(regs_total, process_memory_, entries_instructions);
    }

    return;
}

template <typename AddressType>
shared_ptr<QutInstructionsOfEntries> QuickenTableGenerator<AddressType>::MergeFrameEntries(
        shared_ptr<QutInstructionsOfEntries> to, shared_ptr<QutInstructionsOfEntries> from) {

    if (from->size() == 0) {
        return to;
    } else {
        if (to->size() == 0) {
            return from;
        }
    }

    auto result_frame_instructions = make_shared<QutInstructionsOfEntries>();

    auto to_it = to->begin();
    auto from_it = from->begin();

    uint64_t from_start;
    uint64_t from_end;

    while(to_it != to->end() || from_it != from->end()) {
//        QUT_DEBUG_LOG("QuickenTableGenerator<AddressType>::MergeFrameEntries looping");
        if (from_it == from->end()) {
//            QUT_DEBUG_LOG("MergeFrameEntries from_it == from->end()");
            (*result_frame_instructions)[to_it->first] = make_pair(to_it->second.first, to_it->second.second);
            to_it++;
            continue;
        }

        if (from_start == from_end) {
            from_start = from_it->first;
            from_end = from_it->second.first;
//            QUT_DEBUG_LOG("MergeFrameEntries from_start:%llx, from_end:%llx", from_start, from_end);
        }

        if (from_start == from_end) {
//            QUT_DEBUG_LOG("MergeFrameEntries from_start == from_end");
            break; // Unexpected.
        }

        if (to_it == to->end()) {
//            QUT_DEBUG_LOG("MergeFrameEntries to_it == to->end()");
            (*result_frame_instructions)[from_start] = make_pair(from_end, from_it->second.second);
            from_start = from_end;
            from_it++;
            continue;
        }

        if (from_end <= to_it->first) {
            (*result_frame_instructions)[from_start] = make_pair(from_end, to_it->second.second);
            from_start = from_end;
            from_it++;
        } else {
            if (from_start < to_it->first) {
                (*result_frame_instructions)[from_start] = make_pair(to_it->first, to_it->second.second);
            }
            if (from_end < to_it->second.first) {
                from_start = from_end;
                from_it++;
            } else {
                (*result_frame_instructions)[to_it->first] = make_pair(to_it->second.first, to_it->second.second);
                to_it++;
                from_start = to_it->second.first;
                if (from_end == from_start) {
                    from_it++;
                }
            }
        }

    }

    return result_frame_instructions;

}

#ifdef __arm__
#define QUT_TBL_ROW_SIZE 3  // 4 - 1
#else
#define QUT_TBL_ROW_SIZE 7  // 8 - 1
#endif

template <typename AddressType>
inline bool QuickenTableGenerator<AddressType>::PackEntriesToFutSections(
        QutInstructionsOfEntries* entries, QutSections* fut_sections) {

    deque<shared_ptr<TempEntryPair>> entries_encoded;

    size_t instr_tbl_count = 0;
    size_t bad_entries = 0;
    size_t bad_entries_count = 0;
    auto it = entries->begin();
    while(it != entries->end()) {

        auto entry_pair = make_shared<TempEntryPair>();

        entry_pair->entry_point = it->first;
//0x14fd10
if (entry_pair->entry_point == 0x13a0b8) {
        QUT_DEBUG_LOG("PackEntriesToFutSections 0x13a0b8 entry_pair->entry_point %x, instr %u", (uint32_t)entry_pair->entry_point, it->second.second->size());
}

        // re-encode it.
        if (QuickenInstructionsEncode(*it->second.second.get(), entry_pair->encoded_instructions)) {
            // Well done.
        } else {
            bad_entries_count++;
            // Error occurred if we reached here. Add QUT_END_OF_INS for this entry point.
            entry_pair->encoded_instructions.push_back(QUT_END_OF_INS_OP);
        }

        // Only instructions less or equal than #QUT_TBL_ROW_SIZE need a table entry.
        if (entry_pair->encoded_instructions.size() > QUT_TBL_ROW_SIZE) {
            instr_tbl_count += (entry_pair->encoded_instructions.size() + QUT_TBL_ROW_SIZE) / (QUT_TBL_ROW_SIZE + 1);
        }

//0x14fd10
if (entry_pair->entry_point == 0x13a0b8) {
    QUT_DEBUG_LOG("PackEntriesToFutSections 0x13a0b8 entry_pair->encoded_instructions.size() %llu", entry_pair->encoded_instructions.size());

    for (uint8_t insn : entry_pair->encoded_instructions) {
        QUT_DEBUG_LOG("PackEntriesToFutSections 0x13a0b8 instr " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(insn));
    }
}

        // Finally pushed.
        entries_encoded.push_back(entry_pair);

//        if (it->second.second->size() > QUT_TBL_ROW_SIZE) {
//            instr_tbl_count += ((it->second.second->size() + QUT_TBL_ROW_SIZE) / (QUT_TBL_ROW_SIZE + 1));
//        }
        it++;
    }

    // Init qut_sections
    size_t idx_size = 0;
    size_t tbl_size = 0;

    size_t temp_idx_capacity = entries_encoded.size() * 2; // Double size of total_entries.
    size_t temp_tbl_capacity = instr_tbl_count; // Half size of total_entries.
    uptr* temp_quidx = new uptr[temp_idx_capacity];
    uptr* temp_qutbl = new uptr[temp_tbl_capacity];

    // Compress instructions.
    while (!entries_encoded.empty()) {
        shared_ptr<TempEntryPair> current_entry = entries_encoded.front();
        entries_encoded.pop_front();

        // part 1.
        temp_quidx[idx_size++] = current_entry->entry_point;
        size_t size_of_deque = current_entry->encoded_instructions.size();
        // Less or equal than QUT_TBL_ROW_SIZE instructions can be compact in 2nd part of the index bit set.
        if (size_of_deque <= QUT_TBL_ROW_SIZE) {
            // Compact instruction has '1' in highest bit.
            uptr compact = 0x80L << (QUT_TBL_ROW_SIZE * 8);
            for (size_t i = 0; i < QUT_TBL_ROW_SIZE; i++) {
                size_t left_shift = (QUT_TBL_ROW_SIZE - 1 - i) * 8;
                if (size_of_deque > i) {
                    compact |= (((uint64_t)(current_entry->encoded_instructions.at(i))) << left_shift);
                } else {
                    compact |= (((uint64_t)(QUT_END_OF_INS_OP)) << left_shift);
//                    break;
                }
            }

            // part 2.
            temp_quidx[idx_size++] = compact;
        } else {

            size_t last_tbl_size = tbl_size;
            // TODO overflow check

            uptr row = 0;
            uint32_t size_ceil = (size_of_deque + QUT_TBL_ROW_SIZE) & (0xffffffff - QUT_TBL_ROW_SIZE);

//            QUT_DEBUG_LOG("PackEntriesToFutSections size_of_deque %u size_ceil %u", size_of_deque, size_ceil);
            for (size_t i = 0; i < size_ceil; i++) {

                // Assembling every 4 instructions into a row(32-bit).
                // Assembling every 8 instructions into a row(64-bit).
                size_t left_shift = (QUT_TBL_ROW_SIZE - (i % (QUT_TBL_ROW_SIZE + 1))) * 8;
                if (size_of_deque > i) {
                    row |= (((uint64_t)(current_entry->encoded_instructions.at(i))) << left_shift);
                } else {
                    // Padding QUT_END_OF_INS behind.
                    row |= (((uint64_t)(QUT_END_OF_INS_OP)) << left_shift);
                }

                if (left_shift == 0) {
                    temp_qutbl[tbl_size++] = row;
                    row = 0;
                }
            }

            size_t row_count = tbl_size - last_tbl_size;

            CHECK(row_count <= 0x7f);
            CHECK(last_tbl_size <= 0xffffff);

//            QUT_DEBUG_LOG("PackEntriesToFutSections row_count %u, last_tbl_size %u", row_count, last_tbl_size);

            uptr combined = (row_count & 0x7f) << (QUT_TBL_ROW_SIZE * 8); // Appending row count.
            combined |= (last_tbl_size & 0xffffff); // Appending entry table offset at last 3 bytes.

            // part 2.
            temp_quidx[idx_size++] = combined;
        }

        CHECK(idx_size <= temp_idx_capacity);
        CHECK(tbl_size <= temp_tbl_capacity);
    }

    fut_sections->idx_capacity = temp_idx_capacity;
    fut_sections->tbl_capacity = temp_tbl_capacity;
    fut_sections->quidx = temp_quidx;
    fut_sections->qutbl = temp_qutbl;

    fut_sections->idx_size = idx_size;
    fut_sections->tbl_size = tbl_size;

    fut_sections->total_entries = idx_size / 2;

    bad_entries_count = bad_entries;      // TODO

    QUT_DEBUG_LOG("QuickenInterface::PackEntriesToFutSections idx_size %u, tbl_size %u", (uint32_t)idx_size, (uint32_t)tbl_size);

    return true;
}

template <typename AddressType>
bool QuickenTableGenerator<AddressType>::GenerateUltraQUTSections(
        FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
        FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info, FrameInfo gnu_debug_frame_info,
        FrameInfo arm_exidx_info, QutSections* fut_sections) {

    if (UNLIKELY(fut_sections == nullptr)) {
        return false;
    }

    auto debug_frame_instructions = make_shared<QutInstructionsOfEntries>();
    auto eh_frame_instructions = make_shared<QutInstructionsOfEntries>();
    auto gnu_debug_frame_instructions = make_shared<QutInstructionsOfEntries>();
    auto gnu_eh_frame_instructions = make_shared<QutInstructionsOfEntries>();

    uint16_t regs_total = REGS_TOTAL;
    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections debug_frame_info size_:%llu, offset:%llu, section_bias:%llu",
            debug_frame_info.size_, debug_frame_info.offset_, debug_frame_info.section_bias_);
    DecodeDebugFrameEntriesInstr(debug_frame_info, debug_frame_instructions.get(), regs_total);
    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections debug_frame_instructions %u", (uint32_t)debug_frame_instructions->size());

    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections eh_frame_hdr_info size_:%llu, offset:%llu, section_bias:%llu",
                  eh_frame_hdr_info.size_, eh_frame_hdr_info.offset_, eh_frame_hdr_info.section_bias_);
    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections eh_frame_info size_:%llu, offset:%llu, section_bias:%llu",
                  eh_frame_info.size_, eh_frame_info.offset_, eh_frame_info.section_bias_);
    DecodeEhFrameEntriesInstr(eh_frame_hdr_info, eh_frame_info, eh_frame_instructions.get(), regs_total);
    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections eh_frame_instructions %u", (uint32_t)eh_frame_instructions->size());

    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections gnu_debug_frame_info size_:%llu, offset:%llu, section_bias:%llu",
                  gnu_debug_frame_info.size_, gnu_debug_frame_info.offset_, gnu_debug_frame_info.section_bias_);
    DecodeDebugFrameEntriesInstr(gnu_debug_frame_info, gnu_debug_frame_instructions.get(), regs_total);
    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections gnu_debug_frame_instructions %u", (uint32_t)gnu_debug_frame_instructions->size());

    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections gnu_eh_frame_hdr_info size_:%llu, offset:%llu, section_bias:%llu",
                  gnu_eh_frame_hdr_info.size_, gnu_eh_frame_hdr_info.offset_, gnu_eh_frame_hdr_info.section_bias_);
    DecodeEhFrameEntriesInstr(gnu_eh_frame_hdr_info, gnu_eh_frame_info, gnu_eh_frame_instructions.get(), regs_total);
    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections gnu_eh_frame_instructions %u", (uint32_t)gnu_eh_frame_instructions->size());

    shared_ptr<QutInstructionsOfEntries> merged;
    merged = MergeFrameEntries(debug_frame_instructions, eh_frame_instructions);
    merged = MergeFrameEntries(merged, gnu_debug_frame_instructions);
    merged = MergeFrameEntries(merged, gnu_eh_frame_instructions);

    if (arm_exidx_info.size_ != 0) {
        auto exidx_instructions = make_shared<QutInstructionsOfEntries>();
        DecodeExidxEntriesInstr(arm_exidx_info, exidx_instructions.get());
        QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections exidx_instructions %u", (uint32_t)exidx_instructions->size());
        merged = MergeFrameEntries(merged, exidx_instructions);
    }

    QUT_DEBUG_LOG("QuickenInterface::GenerateUltraQUTSections merged %u", (uint32_t)merged->size());
    PackEntriesToFutSections(merged.get(), fut_sections);

    return true;
}

//template <typename AddressType>
//bool QuickenTableGenerator<AddressType>::GenerateFutSections(size_t start_offset, size_t total_entries,
//        QutSections* fut_sections, size_t &bad_entries_count) {
//
//    if (fut_sections == nullptr) {
//        return false;
//    }
//
//    deque<shared_ptr<TempEntryPair>> entries_encoded;
//
//    size_t instr_tbl_count = 0;
//    TempEntryPair* prev_entry = nullptr;
//    size_t bad_entries = 0;
//    for (size_t i = 0; i < total_entries; i++) {
//
//        // Compute entry_offset
//        uint32_t entry_offset = start_offset + i * 8;
//        uint32_t addr = 0;
//
//        // Read entry
//        if (!GetPrel31Addr(entry_offset, &addr)) {
//            prev_entry = nullptr;
//            bad_entries++;
//            continue;
//        }
//
//        auto entry_pair = make_shared<TempEntryPair>();
//
//        entry_pair->entry_point = addr;
//
//        ExidxDecoder decoder(memory_, process_memory_);
//
//        // Extract data, evaluate instructions and re-encode it.
//        if (decoder.ExtractEntryData(entry_offset) && decoder.Eval()
//            && QuickenInstructionsEncode(*decoder.instructions_.get(), entry_pair->encoded_instructions)) {
//            // Well done.
//        } else {
//            bad_entries_count++;
//            // Error occurred if we reached here. Add QUT_END_OF_INS for this entry point.
//            entry_pair->encoded_instructions.push_back(QUT_END_OF_INS_OP);
//        }
//
//        bool same_instr = false;
//        if (prev_entry) {
//            // Compare two instructions
//            if (prev_entry->encoded_instructions.size() == entry_pair->encoded_instructions.size()) {
//                same_instr = true;
//                for (size_t i = 0; i < prev_entry->encoded_instructions.size(); i++) {
//                    if (prev_entry->encoded_instructions.at(i) != entry_pair->encoded_instructions.at(i)) {
//                        same_instr = false;
//                        break;
//                    };
//                }
//            }
//        }
//
//        // Merge same entry
//        if (same_instr) {
//            // No need save this entry
//            continue;
//        }
//
//        // Only instructions less or equal than 3 need a table entry.
//        if (entry_pair->encoded_instructions.size() > 3) {
//            instr_tbl_count += (entry_pair->encoded_instructions.size() + 3) / 4;
//        }
//
//        // Finally pushed.
//        entries_encoded.push_back(entry_pair);
//
//        prev_entry = entry_pair.get();
//    }
//
//    // Init qut_sections
//    size_t idx_size = 0;
//    size_t tbl_size = 0;
//
//    size_t temp_idx_capacity = entries_encoded.size() * 2; // Double size of total_entries.
//    size_t temp_tbl_capacity = instr_tbl_count; // Half size of total_entries.
//    uint32_t* temp_quidx = new uint32_t[temp_idx_capacity];
//    uint32_t* temp_qutbl = new uint32_t[temp_tbl_capacity];
//
//    // Compress instructions.
//    while (!entries_encoded.empty()) {
//        shared_ptr<TempEntryPair> current_entry = entries_encoded.front();
//        entries_encoded.pop_front();
//
//        // part 1.
//        temp_quidx[idx_size++] = current_entry->entry_point;
//        size_t size_of_deque = current_entry->encoded_instructions.size();
//        // Less or equal than 3 instructions can be compact in 2nd part of the index bit set.
//        if (size_of_deque <= 3) {
//            // Compact instruction has '1' in highest bit.
//            uint32_t compact = 0x80000000;
//            for (size_t i = 0; i < 3; i++) {
//                size_t left_shift = (2 - i) * 8;
//                if (size_of_deque > i) {
//                    compact |= current_entry->encoded_instructions.at(i) << left_shift;
//                } else {
//                    compact |= (QUT_END_OF_INS_OP << left_shift);
//                }
//            }
//
//            // part 2.
//            temp_quidx[idx_size++] = compact;
//        } else {
//
//            size_t last_tbl_size = tbl_size;
//
//            uint32_t row = 0;
//            uint32_t size_ceil = (size_of_deque + 0x3) & 0xfffffffc;
//            for (size_t i = 0; i < size_ceil; i++) {
//
//                // Assembling every 4 instructions into a row(32-bit).
//                size_t left_shift = (3 - (i % 4)) * 8;
//                if (size_of_deque > i) {
//                    row |= current_entry->encoded_instructions.at(i) << left_shift;
//                } else {
//                    // Padding QUT_END_OF_INS behind.
//                    row |= (QUT_END_OF_INS_OP << left_shift);
//                }
//
//                if (left_shift == 0) {
//                    temp_qutbl[tbl_size++] = row;
//                    row = 0;
//                }
//            }
//
//            size_t row_count = tbl_size - last_tbl_size;
//
//            CHECK(row_count <= 0x7f);
//            CHECK(last_tbl_size <= 0xffffff);
//
//            uint32_t combined = (row_count & 0x7f) << 24; // Appending row count.
//            combined |= (last_tbl_size & 0xffffff); // Appending entry table offset.
//
//            // part 2.
//            temp_quidx[idx_size++] = combined;
//        }
//
//        CHECK(idx_size <= temp_idx_capacity);
//        CHECK(tbl_size <= temp_tbl_capacity);
//    }
//
//    fut_sections->idx_capacity = temp_idx_capacity;
//    fut_sections->tbl_capacity = temp_tbl_capacity;
//    fut_sections->quidx = temp_quidx;
//    fut_sections->qutbl = temp_qutbl;
//
//    fut_sections->idx_size = idx_size;
//    fut_sections->tbl_size = tbl_size;
//
//    fut_sections->total_entries = idx_size / 2;
//
//    bad_entries_count = bad_entries;
//
//    return true;
//}

template <typename AddressType>
bool QuickenTableGenerator<AddressType>::GetPrel31Addr(uint32_t offset, uint32_t* addr) {
    uint32_t data;
    if (!memory_->Read32(offset, &data)) {
        return false;
    }

    // Sign extend the value if necessary.
    int32_t value = (static_cast<int32_t>(data) << 1) >> 1;
    *addr = offset + value;
    return true;
}

#define ReturnIndexOverflow(_i, _amount)  \
        if (_i >= _amount) { \
            return false; \
        }

#define NoReturnIndexOverflow(_i, _amount)

#define IterateNextByteIdx(_j, _i, _amount, _ReturnIndexOverflow)  \
    _j--; \
    if (_j < 0) { \
        _j = QUT_TBL_ROW_SIZE; \
        _i++; \
        _ReturnIndexOverflow(_i, _amount) \
    }

#define ReadByte(_byte, _instructions, _j, _i, _amount) \
    _byte = _instructions[_i] >> (8 * _j) & 0xff; \
    IterateNextByteIdx(_j, _i, _amount, NoReturnIndexOverflow)

#define DecodeSLEB128(_value, _instructions, _amount, _j, _i) \
    { \
        unsigned Shift = 0; \
        uint8_t Byte; \
        do { \
            IterateNextByteIdx(_j, _i, _amount, ReturnIndexOverflow) \
            ReadByte(Byte, _instructions, _j, _i, _amount) \
            _value |= (uint64_t(Byte & 0x7f) << Shift); \
            Shift += 7; \
        } while (Byte >= 0x80); \
        if (Shift < 64 && (Byte & 0x40)) _value |= (-1ULL) << Shift; \
    }

// TODO this is 32 bit
inline bool QuickenTable::Decode32(const uint32_t* instructions, const size_t amount, const size_t start_pos) {

    QUT_DEBUG_LOG("QuickenTable::Decode32 instructions %x, amount %u, start_pos %u", instructions, (uint32_t)amount, (uint32_t)start_pos);

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wint-to-pointer-cast"

    int32_t j = start_pos;
    size_t i = 0;

    for (size_t m = 0; m < amount; m++) {
        QUT_DEBUG_LOG("QuickenTable::Decode instruction %x", instructions[m]);
    }

    while(i < amount) {
        uint8_t byte;
        ReadByte(byte, instructions, j, i, amount);
        QUT_DEBUG_LOG("Decode byte " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
        switch (byte >> 6) {
        case 0:
            // 00nn nnnn : vsp = vsp + (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
            cfa_ += (byte << 2);
            break;
        case 1:
            // 01nn nnnn : vsp = vsp - (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
            cfa_ -= (byte << 2);
            break;
        case 2:
            switch (byte) {
                case QUT_INSTRUCTION_VSP_SET_BY_R7_OP:
                    cfa_ = R7(regs_);
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP:
                    cfa_ = R7(regs_) + 8;
                    LR(regs_) = *((uint32_t *)(cfa_ - 4));
                    R7(regs_) = *((uint32_t *)(cfa_ - 8));
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_R11_OP:
                    cfa_ = R11(regs_);
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP:
                    cfa_ = R11(regs_) + 8;
                    LR(regs_) = *((uint32_t *)(cfa_ - 4));
                    R11(regs_) = *((uint32_t *)(cfa_ - 8));
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
                    cfa_ = SP(regs_);
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
                    uint8_t byte;
                    ReadByte(byte, instructions, j, i, amount);
//                    cfa_ = *((uint32_t *)(R10(regs_) + ((byte & 0x7f) << 2)));
                    cfa_ = R10(regs_) + ((byte & 0x7f) << 2);

                    QUT_DEBUG_LOG("Decode QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP %x r10 %x, cfa_ %x, offset %x, " BYTE_TO_BINARY_PATTERN,
                            (uint32_t)byte, R10(regs_), cfa_, (uint32_t)((byte & 0x7f) << 2), BYTE_TO_BINARY(byte));
                    break;
                }
                case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
                    int64_t value = 0;// TODO overflow
                    DecodeSLEB128(value, instructions, amount, j, i)
                    cfa_ = value;
                    break;
                }
                case QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP: {
                    uint8_t byte;
                    ReadByte(byte, instructions, j, i, amount);
                    cfa_ = R7(regs_) + ((byte & 0x7f) << 2);
                    break;
                }
                case QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP: {
                    uint8_t byte;
                    ReadByte(byte, instructions, j, i, amount);
                    cfa_ = R11(regs_) + ((byte & 0x7f) << 2);
                    break;
                }
                case QUT_INSTRUCTION_DEX_PC_SET_OP: {
                    QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_DEX_PC_SET_OP");
                    dex_pc_ = R4(regs_);
                    break;
                }
                case QUT_END_OF_INS_OP: {
                    QUT_DEBUG_LOG("Decode byte QUT_END_OF_INS_OP");
                    return true;
                }
                case QUT_FINISH_OP: {
                    QUT_DEBUG_LOG("Decode byte QUT_FINISH_OP");
                    PC(regs_) = 0;
                    LR(regs_) = 0;
                    return true;
                }
                default: {
                    switch (byte & 0xf0) {
                        case QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX: {
                            R4(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                            break;
                        }
                        case QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX: {
                            R7(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                            break;
                        }
                    }
                }
            }
            break;
        default:
            switch (byte & 0xf0) {
                case QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX:
                    R10(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                    break;
                case QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX:
                    R11(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                    break;
                case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
                    LR(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                    break;
                default:
                    uint64_t value = 0;
                    switch (byte) {
                        case QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            R7(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            R10(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            R11(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            SP(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            LR(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            PC(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            pc_set_ = true;
                            break;
                        case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP:
                            DecodeSLEB128(value, instructions, amount, j, i)
                            cfa_ += (uint32_t)value;
                            break;
                        default:
                            QUT_DEBUG_LOG("Decode byte UNKNOWN");
                            return false;
                    }
                    break;
            }
            break;
        }
    }

    #pragma clang diagnostic pop

    return true;
}

inline bool QuickenTable::Decode64(const uint64_t* instructions, const size_t amount, const size_t start_pos) {

    QUT_DEBUG_LOG("QuickenTable::Decode64 instructions %llx, amount %llu, start_pos %llu", instructions, amount, start_pos);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"

    int32_t j = start_pos;
    size_t i = 0;

    for (size_t m = 0; m < amount; m++) {
        QUT_DEBUG_LOG("QuickenTable::Decode64 instruction %llx", instructions[m]);
    }

    while(i < amount) {
        uint8_t byte;
        ReadByte(byte, instructions, j, i, amount);
        QUT_DEBUG_LOG("Decode byte " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
        switch (byte >> 6) {
            case 0:
                // 00nn nnnn : vsp = vsp + (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
                cfa_ += (byte << 2);
                break;
            case 1:
                // 01nn nnnn : vsp = vsp - (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
                cfa_ -= (byte << 2);
                break;
            case 2:
                switch (byte) {
                    case QUT_INSTRUCTION_VSP_SET_BY_X29_OP:
                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_VSP_SET_BY_X29_OP");
                        cfa_ = R29(regs_);
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_X29_PROLOGUE_OP:
                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_VSP_SET_BY_X29_PROLOGUE_OP");
                        cfa_ = R29(regs_) + 16;
                        LR(regs_) = *((uint64_t *)(cfa_ - 8));
                        R29(regs_) = *((uint64_t *)(cfa_ - 16));
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_VSP_SET_BY_SP_OP");
                        cfa_ = SP(regs_);
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
                        uint8_t byte;
                        ReadByte(byte, instructions, j, i, amount);
                        cfa_ = R28(regs_) + ((byte & 0x7f) << 2);

                        QUT_DEBUG_LOG("Decode QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP %x x28 %llx, cfa_ %x, offset %x, " BYTE_TO_BINARY_PATTERN,
                                      (uint32_t)byte, R28(regs_), cfa_, (uint32_t)((byte & 0x7f) << 2), BYTE_TO_BINARY(byte));
                        break;
                    }
                    case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
                        int64_t value = 0; // TODO overflow
                        DecodeSLEB128(value, instructions, amount, j, i)
                        cfa_ = value;
                        break;
                    }
                    case QUT_INSTRUCTION_VSP_SET_BY_X29_IMM_OP: {
                        uint8_t byte;
                        ReadByte(byte, instructions, j, i, amount);
                        QUT_DEBUG_LOG("Decode before QUT_INSTRUCTION_VSP_SET_BY_X29_IMM_OP cfa_ %llx, R29(regs_) %llx, computed %llx", cfa_, R29(regs_), R29(regs_) + ((byte & 0x7f) << 2));
                        cfa_ = R29(regs_) + ((byte & 0x7f) << 2);
                        break;
                    }
                    case QUT_INSTRUCTION_DEX_PC_SET_OP: {
                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_DEX_PC_SET_OP");
                        dex_pc_ = R20(regs_);
                        break;
                    }
                    case QUT_END_OF_INS_OP: {
                        QUT_DEBUG_LOG("Decode byte QUT_END_OF_INS_OP");
                        return true;
                    }
                    case QUT_FINISH_OP: {
                        QUT_DEBUG_LOG("Decode byte QUT_FINISH_OP");
                        PC(regs_) = 0;
                        LR(regs_) = 0;
                        return true;
                    }
                    default: {
                        switch (byte & 0xf0) {
                            case QUT_INSTRUCTION_X20_OFFSET_OP_PREFIX: {
                                R20(regs_) = *((uint64_t *)(cfa_ - ((byte & 0xf) << 2)));
                                break;
                            }
//                            case QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX: {
//                                R29(regs_) = *((uint64_t *)(cfa_ - ((byte & 0xf) << 2)));
//                                break;
//                            }
                        }
                    }
                }
                break;
            default:
                switch (byte & 0xf0) {
                    case QUT_INSTRUCTION_X28_OFFSET_OP_PREFIX:
                        R28(regs_) = *((uint64_t *)(cfa_ - ((byte & 0xf) << 2)));
                        break;
                    case QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX:
                        QUT_DEBUG_LOG("Decode before QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX cfa_ %llx, computed %llx, R29(regs_) %llx", cfa_, (cfa_ - ((byte & 0xf) << 2)), *((uint64_t *)(cfa_ - ((byte & 0xf) << 2))));
                        R29(regs_) = *((uint64_t *)(cfa_ - ((byte & 0xf) << 2)));
                        QUT_DEBUG_LOG("Decode QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX");
                        if (R29(regs_) == 0) {  // reach end
                            return true;
                        }
                        break;
                    case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
                        LR(regs_) = *((uint64_t *)(cfa_ - ((byte & 0xf) << 2)));
                        break;
                    default:
                        uint64_t value = 0;
                        switch (byte) {
                            case QUT_INSTRUCTION_X28_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                R28(regs_) = *((uint64_t *)(cfa_ - (uint64_t)value));
                                break;
                            case QUT_INSTRUCTION_X29_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                R29(regs_) = *((uint64_t *)(cfa_ - (uint64_t)value));
                                if (R29(regs_) == 0) { // reach end
                                    return true;
                                }
                                break;
                            case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                SP(regs_) = *((uint64_t *)(cfa_ - (uint64_t)value));
                                break;
                            case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                LR(regs_) = *((uint64_t *)(cfa_ - (uint64_t)value));
                                break;
                            case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                PC(regs_) = *((uint64_t *)(cfa_ - (uint64_t)value));
                                pc_set_ = true;
                                break;
                            case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                cfa_ += (uint64_t)value;
                                break;
                            default:
                                QUT_DEBUG_LOG("Decode byte UNKNOWN");
                                return false;
                        }
                        break;
                }
                break;
        }
    }

#pragma clang diagnostic pop

    return true;
}

inline bool QuickenTable::Decode(const uptr* instructions, const size_t amount, const size_t start_pos) {
#ifdef __arm__
    return Decode32(instructions, amount, start_pos);
#else
    return Decode64(instructions, amount, start_pos);
#endif
}

bool QuickenTable::Eval(size_t entry_offset) {

    uptr command = fut_sections_->quidx[entry_offset + 1];

    QUT_DEBUG_LOG("QuickenTable::Eval entry_offset %u, add %llx, command %llx",
            (uint32_t)entry_offset, (uint64_t)fut_sections_->quidx[entry_offset],
            (uint64_t)fut_sections_->quidx[entry_offset + 1]);
//    QUT_DEBUG_LOG("QuickenTable::Eval command >> (sizeof(uptr) * 8 - 1) : %llu", command >> (sizeof(uptr) * 8 - 1));
    if (command >> (sizeof(uptr) * 8 - 1)) {
        return Decode(&command, 1, QUT_TBL_ROW_SIZE - 1); // compact
    } else {
        size_t row_count = (command >> ((sizeof(uptr) - 1) * 8)) & 0x7f;
        size_t row_offset = command & 0xffffff;
        QUT_DEBUG_LOG("QuickenTable::Eval row_count %u, row_offset %u", (uint32_t)row_count, (uint32_t)row_offset);
        return Decode(&fut_sections_->qutbl[row_offset], row_count, QUT_TBL_ROW_SIZE);
    }
}

//template class QuickenTableGenerator<uint32_t>;
//template class QuickenTableGenerator<uint64_t>;

//template bool QuickenTableGenerator<uint32_t>::GenerateFutSections(size_t start_offset, size_t total_entries, QutSections* fut_sections, size_t &bad_entries_count);
template bool QuickenTableGenerator<uint32_t>::GenerateUltraQUTSections(
        FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
        FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info, FrameInfo gnu_debug_frame_info,
        FrameInfo arm_exidx_info, QutSections* fut_sections);
template bool QuickenTableGenerator<uint64_t>::GenerateUltraQUTSections(
        FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
        FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info, FrameInfo gnu_debug_frame_info,
        FrameInfo arm_exidx_info, QutSections* fut_sections);

}  // namespace wechat_backtrace
