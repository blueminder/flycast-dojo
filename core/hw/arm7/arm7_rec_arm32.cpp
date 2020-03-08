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

#if HOST_CPU == CPU_ARM && FEAT_AREC != DYNAREC_NONE
#include "arm7_rec.h"
#include "hw/mem/_vmem.h"

#define _DEVEL          (1)
#define EMIT_I          armEmit32((I))
#define EMIT_GET_PTR()  armGetEmitPtr()
static void armEmit32(u32 emit32);
static void *armGetEmitPtr();
#include "arm_emitter/arm_emitter.h"
#undef I
using namespace ARM;

extern "C" void arm_dispatch();
extern "C" void arm_exit();

extern u8* icPtr;
extern u8* ICache;
extern const u32 ICacheSize;
extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

static void loadReg(eReg host_reg, Arm7Reg guest_reg)
{
	LDR(host_reg, r8, (u8*)&arm_Reg[guest_reg].I - (u8*)&arm_Reg[0].I);
}

static void storeReg(eReg host_reg, Arm7Reg guest_reg)
{
	STR(host_reg, r8, (u8*)&arm_Reg[guest_reg].I - (u8*)&arm_Reg[0].I);
}

static const std::array<eReg, 5> alloc_regs{
	r6, r7, r9, r10, r11
};

class Arm32ArmRegAlloc : public ArmRegAlloc<alloc_regs.size(), Arm32ArmRegAlloc>
{
	using super = ArmRegAlloc<alloc_regs.size(), Arm32ArmRegAlloc>;

	void LoadReg(int host_reg, Arm7Reg armreg)
	{
		// printf("LoadReg R%d <- r%d\n", host_reg, armreg);
		loadReg(getReg(host_reg), armreg);
	}

	void StoreReg(int host_reg, Arm7Reg armreg)
	{
		// printf("StoreReg R%d -> r%d\n", host_reg, armreg);
		storeReg(getReg(host_reg), armreg);
	}

	static eReg getReg(int i)
	{
		verify(i >= 0 && (u32)i < alloc_regs.size());
		return alloc_regs[i];
	}

public:
	Arm32ArmRegAlloc(const std::vector<ArmOp>& block_ops)
		: super(block_ops) {}

	eReg map(Arm7Reg r)
	{
		int i = super::map(r);
		return getReg(i);
	}

	friend super;
};

static void armEmit32(u32 emit32)
{
	if (icPtr >= ICache + ICacheSize - 1024)
	{
		ERROR_LOG(AICA_ARM, "JIT buffer full: %zd bytes free", ICacheSize - (icPtr - ICache));
		die("AICA ARM code buffer full");
	}

	*(u32*)icPtr = emit32;
	icPtr += 4;
}

static void *armGetEmitPtr()
{
	return icPtr;
}

static Arm32ArmRegAlloc *regalloc;
static bool set_flags;
static bool logical_op_set_flags;
static bool set_carry_bit;

static void loadFlags()
{
	//Load flags
	loadReg(r0, RN_PSR_FLAGS);
	//move them to flags register
	MSR(0, 8, r0);
}

static void storeFlags()
{
	//get results from flags register
	MRS(r1, 0);
	//Store flags
	storeReg(r1, RN_PSR_FLAGS);
}

u32 *startConditional(ArmOp::Condition cc)
{
	if (cc == ArmOp::AL)
		return nullptr;
	verify(cc <= ArmOp::LE);
	ARM::ConditionCode condition = (ARM::ConditionCode)((u32)cc ^ 1);
	u32 *code = (u32 *)icPtr;
	JUMP((u32)code, condition);

	return code;
}

void endConditional(u32 *pos)
{
	if (pos != nullptr)
	{
		u32 *curpos = (u32 *)icPtr;
		ARM::ConditionCode condition = (ARM::ConditionCode)(*pos >> 28);
		icPtr = (u8 *)pos;
		JUMP((u32)curpos, condition);
		icPtr = (u8 *)curpos;
	}
}

eReg getOperand(ArmOp::Operand arg, eReg scratch_reg)
{
	if (arg.isNone())
		return (eReg)-1;
	else if (arg.isImmediate())
	{
		if (is_i8r4(arg.getImmediate()))
			MOV(scratch_reg, arg.getImmediate());
		else
			MOV32(scratch_reg, arg.getImmediate());
	}
	else if (arg.isReg())
	{
		if (!arg.isShifted())
			return regalloc->map(arg.getReg().armreg);
		MOV(scratch_reg, regalloc->map(arg.getReg().armreg));
	}

	if (!arg.shift_imm)
	{
		// Shift by register
		eReg shift_reg = regalloc->map(arg.shift_reg.armreg);

		switch (arg.shift_type)
		{
		case ArmOp::LSL:
		case ArmOp::LSR:
			verify(scratch_reg != r0);
			MRS(r0, 0);
			CMP(shift_reg, 32);
			if (arg.shift_type == ArmOp::LSL)
				LSL(scratch_reg, scratch_reg, shift_reg);
			else
				LSR(scratch_reg, scratch_reg, shift_reg);
			MOV(scratch_reg, 0, GE);			// LSL and LSR by 32 or more gives 0
			MSR(0, 8, r0);
			break;
		case ArmOp::ASR:
			verify(scratch_reg != r0);
			MRS(r0, 0);
			CMP(shift_reg, 32);
			verify(scratch_reg != r1);
			SBFX(r1, scratch_reg, 31, 1);
			ASR(scratch_reg, scratch_reg, shift_reg);
			MOV(scratch_reg, r1, GE);		// ASR by 32 or more gives 0 or -1 depending on operand sign
			MSR(0, 8, r0);
			break;
		case ArmOp::ROR:
			ROR(scratch_reg, scratch_reg, shift_reg);
			break;
		default:
			die("Invalid shift");
			break;
		}
		return scratch_reg;
	}
	else
	{
		// Shift by immediate
		// FIXME this case can be handled directly if op2 and DataProcOp
//		if (arg.shift_type != ArmOp::ROR && arg.shift_value != 0 && !logical_op_set_flags)
//		{
//			rv = Operand(rm, (Shift)arg.shift_type, arg.shift_value);
//		}
//		else
		if (arg.shift_value == 0)
		{
			if (arg.shift_type == ArmOp::LSL)
			{
				return scratch_reg;		// LSL 0 is a no-op
			}
			else
			{
				// Shift by 32
				if (logical_op_set_flags)
					set_carry_bit = true;
				if (arg.shift_type == ArmOp::LSR)
				{
					if (logical_op_set_flags)
						UBFX(r12, scratch_reg, 31, 1);				// w14 = rm[31]
					MOV32(scratch_reg, 0);					// scratch = 0
				}
				else if (arg.shift_type == ArmOp::ASR)
				{
					if (logical_op_set_flags)
						UBFX(r12, scratch_reg, 31, 1);				// w14 = rm[31]
					SBFX(scratch_reg, scratch_reg, 31, 1);			// scratch = rm < 0 ? -1 : 0
				}
				else if (arg.shift_type == ArmOp::ROR)
				{
					// RRX
					MOV32(r12, 0);
					MOV32(r12, 1, CS);							// w14 = C
					if (logical_op_set_flags)
					{
						verify(scratch_reg != r1);
						MOV(r1, scratch_reg);						// save scratch_reg
					}
					MOV(scratch_reg, scratch_reg, S_LSR, 1);	// scratch = rm >> 1
					BFI(scratch_reg, r12, 31, 1);			// scratch[31] = C
					if (logical_op_set_flags)
						UBFX(r12, r1, 0, 1);				// w14 = rm[0] (new C)
				}
				else
					die("Invalid shift");
				return scratch_reg;
			}
		}
		else
		{
			// Carry must be preserved or Ror shift
			if (logical_op_set_flags)
				set_carry_bit = true;
			if (arg.shift_type == ArmOp::LSL)
			{
				UBFX(r12, scratch_reg, 32 - arg.shift_value, 1);		// w14 = rm[lsb]
				LSL(scratch_reg, scratch_reg, arg.shift_value);		// scratch <<= shift
			}
			else
			{
				if (logical_op_set_flags)
					UBFX(r12, scratch_reg, arg.shift_value - 1, 1);	// w14 = rm[msb]

				if (arg.shift_type == ArmOp::LSR)
					LSR(scratch_reg, scratch_reg, arg.shift_value);	// scratch >>= shift
				else if (arg.shift_type == ArmOp::ASR)
					ASR(scratch_reg, scratch_reg, arg.shift_value);
				else if (arg.shift_type == ArmOp::ROR)
					ROR(scratch_reg, scratch_reg, arg.shift_value);
				else
					die("Invalid shift");
			}
			return scratch_reg;
		}
	}
}

template <void (*OpImmediate)(eReg rd, eReg rn, s32 imm8, bool S, ConditionCode cc),
		void (*OpShiftImm)(eReg rd, eReg rn, eReg rm, ShiftOp Shift, u32 ImmShift, bool S, ConditionCode cc),
		void (*OpShiftReg)(eReg rd, eReg rn, eReg rm, ShiftOp Shift, eReg shift_reg, bool S, ConditionCode cc)>
void emit3ArgOp(const ArmOp& op)
{
	eReg rn;
	const ArmOp::Operand *op2;
	if (op.op_type != ArmOp::MOV && op.op_type != ArmOp::MVN)
	{
		rn = getOperand(op.arg[0], r2);
		op2 = &op.arg[1];
	}
	else
		op2 = &op.arg[0];

	eReg rd = regalloc->map(op.rd.getReg().armreg);

	eReg rm;
	if (op2->isImmediate())
	{
		if (is_i8r4(op2->getImmediate()) && op2->shift_imm)
		{
			OpImmediate(rd, rn, op2->getImmediate(), set_flags, CC_AL);
			return;
		}
		MOV32(r0, op2->getImmediate());
		rm = r0;
	}
	else if (op2->isReg())
		rm = regalloc->map(op2->getReg().armreg);

	if (op2->shift_imm)
		OpShiftImm(rd, rn, rm, (ShiftOp)op2->shift_type, op2->shift_value, set_flags, CC_AL);
	else
	{
		// Shift by reg
		eReg shift_reg = regalloc->map(op2->shift_reg.armreg);
		OpShiftReg(rd, rn, rm, (ShiftOp)op2->shift_type, shift_reg, set_flags, CC_AL);
	}
}

template <void (*OpImmediate)(eReg rd, s32 imm8, bool S, ConditionCode cc),
		void (*OpShiftImm)(eReg rd, eReg rm, ShiftOp Shift, u32 ImmShift, bool S, ConditionCode cc),
		void (*OpShiftReg)(eReg rd, eReg rm, ShiftOp Shift, eReg shift_reg, bool S, ConditionCode cc)>
void emit2ArgOp(const ArmOp& op)
{
	// Used for rd (MOV, MVN) and rn (CMP, TST, ...)
	eReg rd;
	const ArmOp::Operand *op2;
	if (op.op_type != ArmOp::MOV && op.op_type != ArmOp::MVN)
	{
		rd = getOperand(op.arg[0], r2);
		op2 = &op.arg[1];
	}
	else {
		op2 = &op.arg[0];
		rd = regalloc->map(op.rd.getReg().armreg);
	}
	eReg rm;
	if (op2->isImmediate())
	{
		if (is_i8r4(op2->getImmediate()) && op2->shift_imm)
		{
			OpImmediate(rd, op2->getImmediate(), set_flags, CC_AL);
			return;
		}
		MOV32(r0, op2->getImmediate());
		rm = r0;
	}
	else if (op2->isReg())
		rm = regalloc->map(op2->getReg().armreg);

	if (op2->shift_imm)
		OpShiftImm(rd, rm, (ShiftOp)op2->shift_type, op2->shift_value, set_flags, CC_AL);
	else
	{
		// Shift by reg
		eReg shift_reg = regalloc->map(op2->shift_reg.armreg);
		OpShiftReg(rd, rm, (ShiftOp)op2->shift_type, shift_reg, set_flags, CC_AL);
	}
}

static void emitDataProcOp(const ArmOp& op)
{
	switch (op.op_type)
	{
	case ArmOp::AND:
		emit3ArgOp<&AND, &AND, &AND>(op);
		break;
	case ArmOp::EOR:
		emit3ArgOp<&EOR, &EOR, &EOR>(op);
		break;
	case ArmOp::SUB:
		emit3ArgOp<&SUB, &SUB, &SUB>(op);
		break;
	case ArmOp::RSB:
		emit3ArgOp<&RSB, &RSB, &RSB>(op);
		break;
	case ArmOp::ADD:
		emit3ArgOp<&ADD, &ADD, &ADD>(op);
		break;
	case ArmOp::ORR:
		emit3ArgOp<&ORR, &ORR, &ORR>(op);
		break;
	case ArmOp::BIC:
		emit3ArgOp<&BIC, &BIC, &BIC>(op);
		break;
	case ArmOp::ADC:
		emit3ArgOp<&ADC, &ADC, &ADC>(op);
		break;
	case ArmOp::SBC:
		emit3ArgOp<&SBC, &SBC, &SBC>(op);
		break;
	case ArmOp::RSC:
		emit3ArgOp<&RSC, &RSC, &RSC>(op);
		break;
	case ArmOp::TST:
		emit2ArgOp<&TST, &TST, &TST>(op);
		break;
	case ArmOp::TEQ:
		emit2ArgOp<&TEQ, &TEQ, &TEQ>(op);
		break;
	case ArmOp::CMP:
		emit2ArgOp<&CMP, &CMP, &CMP>(op);
		break;
	case ArmOp::CMN:
		emit2ArgOp<&CMN, &CMN, &CMN>(op);
		break;
	case ArmOp::MOV:
		emit2ArgOp<&MOV, &MOV, &MOV>(op);
		break;
	case ArmOp::MVN:
		emit2ArgOp<&MVN, &MVN, &MVN>(op);
		break;
	default:
		die("invalid op");
		break;
	}

	if (set_carry_bit)
	{
		MRS(r0, 0);
		BFI(r0, r12, 29, 1);		// C is bit 29 in NZCV
		MSR(0, 8, r0);
	}
}

static void emitMemOp(const ArmOp& op)
{
	eReg addr_reg = getOperand(op.arg[0], r2);
	if (op.pre_index)
	{
		const ArmOp::Operand& offset = op.arg[1];
		if (offset.isReg())
		{
			if (addr_reg != r2)
				MOV(r2, addr_reg);
			addr_reg = r2;
			eReg offset_reg = getOperand(offset, r3);
			if (op.add_offset)
				ADD(addr_reg, addr_reg, offset_reg);
			else
				SUB(addr_reg, addr_reg, offset_reg);
		}
		else if (offset.isImmediate() && offset.getImmediate() != 0)
		{
			if (addr_reg != r2)
				MOV(r2, addr_reg);
			addr_reg = r2;
			if (is_i8r4(offset.getImmediate()))
			{
				if (op.add_offset)
					ADD(addr_reg, addr_reg, offset.getImmediate());
				else
					SUB(addr_reg, addr_reg, offset.getImmediate());
			}
			else
			{
				MOV32(r0, offset.getImmediate());
				if (op.add_offset)
					ADD(addr_reg, addr_reg, r0);
				else
					SUB(addr_reg, addr_reg, r0);
			}
		}
	}
	MOV(r0, addr_reg);
	if (op.op_type == ArmOp::STR)
	{
		if (op.arg[2].isImmediate())
			MOV(r1, op.arg[2].getImmediate());
		else
			MOV(r1, regalloc->map(op.arg[2].getReg().armreg));
	}

	CALL((u32)arm7rec_getMemOp(op.op_type == ArmOp::LDR, op.byte_xfer));

	if (op.op_type == ArmOp::LDR)
		MOV(regalloc->map(op.rd.getReg().armreg), r0);

}

static void emitBranch(const ArmOp& op)
{
	if (op.arg[0].isImmediate())
		MOV32(r0, op.arg[0].getImmediate());
	else
	{
		MOV(r0, regalloc->map(op.arg[0].getReg().armreg));
		BIC(r0, r0, 3);
	}
	storeReg(r0, R15_ARM_NEXT);
}

static void emitMRS(const ArmOp& op)
{
	CALL((u32)CPUUpdateCPSR);

	if (op.spsr)
		loadReg(regalloc->map(op.rd.getReg().armreg), RN_SPSR);
	else
		loadReg(regalloc->map(op.rd.getReg().armreg), RN_CPSR);
}

static void emitMSR(const ArmOp& op)
{
	if (op.arg[0].isImmediate())
		MOV32(r0, op.arg[0].getImmediate());
	else
		MOV(r0, regalloc->map(op.arg[0].getReg().armreg));
	if (op.spsr)
		CALL((u32)MSR_do<1>);
	else
		CALL((u32)MSR_do<0>);
}

static void emitFallback(const ArmOp& op)
{
	//Call interpreter
	MOV32(r0, op.arg[0].getImmediate());
	CALL((u32)arm_single_op);
	SUB(r5, r5, r0, false);
}

void arm7backend_compile(const std::vector<ArmOp> block_ops, u32 cycles)
{
	regalloc = new Arm32ArmRegAlloc(block_ops);
	void *codestart = icPtr;

	for (u32 i = 0; i < block_ops.size(); i++)
	{
		const ArmOp& op = block_ops[i];
		DEBUG_LOG(AICA_ARM, "-> %s", op.toString().c_str());

		set_flags = op.flags & ArmOp::OP_SETS_FLAGS;
		logical_op_set_flags = op.isLogicalOp() && set_flags;
		set_carry_bit = false;

		u32 *condPos = nullptr;

		if (op.flags & (ArmOp::OP_READS_FLAGS|ArmOp::OP_SETS_FLAGS))	// TODO OP_READS_FLAGS needed?
			loadFlags();

		if (op.op_type != ArmOp::FALLBACK)
			condPos = startConditional(op.condition);

		regalloc->load(i);

		if (op.op_type <= ArmOp::MVN)
			// data processing op
			emitDataProcOp(op);
		else if (op.op_type <= ArmOp::STR)
			// memory load/store
			emitMemOp(op);
		else if (op.op_type <= ArmOp::BL)
			// branch
			emitBranch(op);
		else if (op.op_type == ArmOp::MRS)
			emitMRS(op);
		else if (op.op_type == ArmOp::MSR)
			emitMSR(op);
		else if (op.op_type == ArmOp::FALLBACK)
			emitFallback(op);
		else
			die("invalid");

		if (set_flags)
			storeFlags();

		regalloc->store(i);

		endConditional(condPos);
	}

	if (is_i8r4(cycles))
		SUB(r5, r5, cycles, true);
	else
	{
		u32 togo = cycles;
		while(ARMImmid8r4_enc(togo) == -1)
		{
			SUB(r5, r5, 256);
			togo -= 256;
		}
		SUB(r5, r5, togo, true);
	}
	JUMP((u32)&arm_exit, CC_MI);	//statically predicted as not taken
	JUMP((u32)&arm_dispatch);

	vmem_platform_flush_cache(codestart, (u8*)icPtr - 1, codestart, (u8*)icPtr - 1);

	delete regalloc;
	regalloc = nullptr;
}

#endif // HOST_CPU == CPU_ARM && FEAT_AREC != DYNAREC_NONE
