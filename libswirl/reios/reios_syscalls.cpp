#include "license/bsd"
#include "reios_syscalls.h"

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
	m_scs_rev.insert({syscall,name});
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

	g_syscalls_mgr.register_syscall("reios_boot",&reios_boot, 0xA0000000, k_invalid_syscall);
	g_syscalls_mgr.register_syscall("reios_sys_system", &reios_sys_system, 0x8C001000 , dc_bios_syscall_system);
	g_syscalls_mgr.register_syscall("reios_sys_font", &reios_sys_font, 0x8C001002, dc_bios_syscall_font);
	g_syscalls_mgr.register_syscall("reios_sys_flashrom", &reios_sys_flashrom, 0x8C001004 , dc_bios_syscall_flashrom);
	g_syscalls_mgr.register_syscall("reios_sys_gd", &reios_sys_gd, 0x8C001006, dc_bios_syscall_gd);
	g_syscalls_mgr.register_syscall("reios_sys_misc", &reios_sys_misc, 0x8C001008 , dc_bios_syscall_misc);
	g_syscalls_mgr.register_syscall("gd_do_bioscall", &gd_do_bioscall, 0x8c0010F0, k_invalid_syscall);

	/*
	g_syscalls_mgr.register_syscall("reios_exception_handler_100", nullptr, 0x8c000100, true);
	g_syscalls_mgr.register_syscall("reios_exception_handler_400", nullptr, 0x8c000400, true);
	g_syscalls_mgr.register_syscall("reios_reset", nullptr, 0xa0000116, true);
	g_syscalls_mgr.register_syscall("reios_memcmp_uint8_t", nullptr, 0x8c000548, true);
	g_syscalls_mgr.register_syscall("reios_ascii2char", nullptr, 0x8c00055e, true);
	g_syscalls_mgr.register_syscall("reios_convert_VGA", nullptr, 0x8c00056c, true);
	g_syscalls_mgr.register_syscall("reios_convert_WinCE", nullptr, 0x8c000570, true);*/
	return true;
}

bool reios_syscalls_shutdown() {
	return true;
}
