#pragma once
#include "types.h"

extern bool g_reios_dbg_enabled;

void reios_dbg_begin_op(const uint32_t pc, const uint32_t opcode);
void reios_dbg_end_op(const uint32_t pc, const uint32_t opcode);
void reios_dbg_diss_range_bw(std::vector<std::string>& buf,const uint32_t start_pc,const uint32_t end_pc);
void reios_dbg_diss_range(std::vector<std::string>& buf,const uint32_t start_pc,const uint32_t end_pc) ;
const uint32_t reios_dbg_get_last_op();
