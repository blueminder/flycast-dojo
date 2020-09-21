#include "reios_dbg.h"
#include "reios/reios_dbg_module_stubs.h"
#include "debugger_cpp/shared_contexts.h"
#include "hw/sh4/sh4_opcode_list.h"
#include <sstream>
#include <stack>
#include <mutex>


#if HOST_OS != OS_WINDOWS
#include <dlfcn.h>
#endif
#include "libswirl.h"
#include "reios_syscalls.h"
#include "hw/sh4/sh4_mem.h"

bool g_reios_dbg_enabled = true;

static uint32_t last_op = -1U;
static cpu_ctx_t cpu_context;
static cpu_diss_t cpu_diss;

static void* p_mod_handle = nullptr;

static bool b_stop = false;
extern sh4_opcodelistentry* OpDesc[0x10000];
extern SuperH4* sh4_cpu;

static bool dummy_debugger_entry_point(int argc, char** argv, void((*event_func) (const char* which, const char* state))) { return true; }
static bool dummy_debugger_running() { return true; }
static bool dummy_debugger_shutdown() { return true; }
static bool dummy_debugger_pass_data(const char* class_name, void* data, const uint32_t size) { return true; }
static bool dummy_debugger_push_event(const char* which, const char* data){ return true; }
bool (*debugger_entry_point)(int argc, char** argv, void((*event_func) (const char* which, const char* state))) = &dummy_debugger_entry_point;
bool (*debugger_running)() = &dummy_debugger_running;
bool (*debugger_shutdown)() = &dummy_debugger_shutdown;
bool (*debugger_pass_data)(const char* class_name, void* data, const uint32_t size) = &dummy_debugger_pass_data;
bool (*debugger_push_event)(const char* which, const char* data) = &dummy_debugger_push_event;

struct syscall_nest_level_t { uint32_t freq; uint32_t is_exit; syscall_nest_level_t() : freq(0),is_exit(0){} };
std::unordered_map<uint32_t,syscall_nest_level_t> syscalls_nest_lvl;

static inline constexpr bool is_ins_jmp_type(const uint32_t ins) {
	return false;
}
 
template <typename base_t>
static inline void conv_str_to_scalar(const char* s,base_t& res,const bool is_hex = false) {
		std::stringstream ss;
		ss << std::hex << s;
		ss >> res;
}

template <typename base_t>
static inline void conv_str_to_scalar(const std::string& s,base_t& res,const bool is_hex = false) {
	conv_str_to_scalar(s.c_str(),res,is_hex);
}

void dgb_on_event(const char* which, const char* state) {
	if (strcmp(which, "dbg_enable") == 0) {
		if (strcmp(state, "true") == 0) 
			g_reios_dbg_enabled = true;
		else
			g_reios_dbg_enabled = false;
	} else if (strcmp(which, "set_reg") == 0) {

		//printf("SET REG : %s  \n", state );
		std::string tmp = std::string(state);
		size_t i = tmp.find(':');
		if (i != std::string::npos) {
			std::string reg = tmp.substr(0,i);
			std::string data = tmp.substr(i+1, tmp.length() - (i+1));
			printf("Set REG (%s) => (%s)\n",reg.c_str(),data.c_str());
			const char rtype = std::tolower(reg[0]);

			switch (rtype) {
				case 'r': 
					reg = reg.substr(1,reg.length());
					conv_str_to_scalar(data,Sh4cntx.r[std::stoi(reg)],true);
					break;
				case 'f': 
					reg = reg.substr(2,reg.length());
					conv_str_to_scalar(data,Sh4cntx.xffr[std::stoi(reg)]);
					break;
				case 's': 
					conv_str_to_scalar(data,Sh4cntx.spc,true);
					break;
				case 'p': 
					conv_str_to_scalar(data,(std::tolower(reg[1]) == 'r') ? Sh4cntx.pr : Sh4cntx.pc,true);
					break;
				case 't':
					conv_str_to_scalar(data,Sh4cntx.sr.T,true);
					break;
			}
		}
	} else if (strcmp(which, "set_pc") == 0) {
		conv_str_to_scalar(state,Sh4cntx.pc,true);
		printf("SET PC : %s (=> 0x%08x)\n", state, Sh4cntx.pc);
		Sh4cntx.pr = Sh4cntx.pc;
	} else if (strcmp(which, "step") == 0) {
		//if (!b_stop)
			//return;
		if (!sh4_cpu)
			return;
		else if (!virtualDreamcast)
			return;
		if (sh4_cpu->IsRunning())
			virtualDreamcast->Stop([] {});

		virtualDreamcast->Step();
	} else if (strcmp(which, "stop") == 0) {
		if (!sh4_cpu)
			return;
		else if (!virtualDreamcast)
			return;
		if (sh4_cpu->IsRunning())
			virtualDreamcast->Stop([]{  });
		b_stop = true;
	} else if (strcmp(which, "start") == 0) {
		if (!virtualDreamcast)
			return;
		if (b_stop || (!sh4_cpu->IsRunning())) {
			virtualDreamcast->Resume();
			b_stop = false;
		} else {
			virtualDreamcast->RequestReset();
		}
	} else if (strcmp(which, "reset") == 0) {
		b_stop = false;
		if (!virtualDreamcast)
			return;
		virtualDreamcast->RequestReset();
	}
}

//Called BEFORE execution of given opcode

template <const bool emit=false>
bool check_scs(const uint32_t pc) {
	if (g_syscalls_mgr.is_syscall(pc)) {
		//printf("Found SC!! 0x%x\n",pc);
		auto tmp = syscalls_nest_lvl.find(pc);
		if (tmp == syscalls_nest_lvl.end()) {
			syscall_nest_level_t snl;
			snl.freq = 1;
			snl.is_exit = 0;
			syscalls_nest_lvl.insert({pc ,snl}); 
			tmp = syscalls_nest_lvl.find(pc + 2);
			if (tmp == syscalls_nest_lvl.end()) {
				snl.is_exit = 1;
				syscalls_nest_lvl.insert({pc + 2,snl}); 
			} else ++tmp->second.freq;

			if (emit)
				debugger_pass_data("syscall_enter", nullptr,0);
		} else {
			if (tmp->second.is_exit != 0) {
				++tmp->second.freq;
				if (emit)
					debugger_pass_data("syscall_leave", (void*)&pc,sizeof(pc));
			} else {
				++tmp->second.freq;
				if (emit)
					debugger_pass_data("syscall_enter",  (void*)&pc,sizeof(pc));
			}
		}
		return true;
	}
	return false;
}



void reios_dbg_diss_range(std::vector<std::string>& buf,const uint32_t start_pc,const uint32_t end_pc) {
	buf.clear();

	for (uint32_t i = start_pc;i < end_pc;i += 2) {
		const uint16_t op = ReadMem16(i);
		buf.push_back( std::move ( disassemble_op(OpDesc[op]->diss, i, op)) );
	}
}

void reios_dbg_diss_range_bw(std::vector<std::string>& buf,const uint32_t start_pc,const uint32_t end_pc) {
	buf.clear();

	for (uint32_t i = end_pc;i >= start_pc;i -= 2) {
		const uint16_t op = ReadMem16(i);
		buf.push_back( std::move ( disassemble_op(OpDesc[op]->diss, i, op)) );
	}
}

void reios_dbg_begin_op(const uint32_t pc, const uint32_t opcode) {
	bool is_sc = false;

	{
		class ee { public:
			FILE* fp;
			public:
			ee() {
				fp = fopen("out.txt","wb");
			}


			~ee() {
				fclose(fp);
			}
		};
		static ee e;
 
		std::string tmp = disassemble_op(OpDesc[opcode]->diss, pc, opcode);
		fprintf(e.fp,"0x%08x:%s\n",pc,tmp.c_str());
		
	}
	if (!g_reios_dbg_enabled) {
		if (! (is_sc = check_scs<false>(pc))) {
			return;
		}
	}

	last_op = opcode;

	cpu_context.pc = pc;
	cpu_context.pr = Sh4cntx.pr;
	cpu_context.sp = Sh4cntx.spc;
	cpu_diss.pc = pc;
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
 
	check_scs<true>(pc);
}

//Called AFTER execution of given opcode
void reios_dbg_end_op(const uint32_t pc, const uint32_t opcode) {
 
}

const uint32_t reios_dbg_get_last_op() {
	return last_op;
}

bool reios_dbg_init() {

#if HOST_OS == OS_WINDOWS
	return false;
#elif
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
#endif
}

bool reios_dbg_shutdown() {

#if HOST_OS == OS_WINDOWS
	return false;
#elif
	if (p_mod_handle)
		dlclose(p_mod_handle);

	p_mod_handle = nullptr;
	return true;
#endif
}
