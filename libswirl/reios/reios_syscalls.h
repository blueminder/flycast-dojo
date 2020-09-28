#pragma once
#include "license/bsd"
 
#include "types.h"
#include <unordered_map>

static constexpr uint32_t dc_bios_syscall_system = 0x8C0000B0;
static constexpr uint32_t dc_bios_syscall_font = 0x8C0000B4;
static constexpr uint32_t dc_bios_syscall_flashrom = 0x8C0000B8;
static constexpr uint32_t dc_bios_syscall_gd = 0x8C0000BC;
static constexpr uint32_t dc_bios_syscall_misc = 0x8c0000E0;
static constexpr uint32_t dc_bios_entrypoint_gd_do_bioscall = 0x8c0010F0; 
static constexpr uint32_t SYSINFO_ID_ADDR = 0x8C001010;
static constexpr uint32_t SYSINFO_ID_ADDR2 = 0x8C000068;
static constexpr uint32_t k_invalid_syscall = std::numeric_limits<uint32_t>::max();
static constexpr uint32_t k_no_vector_table = std::numeric_limits<uint32_t>::max() - 1;

struct reios_syscall_result_args_t {

};

typedef void reios_hook_t (reios_syscall_result_args_t&);

struct reios_syscall_t {
	const char* name;
	const uint32_t addr;
	const bool overrides_pc;
	const bool overrides_pr;
	reios_hook_t* fptr;
	uint32_t pc;
	uint32_t pr;
};

static reios_syscall_t funcs[] = {
	{"",0,NULL} ,
};

struct reios_syscall_cfg_t {
	uint32_t addr;
	uint32_t syscall;
	reios_hook_fp* fn;
	bool enabled;
	
	reios_syscall_cfg_t() :addr(0), syscall(0) , enabled(false) , fn(nullptr) {}
	reios_syscall_cfg_t(const uint32_t _addr,const uint32_t _syscall,const bool _enabled, reios_hook_fp* _fn) :addr(_addr),syscall(_syscall),fn(_fn), enabled(_enabled)  {}

	inline void operator=(const reios_syscall_cfg_t& other) {
		this->addr = other.addr;
		this->syscall = other.syscall;
		this->fn = other.fn;
		this->enabled = other.enabled;
	}
};

class reios_syscall_mgr_c {
private:
	std::unordered_map<std::string, reios_syscall_cfg_t > m_scs;
	std::unordered_map<uint32_t,std::string> m_scs_rev;

	inline auto grab_syscall(const std::string& name) {
		auto it = m_scs.find(name);
		return it;
	}

	inline void lock() {

	}

	inline void unlock() {

	}

public:
	reios_syscall_mgr_c();
	~reios_syscall_mgr_c();

	bool register_syscall(const std::string& name, reios_hook_fp* native_func,const uint32_t addr,const uint32_t syscall, const bool enabled = true);
	//bool register_syscall(std::string&& name, reios_hook_fp* native_func,const uint32_t addr, const bool enabled = true,const bool init_type=false);
	bool activate_syscall(const std::string& name, const bool active);
	bool is_activated(const std::string& name);
	inline constexpr const std::unordered_map<std::string, reios_syscall_cfg_t >& get_map() const {
		return this->m_scs;
	}

	inline bool is_syscall(const uint32_t addr) {
		return (m_scs_rev.find(addr) != m_scs_rev.end());
	}
};

extern reios_syscall_mgr_c g_syscalls_mgr;

bool reios_syscalls_init();
bool reios_syscalls_shutdown();