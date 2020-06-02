#pragma once
#include "license/bsd"
#include "types.h"

//guest set
void guest_mem_set(const uint32_t dst_addr, const uint8_t val, const uint32_t sz);
//vmem 2 vmem addr cp
void guest_to_guest_memcpy(const uint32_t dst_addr, const uint32_t src_addr, const uint32_t sz);
//vmem to host addr cpy
void guest_to_host_memcpy(void* dst_addr, const uint32_t src_addr, const uint32_t sz);
//host mem to vmem addr cpy
void host_to_guest_memcpy(const uint32_t dst_addr, const void* src_addr, const uint32_t sz);

const uint32_t reios_locate_flash_cfg_addr();
