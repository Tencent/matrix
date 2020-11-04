////
//// Created by Carl on 2020-09-21.
////
//
//#include <cstdint>
//#include <vector>
//#include <utility>
//#include <memory>
//#include <deque>
//#include "QuickenInstructions.h"
//
//
//
//namespace wechat_backtrace {
//
//using namespace std;
//
////typedef void(*Validation)(QutInstruction, uint64_t);
////
////enum OPType {
////    CMD,
////    CMD_IMM_MASK,
////    CMD_IMM_SLEB128,
////    CMD_IMM_MASK_OR_SLEB128,
////    CMD_IMM_ULEB128,
////    NOP,
////};
////
////struct QutInstrTable {
////    QutInstruction instr_;
////    OPType type_;
////    uint8_t op;
////    uint8_t prefix_;
////    uint8_t mask_;
////    uint8_t op2;
////    Validation validation_;
////};
////
////
////constexpr static QutInstrTable kQutInstrTable32[QUT_NOP]  = {
////    {QUT_INSTRUCTION_R4_OFFSET, CMD_IMM_MASK, OP(), 0, 0, 0, nullptr},
////    {QUT_INSTRUCTION_R7_OFFSET, CMD_IMM_MASK_OR_SLEB128, 0, 0, 0, 0, nullptr},
////    {QUT_INSTRUCTION_R10_OFFSET, CMD_IMM_MASK_OR_SLEB128, 0, 0, 0, 0, nullptr},
////    {QUT_INSTRUCTION_R11_OFFSET, CMD_IMM_MASK_OR_SLEB128, 0, 0, 0, 0, nullptr},
////    {QUT_NOP, NOP, 0, 0, nullptr},  //  64-bit
////    {QUT_NOP, NOP, 0, 0, nullptr},  //  64-bit
////    {QUT_NOP, NOP, 0, 0, nullptr},  //  64-bit
////    {QUT_INSTRUCTION_SP_OFFSET, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_LR_OFFSET, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_PC_OFFSET, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_VSP_OFFSET, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_VSP_SET_IMM, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_VSP_SET_BY_R7, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_VSP_SET_BY_R11, CMD, 0, 0, nullptr},
////    {QUT_NOP, NOP, 0, 0, nullptr},  //  64-bit
////    {QUT_INSTRUCTION_VSP_SET_BY_SP, CMD, 0, 0, nullptr},
////    {QUT_INSTRUCTION_VSP_SET_BY_JNI_SP, CMD, 0, 0, nullptr},
////    {QUT_END_OF_INS, CMD, 0, 0, nullptr},
////};
//
//
//}  // namespace wechat_backtrace
