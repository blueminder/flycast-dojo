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
#include "reios_dbg_module_stubs.h"
#include "../debugger_cpp/shared_contexts.h"

#define r Sh4cntx.r
#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))
#define debugf printf
#define NULLDBG_ENABLED

extern unique_ptr<GDRomDisc> g_GDRDisc;

void GDROM_HLE_ReadSES(u32 addr)
{
	uint32_t block[] = {
		ReadMem32(addr + 0), //s
		ReadMem32(addr + 4),//b
		ReadMem32(addr + 8),//ba
		ReadMem32(addr + 12),//bb
		addr
	};

#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.sess[0] = block[0];
	ctx.sess[1] = block[1];
	ctx.sess[2] = block[2];
	ctx.sess[3] = block[3];
	ctx.r_addr = addr;
	ctx.w_addr = -1U;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_rdses",(void*)&ctx,sizeof(ctx));
#endif
}

void GDROM_HLE_ReadTOC(u32 addr)
{
	u32 s = ReadMem32(addr + 0);
	u32 b = ReadMem32(addr + 4);
	u32* pDst = (u32*)GetMemPtr(b, 102 * sizeof(u32));


	g_GDRDisc->GetToc(pDst, s);
	printf("GDROM READ TOC : %X %X  (%c %c %c) \n\n", s, b,pDst[0],pDst[1],pDst[2]);
	//verify(0);

	//The syscall swaps to LE it seems
	for (size_t i = 0; i < 102; i++) {
		pDst[i] = SWAP32(pDst[i]);
	}

#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.sess[0] = s;
	ctx.sess[1] = b;
	ctx.sess[2] = -1U;
	ctx.sess[3] = -1U;
	ctx.r_addr = addr;
	ctx.w_addr = -1U;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_rdtoc",(void*)&ctx,sizeof(ctx));
#endif
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

#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.sess[0] = sector;
	ctx.sess[1] = count;
	ctx.sess[2] = -1U;
	ctx.sess[3] = -1U;
	ctx.r_addr = addr;
	ctx.w_addr = -1U;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_rdsectors_to",(void*)&ctx,sizeof(ctx));
#endif
}

void GDROM_HLE_ReadDMA(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	//debugf("GDROM:\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);
#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.sess[0] = s;
	ctx.sess[1] = n;
	ctx.sess[2] = b;
	ctx.sess[3] = u;
	ctx.r_addr = addr;
	ctx.w_addr = -1U;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_rddma",(void*)&ctx,sizeof(ctx));
#endif

	read_sectors_to(b, s, n);
}

void GDROM_HLE_ReadPIO(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	//debugf("GDROM:\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);
#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.sess[0] = s;
	ctx.sess[1] = n;
	ctx.sess[2] = b;
	ctx.sess[3] = u;
	ctx.r_addr = addr;
	ctx.w_addr = -1U;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_rdpio",(void*)&ctx,sizeof(ctx));
#endif

	read_sectors_to(b, s, n);
}

void GDCC_HLE_GETSCD(u32 addr) {
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	//printf("GDROM: Doing nothing for GETSCD [0]=%d, [1]=%d, [2]=0x%08X, [3]=0x%08X\n", s, n, b, u);
#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.sess[0] = s;
	ctx.sess[1] = n;
	ctx.sess[2] = b;
	ctx.sess[3] = u;
	ctx.r_addr = addr;
	ctx.w_addr = -1U;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_getscd",(void*)&ctx,sizeof(ctx));
#endif
}


void GD_HLE_Command(u32 cc, u32 prm)
{
	{
	#ifdef NULLDBG_ENABLED
		gdrom_ctx_t ctx;
		ctx.sess[0] = cc;
		ctx.sess[1] = prm;
		ctx.sess[2] = -1U;
		ctx.sess[3] = -1U;
		ctx.r_addr = -1U;
		ctx.w_addr = -1U;
		ctx.pc = Sh4cntx.pc;
		ctx.pr = Sh4cntx.pr;
		memcpy(ctx.regs,r,sizeof(r));

		debugger_pass_data("gdrom_hle_cmd",(void*)&ctx,sizeof(ctx));
	#endif
	}
	printf("GD_HLE_Command %u %u\n", cc, prm);
	//assert(0);
	switch(cc) {
		case GDCC_UNK_0x19: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x19 CC:%X PRM:%X\n",cc,prm);break;
		case GDCC_UNK_0x1a: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x1a CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x1d: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x1d CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x1e: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x1e CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x1f: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x1f CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x20: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x20 CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x24: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x24 CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x25: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x25 CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x26: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x26 CC:%X PRM:%X\n",cc,prm); break;

	case GDCC_GETTOC:
		printf("GDROM:\t*FIXME* CMD GETTOC CC:%X PRM:%X\n",cc,prm);
		break;

	case GDCC_GETTOC2:
		debugf("GDROM:\tGDCC_GETTOC2 CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadTOC(r[5]);
		//assert(0);
		break;

	case GDCC_GETSES:
		debugf("GDROM:\tGETSES CC:%X PRM:%X\n", cc, prm);
		GDROM_HLE_ReadSES(r[5]);
		break;

	case GDCC_INIT: {
		

		printf("GDROM:\tCMD INIT CC:%X PRM:%X  (r0 = 0x%x)\n",cc,prm,r[0]);
		break;
	}

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

/*
#include <thread>
#include <mutex>
 #include <chrono>
static std::thread m_main_thread;
static std::recursive_mutex m_main_mtx;
*/
static void process_cmds() {
	
	//m_main_mtx.lock();
	//size_t x = 0;

	for (const auto& i : g_reios_ctx.gd_q.get_pending_ops()) {
		if(i.stat != k_gd_cmd_st_active)//if (!i.allocated)
			continue;
		GD_HLE_Command(i.cmd, i.data);

		g_reios_ctx.gd_q.rm_cmd(i);
		//++x;
	}
	//m_main_mtx.unlock();
	//if (0 != x)
	//	printf("process_cmds: handled %lu\n", x);
}
/*
static void thd() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		process_cmds();
		//printf("Call\n");
	}
}
*/
void gdrom_hle_op()
{
	if( SYSCALL_GDROM == r[6] )		// GDROM SYSCALL
	{
		switch(r[7])				// COMMAND CODE
		{
			// *FIXME* NEED RET
		case GDROM_SEND_COMMAND: {// SEND GDROM COMMAND RET: - if failed + req id

#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_send_cmd",(void*)&ctx,sizeof(ctx));
#endif

			debugf("\nGDROM:\tHLE SEND COMMAND CC:%X  param ptr: %X\n", r[4], r[5]);

			GD_HLE_Command(r[4],r[5]);
			g_reios_ctx.last_cmd = r[0] = --g_reios_ctx.dwReqID;		// RET Request ID
		}
		break;

		case GDROM_CHECK_COMMAND: {	// 
		//	printf("%u r4 %u r5\n", r[4], r[5]); die("");
			//auto p = g_reios_ctx.gd_q.grab_cmd(r[4]);

#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_chk_cmd",(void*)&ctx,sizeof(ctx));
#endif

			r[0] = g_reios_ctx.last_cmd == r[4] ? 2 : 0; // RET Finished : Invalid
			debugf("\nGDROM:\tHLE CHECK COMMAND REQID:%X  param ptr: %X -> %X\n", r[4], r[5], r[0]);
			g_reios_ctx.last_cmd = 0xFFFFFFFF;		
			if (r[5] != 0) {
				//No error
				WriteMem32(r[5] + 0, libCore_gdrom_get_status());	// STANDBY
				WriteMem32(r[5] + 4, (g_reios_ctx.is_gdrom) ? 0x80 : g_GDRDisc->GetDiscType());// g_GDRDisc->GetDiscType());	// CDROM | 0x80 for GDROM
				WriteMem32(r[5] + 8, 0);
				WriteMem32(r[5] + 12, 0);

			}
		}
		break;

			// NO return, NO params
		case GDROM_MAIN: {
			
			debugger_pass_data("gdrom_main_cmd",nullptr,0);
			debugf("\nGDROM:\tHLE GDROM_MAIN\n");
			//process_cmds();
			
		}
			break;

		case GDROM_INIT:   {
			/*
			WriteMem32(0x8c001994,1);
			WriteMem32(0x8c00198c,0xffffffd8);
			WriteMem32(0x8c001990,0x8c001acc);
			r[0] = ReadMem32(0x8c001988);//   -0x73ffe744 ); */
		 
			printf("\nGDROM:\tHLE GDROM_INIT 0x%x\n",r[0]);	
			debugger_pass_data("gdrom_init_cmd",nullptr,0);
			g_reios_ctx.last_cmd = 0xFFFFFFFF;
			g_reios_ctx.dwReqID=0xF0FFFFFF;	
		}
		break;
		case GDROM_RESET:g_GDRDisc->Reset(true);  g_reios_ctx.gd_q.reset();	printf("\nGDROM:\tHLE GDROM_RESET\n");	break;

		case GDROM_CHECK_DRIVE: {	// 
			printf("\nGDROM:\tHLE GDROM_CHECK_DRIVE r4:%X r5:%X (STAT = %u /TYPE = %u /q=%u)\n", r[4], r[5], libCore_gdrom_get_status(), g_GDRDisc->GetDiscType(), g_reios_ctx.gd_q.get_pending_ops().size());
		 	WriteMem32(r[4] + 0, libCore_gdrom_get_status());	// STANDBY
			WriteMem32(r[4] + 4, (g_reios_ctx.is_gdrom) ? 0x80 : g_GDRDisc->GetDiscType());//g_GDRDisc->GetDiscType());// g_GDRDisc->GetDiscType());	// CDROM | 0x80 for GDROM
			r[0] = 0;					// RET SUCCESS

		#ifdef NULLDBG_ENABLED
			gdrom_ctx_t ctx;
			ctx.pc = Sh4cntx.pc;
			ctx.pr = Sh4cntx.pr;
			memcpy(ctx.regs,r,sizeof(r));

			debugger_pass_data("gdrom_chk_drive",(void*)&ctx,sizeof(ctx));
		#endif
		}
		break;

		case GDROM_ABORT_COMMAND:	// 
			printf("\nGDROM:\tHLE GDROM_ABORT_COMMAND r4:%X R5:%X\n",r[4],r[5]);
			r[0]=-1;				// RET FAILURE
			
			debugger_pass_data("gdrom_abort_cmd",nullptr,0);
			//die("");
		break;


		case GDROM_SECTOR_MODE:		//
		{
#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_sector_mode",(void*)&ctx,sizeof(ctx));
#endif
			printf("GDROM:\tHLE GDROM_SECTOR_MODE PTR_r4:%X\n",r[4]);
			for(int i=0; i<4; i++) {
				g_reios_ctx.SecMode[i] = ReadMem32(r[4]+(i<<2));
				printf("%08X%s", g_reios_ctx.SecMode[i],((3==i) ? "\n" : "\t"));
			}
			r[0]=0;					// RET SUCCESS
		}
		break;

		default: r[0] = 0;  return;// printf("\nGDROM:\tUnknown SYSCALL: %X\n", r[7]); break;
		}
	}
	else if (r[7] == 0)							// MISC 
	{
#ifdef NULLDBG_ENABLED
	gdrom_ctx_t ctx;
	ctx.pc = Sh4cntx.pc;
	ctx.pr = Sh4cntx.pr;
	memcpy(ctx.regs,r,sizeof(r));

	debugger_pass_data("gdrom_syscall",(void*)&ctx,sizeof(ctx));
#endif
		r[0] = 0;
		printf("SYSCALL:\tSYSCALL: %X\n",r[7]);
	}
	
//	m_main_mtx.unlock();
}
