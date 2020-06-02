#pragma once
#include "license/bsd"
#include "types.h"
#include "reios_gdrom_q.h"
#include "reios_syscalls.h"
#include <unordered_map>

bool reios_init(u8* rom, u8* flash);

void reios_reset();

void reios_term();

void DYNACALL reios_trap(u32 op);

char* reios_disk_id();
extern char reios_software_name[129];



struct reios_context_t {
	char ip_bin[256];
	char reios_hardware_id[17];
	char reios_maker_id[17];
	char reios_device_info[17];
	char reios_area_symbols[9];
	char reios_peripherals[9];
	char reios_product_number[11];
	char reios_product_version[7];
	char reios_releasedate[17];
	char reios_boot_filename[17];
	char reios_software_company[17];
	char reios_software_name[129];
	char reios_bootfile[32];
	u8* biosrom;
	u8* flashrom;
	u32 base_fad;
	bool descrambl;
	bool reios_windows_ce;
	bool pre_init;
	gdrom_queue_manager_c gd_q;

	std::unordered_map<u32, reios_hook_fp*> hooks;
	std::unordered_map<reios_hook_fp*, u32> hooks_rev;
	u32 SecMode[4];

	reios_context_t() {
		base_fad = 45150;
		reios_windows_ce = descrambl = pre_init =  false;
		
	}

	~reios_context_t() {
		//reios_syscalls_shutdown();
	}

	inline constexpr uint32_t syscall_addr_map(const uint32_t addr) const {
		return ((addr & (uint32_t)0x1FFFFFFFU) | (uint32_t)0x80000000U);
	}

	void remove_all_hooks();
	bool register_all_hooks();
	bool apply_all_hooks();
	void reset();
	void register_hook(u32 pc, reios_hook_fp* fn);
	u32 hook_addr(reios_hook_fp* fn);
	void setup_syscall(u32 hook_addr, u32 syscall_addr);
};

extern reios_context_t g_reios_ctx;
#define REIOS_OPCODE 0x085B

 