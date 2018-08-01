#ifdef HAVE_GDBSERVER
#include <set>

#include "gdb/gdb_server.h"
#define GDB_SERVER_IMPL
#include "gdb/gdb_server.h"
#endif
#include "types.h"
#include "sh4_mem.h"
#include "sh4_debug.h"
#include "sh4_interrupts.h"
#include "sh4_if.h"

#ifdef HAVE_GDBSERVER

gdb_server_t *sv;
bool debugger_attached  = false;
std::set<u32> breakpoints;
std::set<u32> read_watchpoints;
std::set<u32> write_watchpoints;
static u32 saved_vector_pc = -1;
static u32 rwatchpoint_hit = -1;
static u32 wwatchpoint_hit = -1;
static bool ignore_watchpoints = false;

static void debugger_gdb_server_resume(void *data);

static void debugger_gdb_server_detach(void *data)
{
	printf("GDB server: detach\n");

	debugger_attached  = false;
	debugger_gdb_server_resume(data);
}

static void debugger_gdb_server_stop(void *data)
{
	printf("GDB server: stop\n");

	debugger_attached  = true;
	sh4_cpu.Stop();
}

static void debugger_gdb_server_resume(void *data)
{
	printf("GDB server: resume\n");

	if (saved_vector_pc != -1)
	{
		Sh4cntx.pc = saved_vector_pc;
		saved_vector_pc = -1;
	}
	if (ignore_watchpoints)
	{
		sh4_cpu.Step();
		ignore_watchpoints = false;
	}
	sh4rcb.cntx.CpuRunning = 1;
}

static void debugger_gdb_server_step(void *data)
{
	printf("GDB server: step\n");

	if (saved_vector_pc != -1)
	{
		Sh4cntx.pc = saved_vector_pc;
		saved_vector_pc = -1;
	}
	sh4_cpu.Step();

	if (ignore_watchpoints)
		ignore_watchpoints = false;

	// No need to trap if the step raised an exception
	if (saved_vector_pc == -1)
		debugger_trap(0x160);
}

static void debugger_gdb_server_add_bp(void *data, int type, intmax_t addr)
{
	printf("GDB server: add breakpoint %d %lx\n", type, addr);

	// Memory / hardware breakpoints
	if (type == 0 || type == 1)
	{
		breakpoints.insert((u32)addr);
	}
	else
	{
		// Write or access watchpoints
		if (type == 2 || type == 4)
			write_watchpoints.insert((u32)addr);

		// Read or access watchpoints
		if (type == 3 || type == 4)
			read_watchpoints.insert((u32)addr);
	}
}

static void debugger_gdb_server_rem_bp(void *data, int type, intmax_t addr)
{
	printf("GDB server: remove breakpoint %d %lx\n", type, addr);

	// Memory / hardware breakpoints
	if (type == 0 || type == 1)
	{
		breakpoints.erase((u32)addr);
	}
	else
	{
		// Write or access watchpoints
		if (type == 2 || type == 4)
			write_watchpoints.erase((u32)addr);

		// Read or access watchpoints
		if (type == 3 || type == 4)
			read_watchpoints.erase((u32)addr);
	}
}

static void debugger_gdb_server_read_mem(void *data, intmax_t addr,
                                         uint8_t *buffer, int size)
{
	if ((addr & 7) == 0 && (size & 7) == 0)
	{
		size >>= 3;
		while (size-- > 0)
		{
			*(u64 *)buffer = ReadMem64(addr);
			buffer += 8;
			addr += 8;
		}
	}
	else if ((addr & 3) == 0 && (size & 3) == 0)
	{
		size >>= 2;
		while (size-- > 0)
		{
			*(u32 *)buffer = ReadMem32(addr);
			buffer += 4;
			addr += 4;
		}
	}
	else if ((addr & 1) == 0 && (size & 1) == 0)
	{
		size >>= 1;
		while (size-- > 0)
		{
			*(u16 *)buffer = ReadMem16(addr);
			buffer += 2;
			addr += 2;
		}
	}
	else
	{
		while (size-- > 0)
		{
			*buffer++ = ReadMem8(addr++);
		}
	}
}

static void debugger_gdb_server_read_reg(void *data, int n, intmax_t *value,
                                         int *size)
{
	*size = 4;
	if (n < 16)
		*value = Sh4cntx.r[n];
	else if (n == 16)
		*value = Sh4cntx.pc;
	else if (n == 17)
		*value = Sh4cntx.pr;
	else if (n == 18)
		*value = Sh4cntx.gbr;
	else if (n == 19)
		*value = Sh4cntx.vbr;
	else if (n == 20)
		*value = Sh4cntx.mac.h;
	else if (n == 21)
		*value = Sh4cntx.mac.l;
	else if (n == 22)
		*value = Sh4cntx.sr.GetFull();
	else if (n == 23)
		*value = 0;	// TICKS
	else if (n == 24)
		*value = 0;	// STALLS
	else if (n == 25)
		*value = 0; // CYCLES;
	else if (n == 26)
		*value = 0; // INSTS;
	else if (n == 27)
		*value = 0; // PLR;
	else
		*value = 0;
}
#endif

int debugger_init()
{
#ifdef HAVE_GDBSERVER

  /* create the gdb server */
  gdb_target_t target;
  target.ctx = NULL;
  target.endian = GDB_LITTLE_ENDIAN;
  target.num_regs = 28;
  target.detach = &debugger_gdb_server_detach;
  target.stop = &debugger_gdb_server_stop;
  target.resume = &debugger_gdb_server_resume;
  target.step = &debugger_gdb_server_step;
  target.add_bp = &debugger_gdb_server_add_bp;
  target.rem_bp = &debugger_gdb_server_rem_bp;
  target.read_reg = &debugger_gdb_server_read_reg;
  target.read_mem = &debugger_gdb_server_read_mem;

  sv = gdb_server_create(&target, 24690);
  if (!sv) {
    fprintf(stderr, "GDB: failed to create GDB server\n");
    return 0;
  }
  printf("GDB: Debug server created\n");
#endif

  return 1;
}

bool debugger_trap(u32 exception_code)
{
#ifdef HAVE_GDBSERVER
	if (!sv || !debugger_attached)
		return false;
	int signal = -1;
	const char *info = NULL;
	switch (exception_code)
	{
	// Instruction TLB miss
	case 0x40:
		info = "Instruction TLB miss";
		signal = GDB_SIGNAL_SEGV;
		break;
	// Data TLB miss (write)
	case 0x60:
		info = "Data TLB miss (write)";
		signal = GDB_SIGNAL_SEGV;
		break;
	// Initial page write exception
	case 0x80:
		info = "Initial page write exception";
		signal = GDB_SIGNAL_SEGV;
		break;
	// Data TLB protection (read)
	case 0xa0:
		info = "Data TLB protection (read)";
		signal = GDB_SIGNAL_SEGV;
		break;
	// Data TLB protection (write)
	case 0xc0:
		info = "Data TLB protection (write)";
		signal = GDB_SIGNAL_SEGV;
		break;
	// Data address error (read)
	case 0xe0:
		info = "Data address error (read)";
		signal = GDB_SIGNAL_SEGV;
		break;
	// Data address error (write)
	case 0x100:
		info = "Data address error (write)";
		signal = GDB_SIGNAL_SEGV;
		break;

	// FPU exception
	case 0x120:
		info = "FPU exception";
		signal = GDB_SIGNAL_FPE;
		break;

	// TRAPA
	case 0x160:
		signal = GDB_SIGNAL_TRAP;
		break;

	// Illegal instruction
	case 0x180:
		info = "Illegal instruction";
		signal = GDB_SIGNAL_ILL;
		break;
	// Slot illegal instruction
	case 0x1a0:
		info = "Slot illegal instruction";
		signal = GDB_SIGNAL_ILL;
		break;

	// User break
	case 0x1e0:
		info = "User break";
		signal = GDB_SIGNAL_INT;
		break;

	default:
		return false;
	}

	if (signal != GDB_SIGNAL_TRAP)
	{
		saved_vector_pc = Sh4cntx.pc;
		Sh4cntx.pc = Sh4cntx.spc;
	}

	sh4_cpu.Stop();

	if (signal == GDB_SIGNAL_TRAP)
	{
		if (wwatchpoint_hit != -1)
		{
			printf("GDB: write watchpoint hit @ %X\n", wwatchpoint_hit);
			gdb_server_watchpoint_hit(sv, GDB_SIGNAL_TRAP, "", wwatchpoint_hit);
			wwatchpoint_hit = -1;
			ignore_watchpoints = true;
			return true;
		}
		else if (rwatchpoint_hit != -1)
		{
			printf("GDB: read watchpoint hit @ %X\n", wwatchpoint_hit);
			gdb_server_watchpoint_hit(sv, GDB_SIGNAL_TRAP, "r", rwatchpoint_hit);
			rwatchpoint_hit = -1;
			ignore_watchpoints = true;
			return true;
		}
		printf("GDB: breakpoint hit @ %X\n", Sh4cntx.pc);
	}
	else
		printf("GDB: signal received %d\n", signal);

	gdb_server_interrupt(sv, signal, info);

	return true;
#else
	return false;
#endif
}

void debugger_tick()
{
#ifdef HAVE_GDBSERVER
	if (sv)
		gdb_server_pump(sv);
#endif
}

void debugger_destroy()
{
#ifdef HAVE_GDBSERVER
	if (!sv)
		return;
	if (debugger_attached)
	{
		gdb_server_process_ended(sv, 0);
		gdb_server_pump(sv);
	}
	gdb_server_destroy(sv);
	sv = NULL;

	printf("GDB: Debug server destroyed\n");

#endif
}

#ifdef HAVE_GDBSERVER
bool is_debugging()
{
	return debugger_attached;
}
#endif

bool check_breakpoint(u32 pc)
{
#ifdef HAVE_GDBSERVER
	if (breakpoints.find(pc) != breakpoints.end())
	{
		RaiseException(0x160, 0x100, 0);
		// Not reached
		return true;
	}
#endif
	return false;
}

bool check_read_watchpoint(u32 addr)
{
#ifdef HAVE_GDBSERVER
	if (!ignore_watchpoints && read_watchpoints.find(addr) != read_watchpoints.end())
	{
		rwatchpoint_hit = addr;
		Sh4cntx.pc -= 2;		// Redo the instruction on continue
		RaiseException(0x160, 0x100);
		// Not reached
		return true;
	}
#endif
	return false;
}

bool check_write_watchpoint(u32 addr)
{
#ifdef HAVE_GDBSERVER
	if (!ignore_watchpoints && write_watchpoints.find(addr) != write_watchpoints.end())
	{
		wwatchpoint_hit = addr;
		Sh4cntx.pc -= 2;
		RaiseException(0x160, 0x100);
		// Not reached
		return true;
	}
#endif
	return false;
}

