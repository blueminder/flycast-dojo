#include "bios_hle_hooks.hpp"
#include <unordered_map>
#include "hw/sh4/sh4_mem.h"
#include "libswirl.h"
#include "bios_hle_includes.hpp"

static std::unordered_map<uint32_t, hle_hook_stub_t* > hooks;
static std::unordered_map<hle_hook_stub_t*, uint32_t> hooks_rev;
static Sh4RCB* p_reg_ctx = nullptr;

#define SYSCALL_ADDR_MAP(addr) ((addr & 0x1FFFFFFF) | 0x80000000)

// 0xB .. .0x8...
template <typename base_t>
static inline constexpr const base_t syscall_addr_map(const base_t addr) {
	return (((addr) & (base_t)0x1FFFFFFF) | (base_t)0x80000000);
}

u16 bios_hle_hooks_register_function(uint32_t func_entry_point, hle_hook_stub_t* fn) {
	hooks[SYSCALL_ADDR_MAP(func_entry_point)] = fn;
	hooks_rev[fn] = func_entry_point;
	auto rv = ReadMem16(func_entry_point);
	WriteMem16(func_entry_point, SH4_EXT_OP_REIOS_V2_TRAP);

	printf("bios hle reg hook pc 0x%08x -> map 0x%08x : orign 0x%08x\n", func_entry_point, SYSCALL_ADDR_MAP(func_entry_point),rv);

	return rv;
}

void bios_hle_hooks_register_set_syscall(uint32_t syscall_vector, uint32_t function_address) {
	WriteMem32(syscall_vector, function_address);
	//WriteMem16(hook_addr, SH4_EXT_OP_REIOS_V2_TRAP);
}
 
uint32_t bios_hle_hooks_reverse(hle_hook_stub_t* fn) {
	auto rv = hooks_rev.find(fn);
	if (rv == hooks_rev.end()) {
		printf("hook_addr: Failed to reverse lookup %p\n", fn);
		return -1;
	}
	return rv->second;
}
 
extern u32 rehook_address;
extern u16 rehook_old_opcode;
extern u32 rehook_expected_pc;
extern bool bios_hle_func_8C0000BC_misc_gdrom_fn_2();
extern u16 bios_hle_func_8C0000BC_misc_gdrom_fn_original;

void DYNACALL bios_hle_hooks_dyna_trap(uint32_t op) {
	uint32_t pc = sh4rcb.cntx.pc - 2;
	uint32_t mapd = SYSCALL_ADDR_MAP(pc);

	//printf("OP AT 0x%08x : => 0x%08x CALLED from 0x%08x \n", pc, ReadMem16(pc), sh4rcb.cntx.pr);
	hooks[mapd]();
	return;

	
	if (rehook_address) {
		 printf("reios: rehook\n");

		 printf("REGS_LLE: AFTER CALL!");
		 for (int i = 0; i < 16; ++i)
			 printf("r[%d]:%d ", i, sh4rcb.cntx.r[i]);
		 printf("\n");

		WriteMem16(rehook_address, SH4_EXT_OP_REIOS_V2_TRAP);
		WriteMem16(mapd , rehook_old_opcode);
		rehook_address = 0;
		//bios_hle_func_8C0000BC_misc_gdrom_fn_original = bios_hle_hooks_register_function(0x8C001000, bios_hle_func_8C0000BC_misc_gdrom_fn_2);
		// re execute the opcode now that it is unpatched
		sh4rcb.cntx.pc = sh4rcb.cntx.pr;

	//	verify(rehook_expected_pc == pc);
	}
	else {
		 printf("reios: dispatch %08X -> %08X\n", pc, mapd);
		 printf("REGS_LLE: BEFORE CALL!");
		 for (int i = 0; i < 16; ++i)
			 printf("r[%d]:%d ", i, sh4rcb.cntx.r[i]);
		 printf("\n");
		bool do_ret = hooks[mapd]();

		if ((sh4rcb.cntx.pc - 2) != pc) {
		//	printf("PC patched to 0x%08x from 0x%08x\n", sh4rcb.cntx.pc - 2, pc);
			//return;
		}

		

		//if (do_ret) {

		//sh4rcb.cntx.pc = pc;
	//	}
	}
}

bool bios_hle_hooks_init(Sh4RCB* reg_context) {
	bios_hle_hooks_shutdown();
	p_reg_ctx = reg_context;
	return true;
}

void bios_hle_hooks_shutdown() {
	hooks.clear();
	hooks_rev.clear();
	p_reg_ctx = nullptr;
}
