#include "license/bsd"
#include "reios_syscalls.h"
#include "hw/mem/_vmem.h"
#include "hw/sh4/sh4_mem.h"

//https://github.com/reicast/reicast-emulator/blob/37dba034a9fd0a03d8ebfd07c3c7ec85b76f0eb7/libswirl/hw/sh4/interpr/sh4_opcodes.cpp#L872

reios_syscall_mgr_c g_syscalls_mgr;

reios_syscall_mgr_c::reios_syscall_mgr_c() {

}

reios_syscall_mgr_c::~reios_syscall_mgr_c() {

}

bool reios_syscall_mgr_c::register_syscall(const std::string& name,reios_hook_fp* native_func,const uint32_t addr,const uint32_t syscall,const bool enabled) {
	lock();
	if (grab_syscall(name) != m_scs.end()) {
		unlock();
		return false;
	}
	m_scs_rev.insert({addr,name});
	m_scs.insert({ name,(reios_syscall_cfg_t(addr,syscall,enabled,native_func)) });
	unlock();
	return true;
}

/*
bool reios_syscall_mgr_c::register_syscall(std::string&& name, reios_hook_fp* native_func, const uint32_t addr, const bool enabled, const bool init_type) {
	lock();
	if (grab_syscall(name) != m_scs.end()) {
		unlock();
		return false;
	}
	m_scs.insert({ std::move(name),std::move(reios_syscall_cfg_t(addr,enabled,native_func,init_type)) });
	unlock();
	return true;
}*/

bool reios_syscall_mgr_c::activate_syscall(const std::string& name, const bool active) {
	lock();
	auto cfg = grab_syscall(name);
	if (cfg == m_scs.end()) {
		unlock();
		return false;
	}
	cfg->second.enabled = active;
	unlock();
	return true;
}

bool reios_syscall_mgr_c::is_activated(const std::string& name) {
	auto cfg = grab_syscall(name);
	if (cfg == m_scs.end()) {
		unlock();
		return false;
	}
	bool res = cfg->second.enabled;
	unlock();
	return res;
}

//Syscalls begin!!

static bool g_init_done = false;


constexpr auto lock_val = 0x8C001994;

void try_lock_gd_mutex() {
	//printf("From 0x%x to 0x%x\n", Sh4cntx.pc, Sh4cntx.pr);
	u8 old = ReadMem8(lock_val);
	WriteMem8(lock_val, old | 0x80);

	Sh4cntx.r[0] = old ? 1 : 0;
}

void release_lock_gd_mutex() {
	Sh4cntx.r[0] = lock_val;
	WriteMem8(lock_val, 0);
}

void gd_check_cmd_easy_mtx_release() {
	//8C003162
	release_lock_gd_mutex();
	Sh4cntx.r[0] = -1;
	Sh4cntx.pc = 0x8C00316A;
}

void get_reg_base_addr() {
	Sh4cntx.r[0] = 0xA05F7000;
	Sh4cntx.pc = Sh4cntx.pr;
}

bool reios_syscalls_init() {
	if (g_init_done)
		return true;

	g_init_done = true;

	extern void reios_boot();
	extern void reios_sys_system();
	extern void reios_sys_font();
	extern void reios_sys_flashrom();
	extern void reios_sys_gd();
	extern void reios_sys_misc();
	extern void gd_do_bioscall();


	g_syscalls_mgr.register_syscall("reios_boot", &reios_boot, 0xA0000000, k_invalid_syscall);

	//8C000776
	//g_syscalls_mgr.register_syscall("reios_sys_skip", &skip_me, 0x8C000776, 0x8C000776); //*0x8C000776 = go wher e? new 

	/*
	 
	 g_syscalls_mgr.register_syscall("reios_sys_font", &reios_sys_font, 0x8C001002, dc_bios_syscall_font);
	*/
	 //g_syscalls_mgr.register_syscall("reios_sys_gd", &reios_sys_gd, 0x8C001000, dc_bios_syscall_gd);
	//g_syscalls_mgr.register_syscall("reios_sys_system", &reios_sys_system, 0x8C001000, dc_bios_syscall_system);
	//g_syscalls_mgr.register_syscall("reios_sys_flashrom", &reios_sys_flashrom, 0x8C001004, dc_bios_syscall_flashrom);
	g_syscalls_mgr.register_syscall("reios_sys_misc", &reios_sys_misc, 0x8C000800, dc_bios_syscall_misc);
	//g_syscalls_mgr.register_syscall("gd_do_bioscall", &gd_do_bioscall, 0x8c0010F0, k_invalid_syscall);  
	g_syscalls_mgr.register_syscall("try_lock_gd_mutex", &try_lock_gd_mutex, 0x8C001970, k_no_syscall);
	g_syscalls_mgr.register_syscall("release_lock_gd_mutex", &release_lock_gd_mutex, 0x8C00197E, k_no_syscall);

	g_syscalls_mgr.register_syscall("get_reg_base_addr", &get_reg_base_addr, 0x8C001108, k_no_syscall);
	return true;
}

bool reios_syscalls_shutdown() {
	return true;
}
