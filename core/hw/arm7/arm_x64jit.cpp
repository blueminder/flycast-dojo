/*
	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "build.h"

#if	HOST_CPU == CPU_X64 && FEAT_AREC != DYNAREC_NONE

#include "deps/xbyak/xbyak.h"
#include "deps/xbyak/xbyak_util.h"
using namespace Xbyak::util;
#include "arm7.h"
#include "arm_emitter/arm_coding.h"

extern u32 arm_single_op(u32 opcode);
extern "C" void CompileCode();
extern "C" void CPUFiq();

extern u8* icPtr;
extern u8* ICache;
extern const u32 ICacheSize;
extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

#ifdef _WIN32
static const Xbyak::Reg32 x86_regs[] = { ecx, edx, r8d, r9d, ebx, ebp, r12d, r13d, r14d, r15d };
#else
static const Xbyak::Reg32 x86_regs[] = { edi, esi, edx, ecx, ebx, ebp, r12d, r13d, r14d, r15d };
#endif
static u32 (**entry_points)();
static Xbyak::CodeGenerator *assembler;

extern "C" void armFlushICache(void *bgn, void *end) {
}

static Xbyak::Reg32 arm_to_x86_reg(ARM::eReg reg)
{
	verify((int)reg < ARRAY_SIZE(x86_regs));
	return x86_regs[(int)reg];
}

//helpers ...
void LoadReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL)
{
	DEBUG_LOG(AICA_ARM, "Loading r%d into %s", regn, arm_to_x86_reg(rd).toString());
	assembler->mov(arm_to_x86_reg(rd), dword[rip + &arm_Reg[regn].I]);
}

void StoreReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL)
{
	DEBUG_LOG(AICA_ARM, "Storing %s to r%d", arm_to_x86_reg(rd).toString(), regn);
	assembler->mov(dword[rip + &arm_Reg[regn].I], arm_to_x86_reg(rd));
}

const u32 N_FLAG = 1 << 31;
const u32 Z_FLAG = 1 << 30;
const u32 C_FLAG = 1 << 29;
const u32 V_FLAG = 1 << 28;

void *armv_start_conditional(ARM::ConditionCode cc)
{
	if (cc == ARM::CC_AL)
		return NULL;
	Xbyak::Label *label = new Xbyak::Label();
	cc = (ARM::ConditionCode)((u32)cc ^ 1);	// invert the condition
	assembler->mov(eax, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
	switch (cc)
	{
	case ARM::EQ:	// Z==1
		assembler->and_(eax, Z_FLAG);
		assembler->jnz(*label);
		break;
	case ARM::NE:	// Z==0
		assembler->and_(eax, Z_FLAG);
		assembler->jz(*label);
		break;
	case ARM::CS:	// C==1
		assembler->and_(eax, C_FLAG);
		assembler->jnz(*label);
		break;
	case ARM::CC:	// C==0
		assembler->and_(eax, C_FLAG);
		assembler->jz(*label);
		break;
	case ARM::MI:	// N==1
		assembler->and_(eax, N_FLAG);
		assembler->jnz(*label);
		break;
	case ARM::PL:	// N==0
		assembler->and_(eax, N_FLAG);
		assembler->jz(*label);
		break;
	case ARM::VS:	// V==1
		assembler->and_(eax, V_FLAG);
		assembler->jnz(*label);
		break;
	case ARM::VC:	// V==0
		assembler->and_(eax, V_FLAG);
		assembler->jz(*label);
		break;
	case ARM::HI:	// (C==1) && (Z==0)
		assembler->and_(eax, C_FLAG | Z_FLAG);
		assembler->cmp(eax, C_FLAG);
		assembler->jz(*label);
		break;
	case ARM::LS:	// (C==0) || (Z==1)
		assembler->and_(eax, C_FLAG | Z_FLAG);
		assembler->cmp(eax, C_FLAG);
		assembler->jnz(*label);
		break;
	case ARM::GE:	// N==V
		assembler->mov(ecx, eax);
		assembler->shl(ecx, 3);
		assembler->xor_(eax, ecx);
		assembler->and_(eax, N_FLAG);
		assembler->jz(*label);
		break;
	case ARM::LT:	// N!=V
		assembler->mov(ecx, eax);
		assembler->shl(ecx, 3);
		assembler->xor_(eax, ecx);
		assembler->and_(eax, N_FLAG);
		assembler->jnz(*label);
		break;
	case ARM::GT:	// (Z==0) && (N==V)
		assembler->mov(ecx, eax);
		assembler->mov(edx, eax);
		assembler->shl(ecx, 3);
		assembler->shl(edx, 1);
		assembler->xor_(eax, ecx);
		assembler->or_(eax, edx);
		assembler->and_(eax, N_FLAG);
		assembler->jz(*label);
		break;
	case ARM::LE:	// (Z==1) || (N!=V)
		assembler->mov(ecx, eax);
		assembler->mov(edx, eax);
		assembler->shl(ecx, 3);
		assembler->shl(edx, 1);
		assembler->xor_(eax, ecx);
		assembler->or_(eax, edx);
		assembler->and_(eax, N_FLAG);
		assembler->jnz(*label);
		break;
	default:
		die("Invalid condition code");
		break;
	}

	return label;
}

void armv_end_conditional(void *ref)
{
	if (ref != NULL)
	{
		Xbyak::Label *label = (Xbyak::Label *)ref;
		assembler->L(*label);
		delete label;
	}
}

// Arm flags aren't loaded into x86 FLAGS
void LoadFlags()
{
}

// x86 FLAGS are stored in armEmit32()
void StoreFlags()
{
}

void armv_imm_to_reg(u32 regn, u32 imm)
{
	assembler->mov(eax, imm);
	assembler->mov(dword[rip + &arm_Reg[regn].I], eax);
}

void armv_call(void* loc)
{
	assembler->call(loc);
	assembler->mov(x86_regs[0], eax);	// FIXME get rid of this
}

void armv_setup()
{
	assembler = new Xbyak::CodeGenerator(ICache + ICacheSize - icPtr, icPtr);
#ifdef _WIN32
	assembler->sub(rsp, 40);	// 32-byte alignment + 32-byte shadow area
#else
	assembler->sub(rsp, 8);		// 32-byte alignment
#endif
}

void armv_intpr(u32 opcd)
{
	//Call interpreter
	assembler->mov(x86_regs[0], opcd);
	armv_call((void*)&arm_single_op);
	assembler->sub(dword[rip + &arm_Reg[CYCL_CNT].I], eax);
}

void armv_end(void* codestart, u32 cycles)
{
	//Normal block end
	//cycle counter rv

	/*
	//pop registers & return
	assembler->sub(dword[rip + &arm_Reg[CYCL_CNT].I], cycles);
	assembler->js((void*)&arm_exit);

	assembler->jmp((void*)&arm_dispatch);
	*/
	assembler->mov(eax, cycles);
#ifdef _WIN32
	assembler->add(rsp, 40);
#else
	assembler->add(rsp, 8);
#endif
	assembler->ret();
	assembler->ready();
	icPtr += assembler->getSize();

	delete assembler;
	assembler = nullptr;
}

void armv_MOV32(ARM::eReg regn, u32 imm)
{
	assembler->mov(arm_to_x86_reg(regn), imm);
}

void armv_mov(ARM::eReg regd, ARM::eReg regn)
{
	if (regn != regd)
		assembler->mov(arm_to_x86_reg(regd), arm_to_x86_reg(regn));
}

void armv_add(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
{
	if (regn != regd)
		assembler->mov(arm_to_x86_reg(regd), arm_to_x86_reg(regn));
	assembler->add(arm_to_x86_reg(regd), arm_to_x86_reg(regm));
}

void armv_sub(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
{
	if (regn != regd)
		assembler->mov(arm_to_x86_reg(regd), arm_to_x86_reg(regn));
	assembler->sub(arm_to_x86_reg(regd), arm_to_x86_reg(regm));
}

void armv_add(ARM::eReg regd, ARM::eReg regn, s32 imm)
{
	if (regn != regd)
		assembler->mov(arm_to_x86_reg(regd), arm_to_x86_reg(regn));
	assembler->add(arm_to_x86_reg(regd), imm);
}

void armv_lsl(ARM::eReg regd, ARM::eReg regn, u32 imm)
{
	if (regn != regd)
		assembler->mov(arm_to_x86_reg(regd), arm_to_x86_reg(regn));
	assembler->shl(arm_to_x86_reg(regd), imm);
}

void armv_bic(ARM::eReg regd, ARM::eReg regn, u32 imm)
{
	if (regn != regd)
		assembler->mov(arm_to_x86_reg(regd), arm_to_x86_reg(regn));
	assembler->and_(arm_to_x86_reg(regd), ~imm);
}

void armEmit32(u32 opcode)
{
	const Xbyak::Reg32 rd = arm_to_x86_reg((ARM::eReg)((opcode >> 12) & 15));
	const Xbyak::Reg32 rn = arm_to_x86_reg((ARM::eReg)((opcode >> 16) & 15));
	bool set_flags = opcode & (1 << 20);
	Xbyak::Reg32 op2;
	int op_type = (opcode >> 21) & 15;
	bool logical_op = op_type == 0 || op_type == 1 || op_type == 8 || op_type == 9	// AND, EOR, TST, TEQ
			 || op_type == 12 || op_type == 13 || op_type == 15 || op_type == 14;	// ORR, MOV, MVN, BIC
	bool set_carry_bit = false;

	if (opcode & (1 << 25))
	{
		// op2 is imm8r4
		u32 rotate = ((opcode >> 8) & 15) << 1;
		u32 imm8 = opcode & 0xff;
		assembler->mov(eax, (imm8 >> rotate) | (imm8 << (32 - rotate)));
		op2 = eax;
		if (set_flags && logical_op && rotate != 0)
		{
			set_carry_bit = true;
			assembler->mov(r10d, eax);
			assembler->shr(r10d, 31);
		}
	}
	else
	{
		// op2 is register
		const Xbyak::Reg32 rm = arm_to_x86_reg((ARM::eReg)(opcode & 15));

		ARM::ShiftOp shift = (ARM::ShiftOp)((opcode >> 5) & 3);

		if (opcode & (1 << 4))
		{
			// shift by register
			const Xbyak::Reg32 shift_reg = arm_to_x86_reg((ARM::eReg)((opcode >> 8) & 15));
			assembler->mov(r11d, ecx);
			switch (shift)
			{
			case ARM::S_LSL:
			case ARM::S_LSR:
				assembler->mov(ecx, shift_reg);
				assembler->mov(eax, 0);
				if (shift == ARM::S_LSL)
					assembler->shl(rm, cl);
				else
					assembler->shr(rm, cl);
				assembler->cmp(shift_reg, 32);
				assembler->cmovb(eax, rm);		// LSL and LSR by 32 or more gives 0
				break;
			case ARM::S_ASR:
				assembler->mov(ecx, shift_reg);
				assembler->mov(eax, rm);
				assembler->sar(eax, 31);
				assembler->sar(rm, cl);
				assembler->cmp(shift_reg, 32);
				assembler->cmovb(eax, rm);		// ASR by 32 or more gives 0 or -1 depending on operand sign
				break;
			case ARM::S_ROR:
				assembler->mov(ecx, shift_reg);
				assembler->mov(eax, rm);
				assembler->ror(eax, cl);
				break;
			default:
				die("Invalid shift");
				break;
			}
			assembler->mov(ecx, r11d);
			op2 = eax;
		}
		else
		{
			// shift by immediate
			u32 shift_imm = (opcode >> 7) & 0x1f;
			if (shift != ARM::S_ROR && shift_imm != 0 && !(set_flags && logical_op))
			{
				assembler->mov(eax, rm);
				switch (shift)
				{
				case ARM::S_LSL:
					assembler->shl(eax, shift_imm);
					break;
				case ARM::S_LSR:
					assembler->shr(eax, shift_imm);
					break;
				case ARM::S_ASR:
					assembler->sar(eax, shift_imm);
					break;
				}
				op2 = eax;
			}
			else if (shift_imm == 0)
			{
				if (shift == ARM::S_LSL)
				{
					op2 = rm;		// LSL 0 is a no-op
				}
				else
				{
					// Shift by 32
					if (set_flags && logical_op)
						set_carry_bit = true;
					if (shift == ARM::S_LSR)
					{
						if (set_carry_bit)
						{
							assembler->mov(r10d, rm);			// r10d = rm[31]
							assembler->shr(r10d, 31);
						}
						assembler->mov(eax, 0);					// eax = 0
					}
					else if (shift == ARM::S_ASR)
					{
						if (set_carry_bit)
						{
							assembler->mov(r10d, rm);			// r10d = rm[31]
							assembler->shr(r10d, 31);
						}
						assembler->mov(eax, rm);
						assembler->sar(eax, 31);				// eax = rm < 0 ? -1 : 0
					}
					else if (shift == ARM::S_ROR)
					{
						// RRX
						assembler->mov(r10d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
						assembler->shl(r10d, 2);
						assembler->and_(r10d, 0x80000000);		// ecx[31] = C
						assembler->mov(eax, rm);
						assembler->shr(eax, 1);					// eax = rm >> 1
						assembler->or_(eax, r10d);				// eax[31] = C
						if (set_carry_bit)
						{
							assembler->mov(r10d, rm);
							assembler->and_(r10d, 1);			// ecx = rm[0] (new C)
						}
					}
					else
						die("Invalid shift");
					op2 = eax;
				}
			}
			else
			{
				// Carry must be preserved or Ror shift
				if (set_flags && logical_op)
					set_carry_bit = true;
				if (shift == ARM::S_LSL)
				{
					if (set_carry_bit)
						assembler->mov(r10d, rm);
					assembler->mov(eax, rm);
					if (set_carry_bit)
						assembler->shr(r10d, 32 - shift_imm);
					assembler->shl(eax, shift_imm);			// eax <<= shift
					if (set_carry_bit)
						assembler->and_(r10d, 1);				// r10d = rm[lsb]
				}
				else
				{
					if (set_carry_bit)
					{
						assembler->mov(r10d, rm);
						assembler->shr(r10d, shift_imm - 1);
						assembler->and_(r10d, 1);				// r10d = rm[msb]
					}

					assembler->mov(eax, rm);
					if (shift == ARM::S_LSR)
						assembler->shr(eax, shift_imm);			// eax >>= shift
					else if (shift == ARM::S_ASR)
						assembler->sar(eax, shift_imm);
					else if (shift == ARM::S_ROR)
						assembler->ror(eax, shift_imm);
					else
						die("Invalid shift");
				}
				op2 = eax;
			}
		}
	}

	bool save_v_flag = true;
	switch (op_type)
	{
	case 0:		// AND
		if (op2 == rd)
			assembler->and_(rd, rn);
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->and_(rd, op2);
		}
		save_v_flag = false;
		break;
	case 1:		// EOR
		if (op2 == rd)
			assembler->xor_(rd, rn);
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->xor_(rd, op2);
		}
		save_v_flag = false;
		break;
	case 2:		// SUB
		if (op2 == rd)
		{
			assembler->sub(rn, op2);
			if (rd != rn)
				assembler->mov(rd, rn);
		}
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->sub(rd, op2);
		}
		if (set_flags)
		{
			assembler->setnb(r10b);
			set_carry_bit = true;
		}
		break;
	case 3:		// RSB
		if (op2 == rd)
			assembler->sub(rd, rn);
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->neg(rd);
			assembler->add(rd, op2);
		}
		if (set_flags)
		{
			assembler->setnb(r10b);
			set_carry_bit = true;
		}
		break;
	case 4:		// ADD
		if (op2 == rd)
			assembler->add(rd, rn);
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->add(rd, op2);
		}
		if (set_flags)
		{
			assembler->setb(r10b);
			set_carry_bit = true;
		}
		break;
	case 12:	// ORR
		if (op2 == rd)
			assembler->or_(rd, rn);
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->or_(rd, op2);
		}
		save_v_flag = false;
		break;
	case 14:	// BIC
		assembler->andn(rd, op2, rn);
		save_v_flag = false;
		break;
	case 5:		// ADC
		assembler->mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
		assembler->and_(r11d, C_FLAG);
		assembler->neg(r11d);
		if (op2 == rd)
			assembler->adc(rd, rn);
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->adc(rd, op2);
		}
		if (set_flags)
		{
			assembler->setb(r10b);
			set_carry_bit = true;
		}
		break;
	case 6:		// SBC
		// rd = rn - op2 - !C
		assembler->mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
		assembler->and_(r11d, C_FLAG);
		assembler->xor_(r11d, C_FLAG);	// on arm, -1 if carry is clear
		assembler->neg(r11d);
		if (op2 == rd)
		{
			assembler->sbb(rn, op2);
			if (rd != rn)
				assembler->mov(rd, rn);
		}
		else
		{
			if (rd != rn)
				assembler->mov(rd, rn);
			assembler->sbb(rd, op2);
		}
		if (set_flags)
		{
			assembler->setnb(r10b);
			set_carry_bit = true;
		}
		break;
	case 7:		// RSC
		// rd = op2 - rn - !C
		assembler->mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
		assembler->and_(r11d, C_FLAG);
		assembler->xor_(r11d, C_FLAG);	// on arm, -1 if carry is clear
		assembler->neg(r11d);
		if (op2 == rd)
			assembler->sbb(rd, rn);
		else
		{
			if (rd != op2)
				assembler->mov(rd, op2);
			assembler->sbb(rd, rn);
		}
		if (set_flags)
		{
			assembler->setnb(r10b);
			set_carry_bit = true;
		}
		break;
	case 8:		// TST
		assembler->test(rn, op2);
		save_v_flag = false;
		break;
	case 9:		// TEQ
		assembler->xor_(rn, op2);
		assembler->test(rn, rn);
		save_v_flag = false;
		break;
	case 10:	// CMP
		assembler->cmp(rn, op2);
		if (set_flags)
		{
			assembler->setnb(r10b);
			set_carry_bit = true;
		}
		break;
	case 11:	// CMN
		assembler->add(rn, op2);
		if (set_flags)
		{
			assembler->setb(r10b);
			set_carry_bit = true;
		}
		break;
	case 13:	// MOV
		if (op2 != rd)
			assembler->mov(rd, op2);
		if (set_flags)
		{
			assembler->test(rd, rd);
			save_v_flag = false;
		}
		break;
	case 15:	// MVN
		if (op2 != rd)
			assembler->mov(rd, op2);
		assembler->not_(rd);
		if (set_flags)
		{
			assembler->test(rd, rd);
			save_v_flag = false;
		}
		break;
	}
	if (set_flags)
	{
		assembler->pushf();
		assembler->pop(rax);

		if (save_v_flag)
		{
			assembler->mov(r11d, eax);
			assembler->shl(r11d, 28 - 11);		// V
		}
		assembler->shl(eax, 30 - 6);			// Z,N
		if (save_v_flag)
			assembler->and_(r11d, V_FLAG);		// V
		assembler->and_(eax, Z_FLAG | N_FLAG);	// Z,N
		if (save_v_flag)
			assembler->or_(eax, r11d);

		assembler->mov(r11d, dword[rip + &arm_Reg[RN_PSR_FLAGS].I]);
		if (set_carry_bit)
		{
			if (save_v_flag)
				assembler->and_(r11d, ~(Z_FLAG | N_FLAG | C_FLAG | V_FLAG));
			else
				assembler->and_(r11d, ~(Z_FLAG | N_FLAG | C_FLAG));
			assembler->shl(r10d, 29);
			assembler->or_(r11d, r10d);
		}
		else
		{
			if (save_v_flag)
				assembler->and_(r11d, ~(Z_FLAG | N_FLAG | V_FLAG));
			else
				assembler->and_(r11d, ~(Z_FLAG | N_FLAG));
		}
		assembler->or_(r11d, eax);
		assembler->mov(dword[rip + &arm_Reg[RN_PSR_FLAGS].I], r11d);
	}
}

extern "C"
u32 arm_compilecode()
{
	CompileCode();
	return 0;
}

extern "C"
void arm_mainloop(u32 cycl, void* regs, void* entrypoints)
{
	entry_points = (u32 (**)())entrypoints;
	arm_Reg[CYCL_CNT].I += cycl;
	__asm__ (
			"push %r12				\n\t"
			"push %r13				\n\t"
			"push %r14				\n\t"
			"push %r15				\n\t"
			"push %rbx				\n\t"
			"push %rbp				\n\t"
	);
	while ((int)arm_Reg[CYCL_CNT].I > 0)
	{
		if (arm_Reg[INTR_PEND].I)
			CPUFiq();

		arm_Reg[CYCL_CNT].I -= entry_points[(arm_Reg[R15_ARM_NEXT].I & (ARAM_SIZE_MAX - 1)) / 4]();
	}
	__asm__ (
			"pop %rbp				\n\t"
			"pop %rbx				\n\t"
			"pop %r15				\n\t"
			"pop %r14				\n\t"
			"pop %r13				\n\t"
			"pop %r12				\n\t"
	);
}

#if 0
//
// Dynarec main loop
//
// w25 is used for temp mem save (post increment op2)
// x26 is the entry points table
// w27 is the cycle counter
// x28 points to the arm7 registers base
#ifdef __MACH__
#define _U "_"
#else
#define _U
#endif

		".globl arm_compilecode				\n"
	"arm_compilecode:						\n\t"
		"call " _U "CompileCode				\n\t"
		"jmp " _U "arm_dispatch				\n\t"

		".globl arm_mainloop				\n"
	"arm_mainloop:							\n\t"	//  arm_mainloop(cycles, regs, entry points)
		"push r12							\n\t"
		"push r13							\n\t"
		"push r14							\n\t"
		"push r15							\n\t"

//		"mov x28, x1						\n\t"	// arm7 registers
//		"mov x26, x2						\n\t"	// lookup base

		"ldr w27, [x28, #192]				\n\t"	// cycle count
		"add w27, w27, w0					\n\t"	// add cycles for this timeslice

		".globl arm_dispatch				\n\t"
		".hidden arm_dispatch				\n"
	"arm_dispatch:							\n\t"
		"ldp w0, w1, [x28, #184]			\n\t"	// load Next PC, interrupt
		"ubfx w2, w0, #2, #21				\n\t"	// w2 = pc >> 2. Note: assuming address space == 8 MB (23 bits)
		"cbnz w1, arm_dofiq					\n\t"	// if interrupt pending, handle it

		"add x2, x26, x2, lsl #3			\n\t"	// x2 = EntryPoints + pc << 1
		"ldr x3, [x2]						\n\t"
		"br x3								\n"

	"arm_dofiq:								\n\t"
		"bl CPUFiq							\n\t"
		"b arm_dispatch						\n\t"

		".globl arm_exit					\n\t"
		".hidden arm_exit					\n"
	"arm_exit:								\n\t"
		"str w27, [x28, #192]				\n\t"	// if timeslice is over, save remaining cycles
		"pop r15							\n\t"
		"pop r14							\n\t"
		"pop r13							\n\t"
		"pop r12							\n\t"
		"ret								\n"
);
#endif // 0
#endif // X64 && DYNAREC_JIT
