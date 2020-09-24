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
#include "reios_dbg.h"

#define r Sh4cntx.r
#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))
#define debugf printf
#define NULLDBG_ENABLED

extern unique_ptr<GDRomDisc> g_GDRDisc;

void _at_exit();

struct gd_sess_dmp_t {
	private:
	FILE* f;
	std::vector<uint8_t> pending;
	const size_t k_buf_sz = 64 * 1024;

	inline void flush(const bool force = false) {
		if ( (force) || (pending.size() >= k_buf_sz)) {
			if (!f) {
				printf("GD_Cmds.txt not open :(\n");
			} else {
				fwrite(&pending[0],1,pending.size(),f);
			}
			pending.clear();
		}
	}

	public:

	gd_sess_dmp_t() {
		f = fopen("gd_cmds.txt","wb");
		if (!f) {
			printf("Wtf %s\n could not be opened\n","gd_cmds.txt");
		} else {
			atexit(_at_exit);
		}
	}

	~gd_sess_dmp_t() {
		close();
	}

	inline void close() {
		if (!f) 
			return;
		flush(true);
		fclose(f);
		f = nullptr;
	}

	inline void write(const uint8_t* data,const size_t sz) {
		flush(false); //check +sz case -_-
		printf("Write %llu\n",sz);
		//optimize this -_-
		for (size_t i = 0;i < sz;++i)
			pending.push_back(data[i]);
	}

	inline void write(const std::string& s) {
		this->write((const uint8_t*)s.c_str(),s.length());
	}
};

static gd_sess_dmp_t gd_sess_dmp;

void _at_exit() {
	gd_sess_dmp.close();

}
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

void GDROM_HLE_ReadTOC(u32 addr)//addr = r[4] ... r[7]
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

inline void read_sectors_to(uint32_t addr,const uint32_t sector,const uint32_t count) {
	u8 * pDst = GetMemPtr(addr, count << 11U);

	if (pDst) {
		printf("Doing SINGLE MULTI read with : %u cnt @%u sec\n",count,sector);
		g_GDRDisc->ReadSector(pDst, sector, count, 2048);
		return;
	}

	{
		uint32_t temp[2048 / sizeof(uint32_t)]; //could support bulk but who cares :)

        for (uint32_t curr_sector=sector,dest_sector = curr_sector + count;curr_sector < dest_sector;++curr_sector) {
            g_GDRDisc->ReadSector((uint8_t*)temp, curr_sector, 1, sizeof(temp));
            for (uint32_t i = 0 , j = sizeof(temp) / sizeof(temp[0]); i < j;++i,addr += 4) {
                WriteMem32(addr, temp[i]);
        	}
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


	{
		static std::string s;
		s = "rd_sec:";
		s += "addr:" + std::to_string(addr) + "|";
		s += "sec:" + std::to_string(sector) + "|";
		s += "cnt:" + std::to_string(count);
		s+="\n";
		gd_sess_dmp.write(s);
	}
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
	g_reios_ctx.st[2].u_32 = n*2048;
	g_reios_ctx.st[3].u_32 = 0;
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
	g_reios_ctx.st[2].u_32 = 2048;
	g_reios_ctx.st[3].u_32 = 0;
}

void GDCC_HLE_GETSCD(u32 addr) {
	u32 s = ReadMem32(addr + 0x00); //format ??
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

	const auto gdst = g_GDRDisc->GetDiscType();
	g_reios_ctx.st[2].u_32 = n;

	switch (gdst) {
		case Open:
			memset(g_reios_ctx.st,0,sizeof(g_reios_ctx.st));
			g_reios_ctx.st[0].u_32 = GD_OPEN;
		break;
		case NoDisk:
			memset(g_reios_ctx.st,0,sizeof(g_reios_ctx.st));
			g_reios_ctx.st[0].u_32 = GD_NODISC;
		break;
		default : {
			g_reios_ctx.st[0].u_32 = libCore_gdrom_get_status();
			g_reios_ctx.st[1].u_32 = (g_reios_ctx.is_gdrom) ? 0x80 : g_GDRDisc->GetDiscType();
		}
		break;
	}

}



void dbg_diss(const uint32_t pc,const uint32_t epc,const bool exception = true) {
	std::vector<std::string> tmp;

	reios_dbg_diss_range(tmp,pc,epc);
	uint32_t pp = pc;

	for (const auto& p : tmp) {
		printf("0x%x : %s\n", pp , p.c_str());
		pp += 2;
	}

	if (!exception)
		return;
	verify(0);
}

void dbg_diss_bw(const uint32_t pc,const uint32_t epc,const bool exception = true) {
	std::vector<std::string> tmp;

	reios_dbg_diss_range_bw(tmp,pc,epc);
	uint32_t pp = pc;

	for (const auto& p : tmp) {
		printf("0x%x : %s\n", pp , p.c_str());
		pp += 2;
	}

	if (!exception)
		return;
	verify(0);
}

void dbg_diss_mixed(const uint32_t pc,const uint32_t range,const bool exception = true)  {
	printf("\n\nShowing DISS BACKWARDS : 0x%x ~ 0x%x\n\n",pc-range,pc);
	dbg_diss(pc - range,pc,false);
	printf("\n\nShowing DISS FORWARD : 0x%x ~ 0x%x\n\n",pc,pc+range);
	dbg_diss(pc,pc + range,false);

	if (!exception)
		return;

	verify(0);
}

void GD_HLE_Command(u32 cc, u32 prm)
{
	static reios_hle_status_t block[4] = {0x0 ,  0xe10, 0x0 ,0x0};
 


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
	//printf("GD_HLE_Command %u %u\n", cc, prm);
	//assert(0);

	{
		static std::string s;
		s = "cc:" + std::to_string(cc) + "|prm:" + std::to_string(prm);
		s += "|";
		for(size_t i = 4;i <= 7;++i)
			s += "r("+std::to_string(i)+"):)" + std::to_string(r[i]);
		s+="\n";
		gd_sess_dmp.write(s);
	}
	
	switch(cc) {
		case GDCC_UNK_0x19: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x19 CC:%X PRM:%X\n",cc,prm);break;
		case GDCC_UNK_0x1a: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x1a CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x1d: printf("GDROM:\t*FIXME* CMD GDCC_UNK_0x1d CC:%X PRM:%X\n",cc,prm); break;
		case GDCC_UNK_0x1e:  {

			//dbg_diss_mixed(Sh4cntx.pc,128);

			printf("GDROM:\t MMMM*FIXME* CMD GDCC_UNK_0x1e CC:%X PRM:%X (pc = 0x%X pr = 0x%X r5 = 0x%X)\n",cc,prm,Sh4cntx.pc,Sh4cntx.pr,r[5]); 
			printf("0x%x 0x%x 0x%x 0x%x\n",block[0],block[1],block[2],block[3]);
			const uint32_t dst = ReadMem32(r[5]);
			WriteMem32(dst + 0, block[0].u_32);
			WriteMem32(dst + 4,block[1].u_32 );
			WriteMem32(dst + 8, block[2].u_32);
			WriteMem32(dst + 12, block[3].u_32);
			g_reios_ctx.st[2].u_32 = 10;

			printf("0x%x 0x%x(~=  0x%02x 0x%02x 0x%02x 0x%02x) 0x%x 0x%x\n",block[0].u_32,block[1].u_32,block[1].u_8[0],block[1].u_8[1],
			block[1].u_8[2],block[1].u_8[3],block[2],block[3]);
			


			break;

		}

		case GDCC_UNK_0x1f: {
			printf("GDROM:\t*WWSFIXME* CMD GDCC_UNK_0x1f CC:%X PRM:%X\n",cc,prm); 
			//if (r[5] == 0)
			//	break;
			reios_hle_status_t a; a.u_32 = ReadMem32(r[5]+0);
			reios_hle_status_t b; b.u_32 = ReadMem32(r[5]+4);
			reios_hle_status_t c; c.u_32 = ReadMem32(r[5]+8);
			reios_hle_status_t d; d.u_32 = ReadMem32(r[5]+12);
 
			block[0].u_32 = a.u_32;
			block[1].u_32 = b.u_32;
			block[2].u_32 = c.u_32 ;
			block[3].u_32 = d.u_32;

			g_reios_ctx.st[2].u_32 = 10;
			
			printf("0x%x 0x%x(~=  0x%02x 0x%02x 0x%02x 0x%02x) 0x%x 0x%x\n",a,b,b.u_8[0],b.u_8[1],b.u_8[2],b.u_8[3],c,d);
 
			break;
		}
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

	case GDCC_PLAY_SECTOR: {
		printf("GDROM:\tCMD PLAYSEC? CC:%X PRM:%X\n",cc,prm);
#if 0
		uint32_t local_10 = ReadMem32(r[4] + 0);

		uint32_t puVar11 = ReadMem32(r[4] + 4);

		uint32_t DAT_8c001d6c = ReadMem32(0x8c001d6c);
		uint32_t DAT_8c001d70 = ReadMem32(0x8c001d70);
		uint32_t DAT_8c001d78 = ReadMem32(0x8c001d78);

		WriteMem16(r[5],ReadMem16(r[5] + 0x20) + DAT_8c001d6c);

		uint32_t param2_val_1 = ((DAT_8c001d70>>10) & (local_10 >> 10)) |
		(local_10  & DAT_8c001d78);

		WriteMem16(r[5] + 4,param2_val_1);
		WriteMem16(r[5] + 8,local_10 & 0xff);
		#endif

/*
		    uVar12 = (ushort)param_1[2];
    puVar11 = param_1[1];
    local_10 = *param_1;
LAB_8c001cf2:
                    // GDCC_PLAY_SECTOR
    *param_2 = param_2[0x20] + DAT_8c001d6c;
    uVar3 = (ushort)DAT_8c001d78;
    param_2[1] = (ushort)((uint)DAT_8c001d70 >> 0x10) & (ushort)((uint)local_10 >> 0x10) |
                 (ushort)local_10 & uVar3;
    param_2[2] = (ushort)local_10 & 0xff;
    param_2[3] = uVar12 & 0xf;
    param_2[4] = (ushort)((uint)DAT_8c001d70 >> 0x10) & (ushort)((uint)puVar11 >> 0x10) |
                 uVar3 & (ushort)puVar11;
    param_2[5] = (ushort)puVar11 & 0xff;
    uVar10 = FUN_8c002bb6(param_2);
    uVar6 = FUN_8c002948(uVar10,param_2);
    return (uint *)uVar6; */
	}
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

static void process_cmds() {
	for (const auto& i : g_reios_ctx.gd_q.get_pending_ops()) {
		if(i.stat != k_gd_cmd_st_active)
			continue;
		GD_HLE_Command(i.cmd, i.data);

		g_reios_ctx.gd_q.rm_cmd(i);
	}
}

void gdrom_hle_op()
{
	{
		static std::string s;
		s = "hle_op:";
		for(size_t i = 4;i <= 7;++i)
			s += "r("+std::to_string(i)+"):)" + std::to_string(r[i]);
		s+="\n";
		gd_sess_dmp.write(s);
	}

	if ((r[6]==0))	 g_reios_ctx.last_gd_op = r[4];

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
				for (size_t i = 0; i < 4;++i) 
					WriteMem32(r[5] + (i << 2U), g_reios_ctx.st[i].u_32);
			}
		}
		break;

			// NO return, NO params
		case GDROM_MAIN: {
			
			debugger_pass_data("gdrom_main_cmd",nullptr,0);
			debugf("\nGDROM:\tHLE GDROM_MAIN\n");
			//process_cmds();
			if (-1!=g_reios_ctx.last_gd_op) {
				GD_HLE_Command(g_reios_ctx.last_gd_op,0);
				g_reios_ctx.last_gd_op=-1;
			}
			
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

/*
	CdDA=0x00,
	CdRom=0x10,
	CdRom_XA=0x20,
	CdRom_Extra=0x30,
	CdRom_CDI=0x40,
	GdRom=0x80,
*/
		case GDROM_CHECK_DRIVE: {	// 
			printf("\nGDROM:\tHLE GDROM_CHECK_DRIVE is_gd (%d) r4:%X r5:%X (STAT = %u /TYPE = %u /q=%u)\n", g_reios_ctx.is_gdrom,r[4], r[5], libCore_gdrom_get_status(), g_GDRDisc->GetDiscType(), g_reios_ctx.gd_q.get_pending_ops().size());
		 	
			if (r[4] != 0) {
				WriteMem32(r[4] + 0, libCore_gdrom_get_status());	// STANDBY
				WriteMem32(r[4] + 4,  (g_reios_ctx.is_gdrom) ? 0x80 : g_GDRDisc->GetDiscType());
			}
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
		//r[0] = 0;
		printf("SYSCALL:\tSYSCALL: %X\n",r[7]);
	}
	
//	m_main_mtx.unlock();
}
