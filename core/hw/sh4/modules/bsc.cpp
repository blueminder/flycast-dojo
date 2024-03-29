//Bus state controller registers

#include "types.h"
#include "hw/sh4/sh4_mmr.h"

#include "hw/naomi/naomi.h"
#include "cfg/option.h"

BSC_PDTRA_type BSC_PDTRA;

static void write_BSC_PCTRA(u32 addr, u32 data)
{
	BSC_PCTRA.full = data;
	if (settings.platform.isNaomi())
		NaomiBoardIDWriteControl((u16)data);
	//else
	//printf("C:BSC_PCTRA = %08X\n",data);
}

//u32 port_out_data;
static void write_BSC_PDTRA(u32 addr, u32 data)
{
	BSC_PDTRA.full = (u16)data;
	//printf("D:BSC_PDTRA = %04x\n", (u16)data);

	if (settings.platform.isNaomi())
		NaomiBoardIDWrite((u16)data);
}

static u32 read_BSC_PDTRA(u32 addr)
{
	if (settings.platform.isNaomi())
	{
		return NaomiBoardIDRead();
	}
	else
	{
		/* as seen on chankast */
		u32 tpctra = BSC_PCTRA.full;
		u32 tpdtra = BSC_PDTRA.full;
		
		u32 tfinal=0;
		// magic values
		if ((tpctra & 0xf) == 0x8)
			tfinal = 3;
		else if ((tpctra & 0xf) == 0xB)
			tfinal = 3;
		else			
			tfinal = 0;

		if ((tpctra & 0xf) == 0xB && (tpdtra & 0xf) == 2)
			tfinal = 0;
		else if ((tpctra & 0xf) == 0xC && (tpdtra & 0xf) == 2)
			tfinal = 3;      

		tfinal |= config::Cable << 8;

		return tfinal;
	}
}

//Init term res
void bsc_init()
{
	//BSC BCR1 0xFF800000 0x1F800000 32 0x00000000 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_BCR1_addr, 0x033efffd>();

	//BSC BCR2 0xFF800004 0x1F800004 16 0x3FFC Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_BCR2_addr, 0x3ffd>();

	//BSC WCR1 0xFF800008 0x1F800008 32 0x77777777 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_WCR1_addr, 0x77777777>();

	//BSC WCR2 0xFF80000C 0x1F80000C 32 0xFFFEEFFF Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_WCR2_addr, 0xfffeefff>();

	//BSC WCR3 0xFF800010 0x1F800010 32 0x07777777 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_WCR3_addr, 0x07777777>();

	//BSC MCR 0xFF800014 0x1F800014 32 0x00000000 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_MCR_addr, 0xf8bbffff>();

	//BSC PCR 0xFF800018 0x1F800018 16 0x0000 Held Held Held Bclk
	sh4_rio_reg16<BSC, BSC_PCR_addr>();

	//BSC RTCSR 0xFF80001C 0x1F80001C 16 0x0000 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_RTCSR_addr, 0x00ff>();

	//BSC RTCNT 0xFF800020 0x1F800020 16 0x0000 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_RTCNT_addr, 0x00ff>();

	//BSC RTCOR 0xFF800024 0x1F800024 16 0x0000 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_RTCOR_addr, 0x00ff>();

	//BSC RFCR 0xFF800028 0x1F800028 16 0x0000 Held Held Held Bclk
	// forced to 0x17 to help naomi/aw boot
	sh4_rio_reg(BSC, BSC_RFCR_addr, RIO_RO);

	//BSC PCTRA 0xFF80002C 0x1F80002C 32 0x00000000 Held Held Held Bclk
	sh4_rio_reg(BSC, BSC_PCTRA_addr, RIO_WF, nullptr, write_BSC_PCTRA);

	//BSC PDTRA 0xFF800030 0x1F800030 16 Undefined Held Held Held Bclk
	sh4_rio_reg(BSC, BSC_PDTRA_addr, RIO_FUNC, read_BSC_PDTRA, write_BSC_PDTRA);

	//BSC PCTRB 0xFF800040 0x1F800040 32 0x00000000 Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_PCTRB_addr, 0x000000ff>();

	//BSC PDTRB 0xFF800044 0x1F800044 16 Undefined Held Held Held Bclk
	sh4_rio_reg_wmask<BSC, BSC_PDTRB_addr, 0x000f>();

	//BSC GPIOIC 0xFF800048 0x1F800048 16 0x00000000 Held Held Held Bclk
	sh4_rio_reg16<BSC, BSC_GPIOIC_addr>();
}

void bsc_reset(bool hard)
{
	/*
	BSC BCR1 H'FF80 0000 H'1F80 0000 32 H'0000 0000*2 Held Held Held Bclk
	BSC BCR2 H'FF80 0004 H'1F80 0004 16 H'3FFC*2 Held Held Held Bclk
	BSC WCR1 H'FF80 0008 H'1F80 0008 32 H'7777 7777 Held Held Held Bclk
	BSC WCR2 H'FF80 000C H'1F80 000C 32 H'FFFE EFFF Held Held Held Bclk
	BSC WCR3 H'FF80 0010 H'1F80 0010 32 H'0777 7777 Held Held Held Bclk

	BSC MCR H'FF80 0014 H'1F80 0014 32 H'0000 0000 Held Held Held Bclk
	BSC PCR H'FF80 0018 H'1F80 0018 16 H'0000 Held Held Held Bclk
	BSC RTCSR H'FF80 001C H'1F80 001C 16 H'0000 Held Held Held Bclk
	BSC RTCNT H'FF80 0020 H'1F80 0020 16 H'0000 Held Held Held Bclk
	BSC RTCOR H'FF80 0024 H'1F80 0024 16 H'0000 Held Held Held Bclk
	BSC RFCR H'FF80 0028 H'1F80 0028 16 H'0000 Held Held Held Bclk
	BSC PCTRA H'FF80 002C H'1F80 002C 32 H'0000 0000 Held Held Held Bclk
	BSC PDTRA H'FF80 0030 H'1F80 0030 16 Undefined Held Held Held Bclk
	BSC PCTRB H'FF80 0040 H'1F80 0040 32 H'0000 0000 Held Held Held Bclk
	BSC PDTRB H'FF80 0044 H'1F80 0044 16 Undefined Held Held Held Bclk
	BSC GPIOIC H'FF80 0048 H'1F80 0048 16 H'0000 0000 Held Held Held Bclk
	BSC SDMR2 H'FF90 xxxx H'1F90 xxxx 8 Write-only Bclk
	BSC SDMR3 H'FF94 xxxx H'1F94 xxxx 8 Bclk
	*/
	BSC_BCR1.full = 0;
	BSC_BCR2.full = 0x3FFC;
	BSC_WCR1.full = 0x77777777;
	BSC_WCR2.full = 0xFFFEEFFF;
	BSC_WCR3.full = 0x07777777;

	BSC_MCR.full = 0;
	BSC_PCR.full = 0;
	BSC_RTCSR.full = 0;
	BSC_RTCNT.full = 0;
	BSC_RTCOR.full = 0;
	BSC_PCTRA.full = 0;
	if (hard)
		BSC_PDTRA.full = 0;
	BSC_PCTRB.full = 0;
	if (hard)
		BSC_PDTRB.full = 0;
	BSC_GPIOIC.full = 0;

	BSC_RFCR.full = 17;
}

void bsc_term()
{
}
