#pragma once
#include "license/bsd"
#include "types.h"
#include <stack>
#include "hw/sh4/sh4_mem.h"
typedef bool hle_hook_stub_t();
 
struct DECL_ALIGN(32) hooked_function_rw_restore_ctx_t {
    uint32_t count;
    uint32_t read_addr;
    uint32_t write_addr;
    std::vector<uint8_t> write_buf, read_buf;
};

struct hooked_function_extended_call_data_t {
    Sh4Context in_ctx, ret_ctx;
    hle_hook_stub_t* dest;
    std::vector< hooked_function_rw_restore_ctx_t > dynamic_patches;
    std::string name;
};
/*

        Call order :

        Top level : LLE CALL of BIOS
        Next : my hooked function


        On trap :

        while (!hooked.reached_top_level()) {
                hooked.save_state
                hooked.copy_mem_base  // ALL Regions affected are stored
                jump(hooked.top().address)

                hooked.restore_state // SH4 Regs etc
                hooked.restore_mem_base()

                jump(sh4.regs.pr)
        }
*/

struct hooked_function_t {
    bool hybrid;
    std::vector<uint8_t> common_buf;
    std::stack< hooked_function_extended_call_data_t > call_order; // top_level => original hook

    hooked_function_t(hle_hook_stub_t* in,bool _hybrid) : hybrid(_hybrid) {
        call_order.push(hooked_function_extended_call_data_t());
        call_order.top().dest = in;
    }

    inline void save_state() {
        if (call_order.size() < 2)
            return;

        memcpy(&call_order.top().in_ctx, &sh4rcb.cntx, sizeof(Sh4Context));

        std::vector< hooked_function_rw_restore_ctx_t >& patches = call_order.top().dynamic_patches;

        for (size_t i = 0, j = patches.size(); i < j; ++i) {
            std::vector<uint8_t>& rb = patches[i].read_buf;

            rb.clear();

            for (size_t k = patches[i].read_addr, l = k +  patches[i].count; k < l; ++k) {
                rb.push_back(ReadMem8(k));
            }
        }
    }

    inline hle_hook_stub_t* grab() {
        return call_order.top().dest;
    }

    inline void restore_state() {
        if (call_order.size() < 2)
            return;

        memcpy(  &sh4rcb.cntx, &call_order.top().in_ctx , sizeof(Sh4Context));

       //todo

        call_order.pop();
    }
};

u16 bios_hle_hooks_register_function(uint32_t func_entry_point, hle_hook_stub_t* fn);
void bios_hle_hooks_register_set_syscall(uint32_t syscall_vector, uint32_t function_address);
 uint32_t bios_hle_hooks_reverse(hle_hook_stub_t* fn);
bool bios_hle_hooks_init(Sh4RCB* reg_context);
void bios_hle_hooks_shutdown();

void DYNACALL bios_hle_hooks_dyna_trap(uint32_t op);