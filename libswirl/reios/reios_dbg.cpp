#include "reios_dbg.h"
#include "reios/reios_dbg_module_stubs.h"
#include "debugger_cpp/shared_contexts.h"
#include "hw/sh4/sh4_opcode_list.h"
#include <sstream>
#include <stack>
#include <mutex>
#include "libswirl.h"

enum cpu_mode_e {
	cpu_mode_idle = 0,
	cpu_mode_step,
	cpu_mode_stop,
	cpu_mode_start,
	cpu_mode_reset,
	cpu_modes,
};

bool g_reios_dbg_enabled = true;

static uint32_t last_op = -1U;
static cpu_ctx_t cpu_context;
static cpu_diss_t cpu_diss;
 
static bool b_stop = false;

extern sh4_opcodelistentry* OpDesc[0x10000];
extern SuperH4* sh4_cpu;
 
static inline constexpr bool is_ins_jmp_type(const uint32_t ins) {
	return false;
}
 
void dgb_on_event(const char* which, const char* state) {
	if (strcmp(which, "dbg_enable") == 0) {
		if (strcmp(state, "true") == 0) 
			g_reios_dbg_enabled = true;
		else
			g_reios_dbg_enabled = false;
	} else if (strcmp(which, "set_pc") == 0) {
		
		std::stringstream ss;
		ss << std::hex << state;
		ss >> Sh4cntx.pc;

		printf("SET PC : %s (=> 0x%08x)\n", state, Sh4cntx.pc);
		Sh4cntx.pr = Sh4cntx.pc;
	} else if (strcmp(which, "step") == 0) {
		if (sh4_cpu->IsRunning())
			virtualDreamcast->Stop([] {});

		virtualDreamcast->Step();
	} else if (strcmp(which, "stop") == 0) {
		if (sh4_cpu->IsRunning())
			virtualDreamcast->Stop([]{  });
		b_stop = true;
	} else if (strcmp(which, "start") == 0) {
		if (b_stop) {
			virtualDreamcast->Resume();
			b_stop = false;
		} else {
			virtualDreamcast->RequestReset();
		}
	} else if (strcmp(which, "reset") == 0) {
		virtualDreamcast->RequestReset();
		b_stop = false;
	}
}

void reios_dbg_begin_op(const uint32_t pc, const uint32_t opcode) {
	if (!g_reios_dbg_enabled)
		return;

	last_op = opcode;

	cpu_context.pc = Sh4cntx.pc;
	cpu_context.pr = Sh4cntx.pr;
	cpu_context.sp = Sh4cntx.spc;
	cpu_diss.pc = Sh4cntx.pc;
	cpu_diss.op = opcode;

	for (size_t i = 0; i < 16; ++i) {
		cpu_context.r[i] = Sh4cntx.r[i];
		cpu_context.fr[i] = Sh4cntx.xffr[i]; //32 FP regs
		cpu_context.fr[16+i] = Sh4cntx.xffr[16+i];
	}

	//Critical : Update contexts!
	debugger_pass_data("cpu_ctx", &cpu_context, sizeof(cpu_context));

	//Pass diss
	if (OpDesc[opcode]->diss != nullptr) {
		std::string tmp = disassemble_op(OpDesc[opcode]->diss, pc, opcode);
		strncpy_s(cpu_diss.buf, tmp.c_str(), sizeof(cpu_diss.buf));
		debugger_pass_data("cpu_diss", (void*)&cpu_diss, sizeof(cpu_diss));
	}
}

void reios_dbg_end_op(const uint32_t pc, const uint32_t opcode) {
 
}

const uint32_t reios_dbg_get_last_op() {
	return last_op;
}