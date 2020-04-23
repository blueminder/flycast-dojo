#include "bios_hle_gdrom_queue.hpp"

struct DECL_ALIGN(32) cmd_queue_node_t {
	static const size_t k_gdrom_q_result_size = 4U;

	uint32_t op;
	int32_t stat;
	uint32_t res[k_gdrom_q_result_size];

	cmd_queue_node_t(const uint32_t _op, const int32_t _stat) : op(_op), stat(_stat) {
		//memset(res, 0, sizeof(res));
	}

	cmd_queue_node_t() : op(-1U), stat(-1) {
		//memset(res, 0, sizeof(res));
	}

	~cmd_queue_node_t() {

	}

	inline void operator=(const cmd_queue_node_t& other) {
		this->op = other.op;
		this->stat = other.stat;
		memcpy(this->res, other.res, sizeof(this->res));
	}

	inline void operator=(const cmd_queue_node_t* other) {
		this->op = other->op;
		this->stat = other->stat;
		memcpy(this->res, other->res, sizeof(this->res));
	}
};

static const size_t k_gdrom_q_max_size = 16U;
static size_t u_gdrom_q_ptr = 0U;
static cmd_queue_node_t DECL_ALIGN(32) cmd_q_buffer[k_gdrom_q_max_size];
 
bool bios_hle_gdrom_queue_init() {
	u_gdrom_q_ptr = 0U;
	return true;
}

void bios_hle_gdrom_queue_shutdown() {
 
}
