/*
	Basic gdrom syscall emulation
	Adapted from some (very) old pre-nulldc hle code
*/

#include "license/bsd"
#include <stdio.h>
#include "types.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "gdrom_hle.h"
#include "reios.h"
#include "hw/gdrom/gdromv3.h"

#define r Sh4cntx.r
#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))
#define debugf printf

extern unique_ptr<GDRomDisc> g_GDRDisc;

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

	debugf("GDROM READ TOC : %X %X \n\n", s, b);

	g_GDRDisc->GetToc(pDst, s);

	//The syscall swaps to LE it seems
	for (size_t i = 0; i < 102; i++) {
		pDst[i] = SWAP32(pDst[i]);
	}
}

void read_sectors_to(u32 addr, u32 sector, u32 count) {
	u8 * pDst = GetMemPtr(addr, 0);

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

void GD_HLE_Command(u32 cc, u32 prm)
{
	printf("GD_HLE_Command %u %u\n", cc, prm);
	switch(cc)
	{
	case GDCC_GETTOC:
		printf("GDROM:\t*FIXME* CMD GETTOC CC:%X PRM:%X\n",cc,prm);
		break;

	case GDCC_GETTOC2:
		debugf("GDROM:\GDCC_GETTOC2 CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadTOC(r[5]);
		break;

	case GDCC_GETSES:
		debugf("GDROM:\tGETSES CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadSES(r[5]);
		break;

	case GDCC_INIT:
		printf("GDROM:\tCMD INIT CC:%X PRM:%X\n",cc,prm);
		break;

	case GDCC_PIOREAD:
		printf("GDROM:\tCMD RDPIO CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadPIO(r[5]);
		break;

	case GDCC_DMAREAD:
		debugf("GDROM:\tCMD DMAREAD CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadDMA(r[5]);
		break;

	case GDCC_PLAY_SECTOR:
		printf("GDROM:\tCMD PLAYSEC? CC:%X PRM:%X\n",cc,prm);
		break;

	case GDCC_RELEASE:
		printf("GDROM:\tCMD RELEASE? CC:%X PRM:%X\n",cc,prm);
		break;

	case GDCC_STOP:	printf("GDROM:\tCMD STOP CC:%X PRM:%X\n",cc,prm);	break;
	case GDCC_SEEK:	printf("GDROM:\tCMD SEEK CC:%X PRM:%X\n",cc,prm);	break;
	case GDCC_PLAY:	printf("GDROM:\tCMD PLAY CC:%X PRM:%X\n",cc,prm);	break;
	case GDCC_PAUSE:printf("GDROM:\tCMD PAUSE CC:%X PRM:%X\n",cc,prm);	break;

	case GDCC_READ:
		printf("GDROM:\tCMD READ CC:%X PRM:%X\n",cc,prm);
		break;

	case GDCC_GETSCD:
		debugf("GDROM:\tGETSCD CC:%X PRM:%X\n",cc,prm);
		GDCC_HLE_GETSCD(r[5]);
		break;

	default: printf("GDROM:\tUnknown GDROM CC:%X PRM:%X\n",cc,prm); break;
	}
}

#include <thread>
#include <mutex>
 #include <chrono>
static std::thread m_main_thread;
static std::recursive_mutex m_main_mtx;

static void process_cmds() {
	
	m_main_mtx.lock();
	size_t x = 0;

	for (const auto& i : g_reios_ctx.gd_q.get_pending_ops()) {
		if(i.stat != k_gd_cmd_st_active)//if (!i.allocated)
			continue;
		GD_HLE_Command(i.cmd, i.data);

		g_reios_ctx.gd_q.rm_cmd(i);
		++x;
	}
	m_main_mtx.unlock();
	if (0 != x)
		printf("process_cmds: handled %lu\n", x);
}

static void thd() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		process_cmds();
		//printf("Call\n");
	}
}

void gdrom_hle_op()
{
	static bool init_thd = false;
	if (!init_thd) {
		m_main_thread = std::thread(&thd);
		init_thd = true;
	}
	//static u32 last_cmd = 0xFFFFFFFF;	// only works for last cmd, might help somewhere
	//static u32 dwReqID=0xF0FFFFFF;		// ReqID, starting w/ high val

	static size_t prev_cmd = -1U;

	m_main_mtx.lock();
	if( SYSCALL_GDROM == r[6] )		// GDROM SYSCALL
	{
		switch(r[7])				// COMMAND CODE
		{
			// *FIXME* NEED RET
		case GDROM_SEND_COMMAND: {// SEND GDROM COMMAND RET: - if failed + req id
			debugf("\nGDROM:\tHLE SEND COMMAND CC:%X  param ptr: %X\n", r[4], r[5]);
			//GD_HLE_Command(r[4],r[5]);
			//last_cmd = r[0] = --dwReqID;		// RET Request ID
			auto p = g_reios_ctx.gd_q.add_cmd(r[4], r[5], k_gd_cmd_st_active);
			r[0] = p ? p->id : -1;
			prev_cmd = r[0];
			///process_cmds();
		}
		break;

		case GDROM_CHECK_COMMAND: {	// 
		//	printf("%u r4 %u r5\n", r[4], r[5]); die("");
			auto p = g_reios_ctx.gd_q.grab_cmd(r[4]);

			if (!p) {
				r[0] = 0;
				prev_cmd = -1U;
			}
			else {
				if (p->id == prev_cmd)
					r[0] = 0;
				else
					r[0] = p->stat;

				prev_cmd = p->id;
			} 
			if (r[5] != 0) {
				//No error
				WriteMem32(r[5] + 0, 0); // libCore_gdrom_get_status());	// STANDBY
				WriteMem32(r[5] + 4, g_GDRDisc->GetDiscType());// g_GDRDisc->GetDiscType());	// CDROM | 0x80 for GDROM
				WriteMem32(r[5] + 8, 0);
				WriteMem32(r[5] + 12, 0);
			}

			
			//r[0] = last_cmd == r[4] ? 2 : 0; // RET Finished : Invalid
			debugf("\nGDROM:\tHLE CHECK COMMAND REQID:%X  param ptr: %X -> %X\n", r[4], r[5], r[0]);
			//last_cmd = 0xFFFFFFFF;			// INVALIDATE CHECK CMD
		}
		break;

			// NO return, NO params
		case GDROM_MAIN: {
			debugf("\nGDROM:\tHLE GDROM_MAIN\n");
			process_cmds();
			
		}
			break;

		case GDROM_INIT:   printf("\nGDROM:\tHLE GDROM_INIT\n");	break;
		case GDROM_RESET:g_GDRDisc->Reset(true);  g_reios_ctx.gd_q.reset();	printf("\nGDROM:\tHLE GDROM_RESET\n");	break;

		case GDROM_CHECK_DRIVE: {	// 
			debugf("\nGDROM:\tHLE GDROM_CHECK_DRIVE r4:%X r5:%X (STAT = %u /TYPE = %u /q=%u)\n", r[4], r[5], libCore_gdrom_get_status(), g_GDRDisc->GetDiscType(), g_reios_ctx.gd_q.get_pending_ops().size());
			WriteMem32(r[4] + 0, 2);	// STANDBY
			WriteMem32(r[4] + 4, g_GDRDisc->GetDiscType());// g_GDRDisc->GetDiscType());	// CDROM | 0x80 for GDROM
			r[0] = 0;					// RET SUCCESS
		}
		break;

		case GDROM_ABORT_COMMAND:	// 
			printf("\nGDROM:\tHLE GDROM_ABORT_COMMAND r4:%X R5:%X\n",r[4],r[5]);
			r[0]=-1;				// RET FAILURE
			//die("");
		break;


		case GDROM_SECTOR_MODE:		// 
			printf("GDROM:\tHLE GDROM_SECTOR_MODE PTR_r4:%X\n",r[4]);
			for(int i=0; i<4; i++) {
				g_reios_ctx.SecMode[i] = ReadMem32(r[4]+(i<<2));
				printf("%08X%s", g_reios_ctx.SecMode[i],((3==i) ? "\n" : "\t"));
			}
			r[0]=0;					// RET SUCCESS
		break;

		default: r[0] = 0;  return;// printf("\nGDROM:\tUnknown SYSCALL: %X\n", r[7]); break;
		}
	}
	else							// MISC 
	{
		r[0] = 0;
		printf("SYSCALL:\tSYSCALL: %X\n",r[7]);
	}
	
	m_main_mtx.unlock();
}
