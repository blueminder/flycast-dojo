#pragma once
#include "license/bsd"

#include "types.h"
#include <vector>

static constexpr uint32_t k_gd_cmd_st_none = 0;
static constexpr uint32_t k_gd_cmd_st_active = 1;
static constexpr uint32_t k_gd_cmd_st_done = 2;
static constexpr uint32_t k_gd_cmd_st_abort = 3;
static constexpr uint32_t k_gd_cmd_st_err = 4;
constexpr size_t k_max_kos_qlen = 16;
constexpr size_t k_gdrom_hle_invalid_index = std::numeric_limits<size_t>::max();

 
struct gdrom_toc2_info {
	uint32_t session;
	uint32_t buffer;
};

struct gdrom_rd_info {
	uint32_t lba;
	uint32_t count;
	uint32_t buffer;
	uint32_t unknown;
};

struct gdrom_play_info {
	uint32_t start;
	uint32_t end;
	uint32_t repeat;
};
 

struct gdrom_entry_node_t {
	bool allocated;
	uint32_t stat;
	uint32_t cmd;
	uint32_t data;
	size_t id;
	uint32_t res[4];
	//merge these TODO
	gdrom_toc2_info toc, toc2;
	gdrom_rd_info rd;
	gdrom_play_info play;
	gdrom_entry_node_t();
	~gdrom_entry_node_t();

	inline void operator=(const gdrom_entry_node_t& other);
};

class gdrom_queue_manager_c {
private:
	std::vector< gdrom_entry_node_t > g_pending_ops;
public:

	gdrom_queue_manager_c() { reset(); }
	~gdrom_queue_manager_c() {}

	inline constexpr const std::vector< gdrom_entry_node_t >& get_pending_ops() const { return this->g_pending_ops; }

	inline gdrom_entry_node_t* grab_cmd(const uint32_t which) {
		return &g_pending_ops[which];
		//return ((which >= g_pending_ops.size()) || (g_pending_ops[which].stat == k_gd_cmd_st_active) || (g_pending_ops[which].allocated )) ? nullptr : &g_pending_ops[which];
	}

	inline void  set_stat(const uint32_t which, const uint32_t st) {
		g_pending_ops[which].stat = st;
	}

	inline void  set_res(const uint32_t which_id, const uint32_t which_res, const uint32_t val) {
		g_pending_ops[which_id].res[which_res] = val;
	}

	void reset();
	void rm_cmd(const gdrom_entry_node_t& node);
	gdrom_entry_node_t* add_cmd(const uint32_t cmd, const uint32_t data, const uint32_t status);
};
