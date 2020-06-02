#include "license/bsd"
#include "reios_gdrom_q.h"

gdrom_entry_node_t::gdrom_entry_node_t() : allocated(false), stat(k_gd_cmd_st_none), cmd(-1U), id(k_gdrom_hle_invalid_index), res{ 0,0,0,0 }, toc{ 0,0 }, toc2{ 0,0 }, rd{ 0,0,0,0 }, play{ 0,0,0 } {
}

gdrom_entry_node_t::~gdrom_entry_node_t() {}

inline void gdrom_entry_node_t::operator=(const gdrom_entry_node_t& other) {
	this->stat = other.stat;
	this->cmd = other.cmd;
	this->id = other.id;
	this->allocated = other.allocated;

	//pod
	memcpy(static_cast<void*>(this->res), static_cast<const void*>(other.res), sizeof(this->res));

	//pod non-packed 
	memcpy(static_cast<void*>(&this->toc), static_cast<const void*>(&other.toc), sizeof(this->toc));
	memcpy(static_cast<void*>(&this->toc2), static_cast<const void*>(&other.toc2), sizeof(this->toc2));
	memcpy(static_cast<void*>(&this->rd), static_cast<const void*>(&other.rd), sizeof(this->rd));
	memcpy(static_cast<void*>(&this->play), static_cast<const void*>(&other.play), sizeof(this->play));
}

//gdrom_hle_rm_cmd
void gdrom_queue_manager_c::rm_cmd(const gdrom_entry_node_t& node) {
	const size_t id = node.id;
	const size_t sz = g_pending_ops.size();

	printf("rm %lu\n", id);

	if (id == k_gdrom_hle_invalid_index)
		return;
	else if (id >= sz)
		return;

	g_pending_ops[id].allocated = false;
	g_pending_ops[id].stat = k_gd_cmd_st_done;

	return;

	//Don't delete
	if (sz == 1) {
		g_pending_ops.clear();
		return;
	}

	const size_t last = sz - 1;

	if (last == id) {
		g_pending_ops.pop_back();
		return;
	}

	const gdrom_entry_node_t& tmp = g_pending_ops[last];
	g_pending_ops[id] = tmp;

	g_pending_ops.pop_back();
}

gdrom_entry_node_t* gdrom_queue_manager_c::add_cmd(const uint32_t cmd, const uint32_t data, const uint32_t status) {
	size_t index = k_gdrom_hle_invalid_index;

	if (g_pending_ops.size() < k_max_kos_qlen) {
		index = g_pending_ops.size();
		g_pending_ops.push_back(gdrom_entry_node_t());
	}
	else {
		for (size_t i = 0, j = g_pending_ops.size(); i < j; ++i) {
			if (g_pending_ops[i].stat != k_gd_cmd_st_active) {
				index = i;
				break;
			}
		}
	}

	printf("gdrom_hle_add_cmd %lu cmd %u (tot %u)\n", index, cmd, g_pending_ops.size());

	//valid ?
	if (index == k_gdrom_hle_invalid_index)
		return nullptr;

	//Do ops
	gdrom_entry_node_t& me = g_pending_ops[index];
	me.stat = status;
	me.cmd = cmd;
	me.id = index;
	me.allocated = true;
	me.data = data;

	return &g_pending_ops[index];
}

void gdrom_queue_manager_c::reset() {
	g_pending_ops.clear();
}