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

#ifndef UNIT_TESTS
#define TAIL_CALLING 1
#endif

#if	HOST_CPU == CPU_X64 && FEAT_AREC != DYNAREC_NONE

#include "deps/xbyak/xbyak.h"
#include "deps/xbyak/xbyak_util.h"
using namespace Xbyak::util;
#include "arm7.h"
#include "arm_emitter/arm_coding.h"

extern u32 arm_single_op(u32 opcode);
extern "C" void CompileCode();
extern "C" void CPUFiq();
extern "C" void arm_dispatch();

extern u8* icPtr;
extern u8* ICache;
extern const u32 ICacheSize;
extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

#ifdef _WIN32
static const Xbyak::Reg32 x86_regs[] = { ecx, edx, r8d, r9d, ebx, eax, eax, eax, eax, r15d };
#else
static const Xbyak::Reg32 x86_regs[] = { edi, esi, edx, ecx, ebx, eax, eax, eax, eax, r15d };
#endif
#ifdef TAIL_CALLING
extern "C" u32 (**entry_points)();
#endif
u32 (**entry_points)();
static Xbyak::CodeGenerator *assembler;

extern "C" void armFlushICache(void *bgn, void *end) {
}

static Xbyak::Reg32 arm_to_x86_reg(ARM::eReg reg)
{
	verify((int)reg < ARRAY_SIZE(x86_regs));
	verify(x86_regs[(int)reg] != eax);
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

void armv_call(void* loc, bool expect_result)
{
	assembler->call(loc);
	if (expect_result)
		assembler->mov(x86_regs[0], eax);
}

void armv_setup()
{
	assembler = new Xbyak::CodeGenerator(ICache + ICacheSize - icPtr, icPtr);
#ifndef TAIL_CALLING
#ifdef _WIN32
	assembler->sub(rsp, 40);	// 16-byte alignment + 32-byte shadow area
#else
	assembler->sub(rsp, 8);		// 16-byte alignment
#endif
#endif
}

void armv_intpr(u32 opcd)
{
	//Call interpreter
	assembler->mov(x86_regs[0], opcd);
	armv_call((void*)&arm_single_op, false);	// no need to move eax into edi/ecx
#ifdef TAIL_CALLING
	assembler->sub(r14d, eax);
#else
	assembler->sub(dword[rip + &arm_Reg[CYCL_CNT].I], eax);
#endif
}

void armv_end(void* codestart, u32 cycles)
{
#ifdef TAIL_CALLING
	assembler->sub(r14d, cycles);
	assembler->jmp((void*)&arm_dispatch);
#else
	assembler->mov(eax, cycles);
#ifdef _WIN32
	assembler->add(rsp, 40);
#else
	assembler->add(rsp, 8);
#endif
	assembler->ret();
#endif
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
	const int op_type = (opcode >> 21) & 15;
	const bool set_flags = opcode & (1 << 20);
	const bool logical_op_set_flags = set_flags &&
			(op_type == 0 || op_type == 1 || op_type == 8 || op_type == 9			// AND, EOR, TST, TEQ
			 || op_type == 12 || op_type == 13 || op_type == 15 || op_type == 14);	// ORR, MOV, MVN, BIC
	bool set_carry_bit = false;
	Xbyak::Operand op2;
	u32 imm_value;

	if (opcode & (1 << 25))
	{
		// op2 is imm8r4
		u32 rotate = ((opcode >> 8) & 15) << 1;
		u32 imm8 = opcode & 0xff;
		imm_value = (imm8 >> rotate) | (imm8 << (32 - rotate));
		if (logical_op_set_flags && rotate != 0)
		{
			set_carry_bit = true;
			assembler->mov(r10d, imm_value);
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
			if (shift != ARM::S_ROR && shift_imm != 0 && !logical_op_set_flags)
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
					if (logical_op_set_flags)
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
				if (logical_op_set_flags)
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
			if (!op2.isNone())
				assembler->and_(rd, op2);
			else
				assembler->and_(rd, imm_value);
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
			if (!op2.isNone())
				assembler->xor_(rd, op2);
			else
				assembler->xor_(rd, imm_value);
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
			if (op2.isNone())
				assembler->sub(rd, imm_value);
			else
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
			if (op2.isNone())
				assembler->add(rd, imm_value);
			else
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
			if (op2.isNone())
				assembler->add(rd, imm_value);
			else
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
			if (!op2.isNone())
				assembler->or_(rd, op2);
			else
				assembler->or_(rd, imm_value);
		}
		save_v_flag = false;
		break;
	case 14:	// BIC
		if (op2.isNone())
		{
			assembler->mov(eax, imm_value);
			op2 = eax;
		}
		assembler->andn(rd, static_cast<Xbyak::Reg32&>(op2), rn);
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
			if (op2.isNone())
				assembler->adc(rd, imm_value);
			else
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
		assembler->neg(r11d);
		assembler->cmc();		// on arm, -1 if carry is clear
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
			if (op2.isNone())
				assembler->sbb(rd, imm_value);
			else
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
		assembler->neg(r11d);
		assembler->cmc();		// on arm, -1 if carry is clear
		if (op2 == rd)
			assembler->sbb(rd, rn);
		else
		{
			if (op2.isNone())
				assembler->mov(rd, imm_value);
			else if (rd != op2)
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
		if (!op2.isNone())
			assembler->test(rn, static_cast<Xbyak::Reg32&>(op2));
		else
			assembler->test(rn, imm_value);
		save_v_flag = false;
		break;
	case 9:		// TEQ
		if (!op2.isNone())
			assembler->xor_(rn, op2);
		else
			assembler->xor_(rn, imm_value);
		save_v_flag = false;
		break;
	case 10:	// CMP
		if (!op2.isNone())
			assembler->cmp(rn, op2);
		else
			assembler->cmp(rn, imm_value);
		if (set_flags)
		{
			assembler->setnb(r10b);
			set_carry_bit = true;
		}
		break;
	case 11:	// CMN
		if (!op2.isNone())
			assembler->add(rn, op2);
		else
			assembler->add(rn, imm_value);
		if (set_flags)
		{
			assembler->setb(r10b);
			set_carry_bit = true;
		}
		break;
	case 13:	// MOV
		if (op2.isNone())
			assembler->mov(rd, imm_value);
		else if (op2 != rd)
			assembler->mov(rd, op2);
		if (set_flags)
		{
			assembler->test(rd, rd);
			save_v_flag = false;
		}
		break;
	case 15:	// MVN
		if (op2.isNone())
			assembler->mov(rd, ~imm_value);
		else
		{
			if (op2 != rd)
				assembler->mov(rd, op2);
			assembler->not_(rd);
		}
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

#ifndef TAIL_CALLING
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
			"push %r15				\n\t"
			"push %rbx				\n\t"
	);
	while ((int)arm_Reg[CYCL_CNT].I > 0)
	{
		if (arm_Reg[INTR_PEND].I)
			CPUFiq();

		arm_Reg[CYCL_CNT].I -= entry_points[(arm_Reg[R15_ARM_NEXT].I & (ARAM_SIZE_MAX - 1)) / 4]();
	}
	__asm__ (
			"pop %rbx				\n\t"
			"pop %r15				\n\t"
	);
}

#else

#ifdef __MACH__
#define _U "_"
#else
#define _U
#endif
__asm__ (
		".globl arm_compilecode				\n"
	"arm_compilecode:						\n\t"
		"call " _U"CompileCode				\n\t"
		"jmp " _U"arm_dispatch				\n\t"

		".globl arm_mainloop				\n"
	"arm_mainloop:							\n\t"	//  arm_mainloop(cycles, regs, entry points)
		"pushq %r14							\n\t"
		"pushq %r15							\n\t"
		"pushq %rbx							\n\t"
#ifdef _WIN32
		"subq $32, %rsp						\n\t"
#endif

		"movl arm_Reg + 192(%rip), %r14d	\n\t"	// CYCL_CNT
#ifdef _WIN32
		"add %ecx, %r14d					\n\t"	// add cycles for this timeslice
		"movq %r8, entry_points(%rip)		\n\t"
#else
		"add %edi, %r14d					\n\t"	// add cycles for this timeslice
		"movq %rdx, entry_points(%rip)		\n\t"
#endif

		".globl arm_dispatch				\n"
	"arm_dispatch:							\n\t"
		"movq entry_points(%rip), %rdx		\n\t"
		"movl arm_Reg + 184(%rip), %ecx		\n\t"	// R15_ARM_NEXT
		"movl arm_Reg + 188(%rip), %eax		\n\t"	// INTR_PEND
		"cmp $0, %r14d						\n\t"
		"jle 2f								\n\t"	// timeslice is over
		"test %eax, %eax					\n\t"
		"jne 1f								\n\t"	// if interrupt pending, handle it

		"and $0x7ffffc, %ecx				\n\t"
		"jmp *(%rdx, %rcx, 2)				\n"

	"1:										\n\t"	// arm_dofiq:
		"call " _U"CPUFiq					\n\t"
		"jmp " _U"arm_dispatch				\n"

	"2:										\n\t"	// arm_exit:
		"movl %r14d, arm_Reg + 192(%rip)	\n\t"	// CYCL_CNT: save remaining cycles
#ifdef _WIN32
		"addq $32, %rsp						\n\t"
#endif
		"popq %rbx							\n\t"
		"popq %r15							\n\t"
		"popq %r14							\n\t"
		"ret								\n"
);
#endif // TAIL_CALLING
#endif // X64 && DYNAREC_JIT
