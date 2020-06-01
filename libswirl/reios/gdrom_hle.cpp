/*
	Basic gdrom syscall emulation
	Adapted from some (very) old pre-nulldc hle code
*/

#include <stdio.h>
#include "types.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "gdrom_hle.h"
#include "hw/gdrom/gdromv3.h"

#define SWAP32(a) ( (((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff) )
#define debugf printf

constexpr auto GD_CMD_PIOREAD = 16; 
constexpr auto GD_CMD_DMAREAD = 17;  
constexpr auto GD_CMD_GETTOC = 18;
constexpr auto GD_CMD_GETTOC2 = 19; 
constexpr auto GD_CMD_PLAY = 20;    
constexpr auto GD_CMD_PLAY2 = 21;   
constexpr auto GD_CMD_PAUSE = 22;   
constexpr auto GD_CMD_RELEASE = 23;  
constexpr auto GD_CMD_INIT = 24;;    
constexpr auto GD_CMD_SEEK = 27; 
constexpr auto GD_CMD_READ = 28; 
constexpr auto GD_CMD_STOP = 33;     
constexpr auto GD_CMD_GETSCD = 34;
constexpr auto GD_CMD_GETSES = 35;
constexpr auto GD_CMD_STATUS_NONE = 0;
constexpr auto GD_CMD_STATUS_ACTIVE = 1;
constexpr auto GD_CMD_STATUS_DONE = 2;
constexpr auto GD_CMD_STATUS_ABORT = 3;
constexpr auto GD_CMD_STATUS_ERROR = 4;
constexpr auto GD_ERROR_OK = 0;
constexpr auto GD_ERROR_NO_DISC = 2;
constexpr auto GD_ERROR_DISC_CHANGE = 6;
constexpr auto GD_ERROR_SYSTEM = 1;
constexpr size_t k_max_kos_qlen = 16;
constexpr size_t k_gdrom_hle_invalid_index = std::numeric_limits<size_t>::max();

#pragma pack(push , 1)
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
#pragma pop
 
struct gdrom_entry_node_t {
	uint32_t stat;
	uint32_t cmd;
	size_t id;
	uint32_t res[4];
	//merge these TODO
	gdrom_toc2_info toc, toc2;
	gdrom_rd_info rd;
	gdrom_play_info play;
	gdrom_entry_node_t() : stat(GD_CMD_STATUS_NONE), cmd(-1U), id(k_gdrom_hle_invalid_index), res{ 0,0,0,0 }, toc{ 0,0 }, toc2{ 0,0 }, rd{ 0,0,0,0 }, play{ 0,0,0} {
	}

	~gdrom_entry_node_t() {}

	inline void operator=(const gdrom_entry_node_t& other) {
		this->stat = other.stat;
		this->cmd = other.cmd;
		this->id = other.id;
		//pod
		memcpy(static_cast<void*>(this->res), static_cast<const void*>(other.res), sizeof(this->res));
		//pod non-packed 
		memcpy(static_cast<void*>(&this->toc), static_cast<const void*>(&other.toc), sizeof(this->toc));
		memcpy(static_cast<void*>(&this->toc2) , static_cast<const void*>(&other.toc2), sizeof(this->toc2));
		memcpy(static_cast<void*>(&this->rd), static_cast<const void*>(&other.rd), sizeof(this->rd));
		memcpy(static_cast<void*>(&this->play), static_cast<const void*>(&other.play), sizeof(this->play));
	}
};

extern unique_ptr<GDRomDisc> g_GDRDisc;
extern void guest_to_host_memcpy(void* dst_addr, const uint32_t src_addr, const uint32_t sz);
extern void host_to_guest_memcpy(const uint32_t dst_addr, const void* src_addr, const uint32_t sz);

class gdrom_queue_manager_c {
private:
	std::vector< gdrom_entry_node_t > g_pending_ops;
public:

	gdrom_queue_manager_c() {}
	~gdrom_queue_manager_c() {}

	inline void reset() {
		g_pending_ops.clear();
	}

	inline constexpr gdrom_entry_node_t* grab_cmd(const uint32_t which)  {
		return ((which >= g_pending_ops.size()) || (g_pending_ops[which].stat == GD_CMD_STATUS_NONE)) ? nullptr : &g_pending_ops[which];
	}

	inline void  set_stat(const uint32_t which,const uint32_t st) {
		g_pending_ops[which].stat = st;
	}

	inline void  set_res(const uint32_t which_id,const uint32_t which_res, const uint32_t val) {
		g_pending_ops[which_id].res[which_res] = val;
	}

	//gdrom_hle_rm_cmd
	void rm_cmd(const gdrom_entry_node_t& node) {
		const size_t id = node.id;
		const size_t sz = g_pending_ops.size();

		if (id == k_gdrom_hle_invalid_index)
			return;
		else if (id >= sz)
			return;

		g_pending_ops[id].stat = GD_CMD_STATUS_DONE;
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

	void exec_cmd(gdrom_entry_node_t& node) {
		if (node.id == k_gdrom_hle_invalid_index) {
			debugf("Invalid NODE ID!!\n");
			return;
		}

		switch (node.cmd) {

		default:
			debugf("Unknown cmd : %u\n", node.cmd);
			return;
		}

		uint32_t status = 0;// CDROM_ERROR_OK;
		void* ptr;
		switch (node.cmd) {
		case GD_CMD_INIT:
			node.stat = GD_CMD_STATUS_DONE;
			break;
		case GD_CMD_GETTOC2:
			//ptr = mem_get_region(cmd->params.toc2.buffer);
			//status = gdrom_read_toc(ptr);
			/*if (status == CDROM_ERROR_OK) {

				struct gdrom_toc* toc = (struct gdrom_toc*)ptr;
				for (unsigned i = 0; i < 99; i++) {
					toc->track[i] = ntohl(toc->track[i]);
				}
				toc->first = ntohl(toc->first);
				toc->last = ntohl(toc->last);
				toc->leadout = ntohl(toc->leadout);
			}*/
			break;
		case GD_CMD_PIOREAD:
		case GD_CMD_DMAREAD:
			//ptr = mem_get_region(cmd->params.readcd.buffer);
			//status = gdrom_read_cd(cmd->params.readcd.lba,
				//cmd->params.readcd.count, 0x28, ptr, NULL);
			break;
		default:

			node.stat = GD_CMD_STATUS_ERROR;
			node.res[0] = GD_ERROR_SYSTEM;
			return;
		}

		/*
		switch (status) {
		case CDROM_ERROR_OK:
			cmd->stat = GD_CMD_STATUS_DONE;
			cmd->res[0] = GD_ERROR_OK;
			break;
		case CDROM_ERROR_NODISC:
			cmd->stat = GD_CMD_STATUS_ERROR;
			cmd->res[0] = GD_ERROR_NO_DISC;
			break;
		default:
			cmd->stat = GD_CMD_STATUS_ERROR;
			cmd->res[0] = GD_ERROR_SYSTEM;
		}*/
	}

	void run_all_cmds() {
		printf("gdrom_hle_run_all_cmds\n");
		for (size_t i = 0, j = g_pending_ops.size(); i < j; ++i) {
			gdrom_entry_node_t& me = g_pending_ops[i];

			if (GD_CMD_STATUS_DONE == me.stat)
				continue;

			if (GD_CMD_STATUS_ACTIVE != me.stat) {
				this->rm_cmd(me);
				//i = 0;
				//j = g_pending_ops.size();
				continue;
			}

			printf("Executing CMD : %lu/%lu\n", me.id, j);
			GD_HLE_Command(me.cmd, me.id, i);// gdrom_hle_rm_cmd(me);
			me.stat = GD_CMD_STATUS_DONE;;
		}
	}

	size_t add_cmd(const uint32_t cmd, const uint32_t data) {
		size_t index = k_gdrom_hle_invalid_index;

		if (g_pending_ops.size() < k_max_kos_qlen) {
			index = g_pending_ops.size();
			g_pending_ops.push_back(gdrom_entry_node_t());
		}
		else {
			for (size_t i = 0, j = g_pending_ops.size(); i < j; ++i) {
				if (g_pending_ops[i].stat != GD_CMD_STATUS_ACTIVE) {
					index = i;
					break;
				}
			}
		}

		printf("gdrom_hle_add_cmd %lu\n", index);
		//valid ?
		if (index == k_gdrom_hle_invalid_index)
			return k_gdrom_hle_invalid_index;

		//Do ops
		gdrom_entry_node_t& me = g_pending_ops[index];
		me.stat = GD_CMD_STATUS_ACTIVE;
		me.cmd = cmd;
		me.id = index;

		switch (cmd) {
		case GD_CMD_PIOREAD:
		case GD_CMD_DMAREAD:
			guest_to_host_memcpy(&me.rd, data, sizeof(gdrom_rd_info));
			return index;
		case GD_CMD_GETTOC:
			guest_to_host_memcpy(&me.toc, data, sizeof(gdrom_toc2_info));
			return index;
		case GD_CMD_GETTOC2:
			guest_to_host_memcpy(&me.toc2, data, sizeof(gdrom_toc2_info));
			return index;
		case GD_CMD_PLAY:
		case GD_CMD_PLAY2:
			guest_to_host_memcpy(&me.play, data, sizeof(gdrom_play_info));
			return index;
		}
		return index;
	}

};

static gdrom_queue_manager_c g_q_mgr;
static inline constexpr const uint32_t ptr_to_u32_le(const void* p) {
	const uint8_t* a = static_cast<const uint8_t*>(p);
	return ((uint32_t)a[3] << 24) | ((uint32_t)a[2] << 16) | ((uint32_t)a[1] << 8) | ((uint32_t)a[0]);
}


void GDROM_HLE_ReadSES(u32 addr)
{
	u32 s = ReadMem32(addr + 0);
	u32 b = ReadMem32(addr + 4);
	u32 ba = ReadMem32(addr + 8);
	u32 bb = ReadMem32(addr + 12);

	printf("GDROM_HLE_ReadSES: doing nothing w/ %d, %d, %d, %d\n", s, b, ba, bb);
}

void GDROM_HLE_ReadTOC(u32 Addr)
{
	u32 s = ReadMem32(Addr + 0);
	u32 b = ReadMem32(Addr + 4);

	u32* pDst = (u32*)GetMemPtr(b, 0);

	//
	debugf("GDROM READ TOC : %X %X \n\n", s, b);

	g_GDRDisc->GetToc(pDst, s);

	//The syscall swaps to LE it seems
	for (int i = 0; i < 102; i++) {
		pDst[i] = SWAP32(pDst[i]);
	}
}

void read_sectors_to(u32 addr, u32 sector, u32 count) {
	u8* pDst = GetMemPtr(addr, 0);

	if (pDst) {
		g_GDRDisc->ReadSector(pDst, sector, count, 2048);
	}
	else {
		u8 temp[2048];

		while (count > 0) {
			g_GDRDisc->ReadSector(temp, sector, 1, 2048);

			for (int i = 0; i < 2048 / 4; i += 4) {
				WriteMem32(addr, temp[i]);
				addr += 4;
			}

			sector++;
			count--;
		}
	}

}

void GDROM_HLE_ReadDMA(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	debugf("GDROM:\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);
	read_sectors_to(b, s, n);
}

void GDROM_HLE_ReadPIO(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	debugf("GDROM:\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);

	read_sectors_to(b, s, n);
}

void GDCC_HLE_GETSCD(u32 addr) {
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	printf("GDROM: Doing nothing for GETSCD [0]=%d, [1]=%d, [2]=0x%08X, [3]=0x%08X\n", s, n, b, u);
}

#define r Sh4cntx.r

u32 SecMode[4];

void GD_HLE_Command(u32 cc, u32 prm,const int32_t m)
{
	switch (cc)
	{
	case GDCC_GETTOC:
		printf("GDROM:\t*FIXME* CMD GETTOC CC:%X PRM:%X\n", cc, prm);
		break;

	case GDCC_GETTOC2:
		GDROM_HLE_ReadTOC(r[5]);
		break;

	case GDCC_GETSES:
		debugf("GDROM:\tGETSES CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadSES(r[5]);
		break;

	case GDCC_INIT:
		printf("GDROM:\tCMD INIT CC:%X PRM:%X\n", cc, prm);
		if (m != -1)
			g_q_mgr.set_stat(m , GD_CMD_STATUS_DONE);
		break;

	case GDCC_PIOREAD:
		GDROM_HLE_ReadPIO(r[5]);
		break;

	case GDCC_DMAREAD:
		debugf("GDROM:\tCMD DMAREAD CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadDMA(r[5]);
	break;

	case GDCC_PLAY_SECTOR:
		printf("GDROM:\tCMD PLAYSEC? CC:%X PRM:%X\n", cc, prm);
	break;

	case GDCC_RELEASE:
		printf("GDROM:\tCMD RELEASE? CC:%X PRM:%X\n", cc, prm);
	break;

	case GDCC_STOP:	printf("GDROM:\tCMD STOP CC:%X PRM:%X\n", cc, prm);	 break;
	case GDCC_SEEK:	printf("GDROM:\tCMD SEEK CC:%X PRM:%X\n", cc, prm);	 break;
	case GDCC_PLAY:	printf("GDROM:\tCMD PLAY CC:%X PRM:%X\n", cc, prm);	 break;
	case GDCC_PAUSE:printf("GDROM:\tCMD PAUSE CC:%X PRM:%X\n", cc, prm); break;

	case GDCC_READ:
		printf("GDROM:\tCMD READ CC:%X PRM:%X\n", cc, prm);
		break;

	case GDCC_GETSCD:
		debugf("GDROM:\tGETSCD CC:%X PRM:%X\n", cc, prm);
		GDCC_HLE_GETSCD(r[5]);
		break;

	default: 
		printf("GDROM:\tUnknown GDROM CC:%X PRM:%X\n", cc, prm); 
		if (m != -1) {
			 
			g_q_mgr.set_stat(m, GD_CMD_STATUS_ERROR);
			g_q_mgr.set_res(m, 0, GD_ERROR_SYSTEM);
		}
		return;
	}

	if (m == -1)
		return;

	g_q_mgr.set_stat(m, GD_CMD_STATUS_DONE);
	g_q_mgr.set_res(m, 0, GD_ERROR_OK);
	 
}



void gdrom_hle_op()
{
	//static u32 last_cmd = 0xFFFFFFFF;	// only works for last cmd, might help somewhere
	//static u32 dwReqID = 0xF0FFFFFF;		// ReqID, starting w/ high val

	if (SYSCALL_GDROM == r[6])		// GDROM SYSCALL
	{
		switch (r[7])				// COMMAND CODE
		{
			// *FIXME* NEED RET
		case GDROM_SEND_COMMAND: {// SEND GDROM COMMAND RET: - if failed + req id
			debugf("\nGDROM:\tHLE SEND COMMAND CC:%X  param ptr: %X\n", r[4], r[5]);
			die("");
			//GD_HLE_Command(r[4], r[5]);
			//last_cmd = r[0] = --dwReqID;		// RET Request ID
			size_t res = g_q_mgr.add_cmd(r[4], r[5]);
			if (res == k_gdrom_hle_invalid_index)
				r[0] = -1;
			else
				r[0] = (uint32_t)res;
		}
			break;

		case GDROM_CHECK_COMMAND:	// 
			//r[0] = last_cmd == r[4] ? 2 : 0; // RET Finished : Invalid
			
			//last_cmd = 0xFFFFFFFF;			// INVALIDATE CHECK CMD
			{
				debugf("\nGDROM:\tHLE CHECK COMMAND REQID:%X  param ptr: %X -> %X\n", r[4], r[5], r[0]);
				gdrom_entry_node_t* cmd = g_q_mgr.grab_cmd(r[4]);
				if (cmd == nullptr) {
					printf("(not found!)\n");
					r[0] = GD_CMD_STATUS_NONE;
				}
				else {
					printf("(found! : %d)\n",cmd->stat);
					r[0] = cmd->stat;
					if (cmd->stat == GD_CMD_STATUS_ERROR &&
						r[5] != 0) {
						host_to_guest_memcpy(r[5], &cmd->res, sizeof(cmd->res));
					}
				}
			}
			break;

			// NO return, NO params
		case GDROM_MAIN:
			debugf("\nGDROM:\tHLE GDROM_MAIN\n");
			g_q_mgr.run_all_cmds();
			break;

		case GDROM_INIT:	printf("\nGDROM:\tHLE GDROM_INIT\n");	break;
		case GDROM_RESET:	printf("\nGDROM:\tHLE GDROM_RESET\n");	break;

		case GDROM_CHECK_DRIVE : {	// 
			debugf("\nGDROM:\tHLE GDROM_CHECK_DRIVE r4:%X \n", r[4], r[5]);
			if (r[4] != 0) {
				extern GD_SecNumbT SecNumber; //Nasty hack but interface is a MESS
				if (g_GDRDisc->GetDiscType() == Open) {
					WriteMem32(r[4] + 0, 6);
					WriteMem32(r[4] + 4, 0);
				} else if (g_GDRDisc->GetDiscType() == NoDisk) {
					WriteMem32(r[4] + 0, 7);
					WriteMem32(r[4] + 4, 0);
				} else {
					WriteMem32(r[4] + 0, SecNumber.Status);
					WriteMem32(r[4] + 4, g_GDRDisc->GetDiscType());
				}
				debugf("(stat = %d , disc type = %d)\n\n", SecNumber.Status, g_GDRDisc->GetDiscType());
				r[0] = 0;					// RET SUCCESS
			}
			else r[0] = -1;
		}
		break;

		case GDROM_ABORT_COMMAND: {	// 
			printf("\nGDROM:\tHLE GDROM_ABORT_COMMAND r4:%X\n", r[4], r[5]);
			r[0] = -1;				// RET FAILURE

			gdrom_entry_node_t* cmd = g_q_mgr.grab_cmd(r[4]);
			if (cmd == nullptr || cmd->stat != GD_CMD_STATUS_ACTIVE) {
				r[0] = -1;
			}
			else {
				cmd->stat = GD_CMD_STATUS_ABORT;
				r[0] = 0;
			}
		}

			break;


		case GDROM_SECTOR_MODE:		// 
			printf("GDROM:\tHLE GDROM_SECTOR_MODE PTR_r4:%X\n", r[4]);
			for (int i = 0; i < 4; i++) {
				SecMode[i] = ReadMem32(r[4] + (i << 2));
				printf("%08X%s", SecMode[i], ((3 == i) ? "\n" : "\t"));
			}
			r[0] = 0;					// RET SUCCESS
			break;

		default: printf("\nGDROM:\tUnknown SYSCALL: %X\n", r[7]); break;
		}
	}
	else							// MISC 
	{
		printf("SYSCALL:\tSYSCALL: %X\n", r[7]);
	}
}