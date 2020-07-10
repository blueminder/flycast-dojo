#include "reios_dbg.h"
#include "reios/reios_dbg_module_stubs.h"
bool g_reios_dbg_enabled = true;

static uint32_t last_op = -1U;

static inline constexpr bool is_ins_jmp_type(const uint32_t ins) {
	return false;
}

void reios_dbg_trace_op(const uint32_t pc, const uint32_t opcode) {
	last_op = opcode;

	//debugger_push_event("a", "b");
}

const uint32_t reios_dbg_get_last_op() {
	return last_op;
}