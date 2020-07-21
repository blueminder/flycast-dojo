#include "reios_dbg.h"
#include "reios/reios_dbg_module_stubs.h"
#include "debugger_cpp/shared_contexts.h"
#include "hw/sh4/sh4_opcode_list.h"
#include <sstream>
#include <stack>
#include <mutex>
#include <dlfcn.h>
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

static void* p_mod_handle = nullptr;

static bool b_stop = false;
extern sh4_opcodelistentry* OpDesc[0x10000];
extern SuperH4* sh4_cpu;

bool dummy_debugger_entry_point(int argc, char** argv, void((*event_func) (const char* which, const char* state))) { return true; }
bool dummy_debugger_running() { return true; }
bool dummy_debugger_shutdown() { return true; }
bool dummy_debugger_pass_data(const char* class_name, void* data, const uint32_t size) { return true; }
bool dummy_debugger_push_event(const char* which, const char* data){ return true; }

bool (*debugger_entry_point)(int argc, char** argv, void((*event_func) (const char* which, const char* state))) = &dummy_debugger_entry_point;
bool (*debugger_running)() = &dummy_debugger_running;
bool (*debugger_shutdown)() = &dummy_debugger_shutdown;
bool (*debugger_pass_data)(const char* class_name, void* data, const uint32_t size) = &dummy_debugger_pass_data;
bool (*debugger_push_event)(const char* which, const char* data) = &dummy_debugger_push_event;



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
		strncpy(cpu_diss.buf, tmp.c_str(), sizeof(cpu_diss.buf));
		debugger_pass_data("cpu_diss", (void*)&cpu_diss, sizeof(cpu_diss));
	}
}

void reios_dbg_end_op(const uint32_t pc, const uint32_t opcode) {
 
}

const uint32_t reios_dbg_get_last_op() {
	return last_op;
}

bool reios_dbg_init() {
	if (p_mod_handle)
		dlclose(p_mod_handle);

	p_mod_handle = dlopen("./libdebugger.so",RTLD_NOW);
	if (!p_mod_handle) {
		//XXX Print smt
		return false;
	}

	void* entry = (void*) dlsym(p_mod_handle,"debugger_entry_point");
	void* shutdown = (void*) dlsym(p_mod_handle,"debugger_shutdown");
	void* running = (void*) dlsym(p_mod_handle,"debugger_running");
	void* pass_data = (void*) dlsym(p_mod_handle,"debugger_pass_data");
	void* push_event = (void*) dlsym(p_mod_handle,"debugger_push_event");

	if (entry != nullptr) {
		debugger_entry_point = (bool(*) (int argc, char** argv, void((*event_func) (const char* which, const char* state)))) entry;
	}

	if (running != nullptr) {
		debugger_running = (bool (*)())running;
	}

	if (shutdown != nullptr) {
		debugger_shutdown = (bool (*)())shutdown;
	}
	
	if (pass_data != nullptr) {
		debugger_pass_data = (bool (*)(const char* class_name, void* data, const uint32_t size))pass_data;
	}

	if (push_event != nullptr) {
		debugger_push_event = (bool (*)(const char* which, const char* data))push_event;
	}

	return true;
}

bool reios_dbg_shutdown() {
	if (p_mod_handle)
		dlclose(p_mod_handle);

	p_mod_handle = nullptr;
	return true;
}
