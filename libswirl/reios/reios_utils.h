#pragma once
#include "license/bsd"
#include "types.h"
#include <unordered_map>


struct code_path_entry_t {
	uint32_t return_addr;
	uint32_t jmp_addr;
	Sh4Context reg_ctx;
	uint8_t* gd_rom_data;
	uint32_t gd_rom_sz;

	code_path_entry_t() : gd_rom_data(nullptr) {

	}

	~code_path_entry_t() {
		delete[] gd_rom_data;
	}

	inline void operator=(const code_path_entry_t& other) {
		this->return_addr = other.return_addr;
		this->jmp_addr = other.jmp_addr;
		memcpy(&this->reg_ctx, &other.reg_ctx, sizeof(this->reg_ctx));
	}
};

struct code_path_node_t {
	std::vector< code_path_entry_t > entries;
	code_path_entry_t final_res;

	code_path_node_t() {}
	~code_path_node_t() {}

	inline  const bool has_more() const { return !entries.empty(); }
	inline void push(code_path_entry_t&& entry) {
		entries.push_back(std::move(entry));
	}

};

class code_path_tracer_c {
private:
	std::unordered_map<uint32_t, code_path_node_t > m_code_paths;
public:

	code_path_tracer_c() {

	}

	~code_path_tracer_c() {

	}

};

//guest set
void guest_mem_set(const uint32_t dst_addr, const uint8_t val, const uint32_t sz);
//vmem 2 vmem addr cp
void guest_to_guest_memcpy(const uint32_t dst_addr, const uint32_t src_addr, const uint32_t sz);
//vmem to host addr cpy
void guest_to_host_memcpy(void* dst_addr, const uint32_t src_addr, const uint32_t sz);
//host mem to vmem addr cpy
void host_to_guest_memcpy(const uint32_t dst_addr, const void* src_addr, const uint32_t sz);

const uint32_t reios_locate_flash_cfg_addr();
