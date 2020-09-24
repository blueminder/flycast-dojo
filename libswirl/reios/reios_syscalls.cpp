#include "license/bsd"
#include "reios_syscalls.h"
#include "hw/mem/_vmem.h"
#include "hw/sh4/sh4_mem.h"

//https://github.com/reicast/reicast-emulator/blob/37dba034a9fd0a03d8ebfd07c3c7ec85b76f0eb7/libswirl/hw/sh4/interpr/sh4_opcodes.cpp#L872

reios_syscall_mgr_c g_syscalls_mgr;

reios_syscall_mgr_c::reios_syscall_mgr_c() {

}

reios_syscall_mgr_c::~reios_syscall_mgr_c() {

}

bool reios_syscall_mgr_c::register_syscall(const std::string& name,reios_hook_fp* native_func,const uint32_t addr,const uint32_t syscall,const bool enabled) {
	lock();
	if (grab_syscall(name) != m_scs.end()) {
		unlock();
		return false;
	}
	m_scs_rev.insert({addr,name});
	m_scs.insert({ name,(reios_syscall_cfg_t(addr,syscall,enabled,native_func)) });
	unlock();
	return true;
}

/*
bool reios_syscall_mgr_c::register_syscall(std::string&& name, reios_hook_fp* native_func, const uint32_t addr, const bool enabled, const bool init_type) {
	lock();
	if (grab_syscall(name) != m_scs.end()) {
		unlock();
		return false;
	}
	m_scs.insert({ std::move(name),std::move(reios_syscall_cfg_t(addr,enabled,native_func,init_type)) });
	unlock();
	return true;
}*/

bool reios_syscall_mgr_c::activate_syscall(const std::string& name, const bool active) {
	lock();
	auto cfg = grab_syscall(name);
	if (cfg == m_scs.end()) {
		unlock();
		return false;
	}
	cfg->second.enabled = active;
	unlock();
	return true;
}

bool reios_syscall_mgr_c::is_activated(const std::string& name) {
	auto cfg = grab_syscall(name);
	if (cfg == m_scs.end()) {
		unlock();
		return false;
	}
	bool res = cfg->second.enabled;
	unlock();
	return res;
}

//Syscalls begin!!

static bool g_init_done = false;


constexpr auto lock_val = 0x8C001994;

void try_lock_gd_mutex() {
	//printf("From 0x%x to 0x%x\n", Sh4cntx.pc, Sh4cntx.pr);
	u8 old = ReadMem8(lock_val);
	WriteMem8(lock_val, old | 0x80);

	Sh4cntx.r[0] = old ? 1 : 0;

	Sh4cntx.pc = Sh4cntx.pr;
}

void release_lock_gd_mutex() {
	Sh4cntx.r[0] = lock_val;
	WriteMem8(lock_val, 0);
	Sh4cntx.pc = Sh4cntx.pr;
}

void gd_check_cmd_easy_mtx_release() {
	//8C003162
	release_lock_gd_mutex();
	Sh4cntx.r[0] = -1;
	Sh4cntx.pc = 0x8C00316A;
}

void get_reg_base_addr() {
	Sh4cntx.r[0] = 0xA05F7000;
	Sh4cntx.pc = Sh4cntx.pr;
}

void mutex_op(const int32_t stat) { // Not hooked
	WriteMem8(lock_val, stat);
}

inline void stack_push(const uint32_t val) {
	Sh4cntx.r[15] -= 4;
	WriteMem32(Sh4cntx.r[15] , val);
}

inline const uint32_t stack_pop() {
	const uint32_t v = ReadMem32(Sh4cntx.r[15]);
	Sh4cntx.r[15] += 4;
	return v;
}


inline void push_reg_to_mem(const uint32_t data,const uint32_t reg_index) {
	Sh4cntx.r[reg_index] -= 4;
	WriteMem32( ReadMem32( Sh4cntx.r[reg_index]) , data );
}

inline void push_ref_reg_to_mem(const uint32_t data,uint32_t& reg) {
	reg -= 4;
	WriteMem32((reg),data );
}

inline const uint32_t pop_mem_from_reg(uint32_t reg_index) {
	const uint32_t v = ReadMem32(Sh4cntx.r[reg_index]);
	Sh4cntx.r[reg_index] += 4;
	return v;
}

inline const uint32_t pop_mem_from_ref_reg(uint32_t& reg) {
	const uint32_t v = ReadMem32(reg);
	reg += 4;
	return v;

}
bool reios_syscalls_shutdown() {
	return true;
}

#if 1
#define r Sh4cntx.r
 
void exit_from_gd_thread() { //8C0018C0

	r[0] = 0x8C00198C;
	r[2] = ReadMem32(r[0]);
	r[2] -= r[15];
	r[2] >>= 2;
	r[0] = r[2];
	const bool t_result = (0 == r[0]);
	
	//Sh4cntx.sr.T = t_result;
	r[0] = 0x8C001990;
	r[0] = ReadMem32(r[0]);
	r[1] = r[2];

	push_ref_reg_to_mem(Sh4cntx.pr, r[0]);  
	push_ref_reg_to_mem(Sh4cntx.mac.h, r[0]);  
	push_ref_reg_to_mem(Sh4cntx.mac.l, r[0]);  
	push_ref_reg_to_mem(r[14], r[0]);
	push_ref_reg_to_mem(r[13], r[0]);
	push_ref_reg_to_mem(r[12], r[0]);
	push_ref_reg_to_mem(r[11], r[0]);
	push_ref_reg_to_mem(r[10], r[0]);
	push_ref_reg_to_mem(r[9], r[0]);
	push_ref_reg_to_mem(r[8], r[0]);

	if (!t_result) {
		while (r[2] > 0) {
			--r[2];

			r[3] = stack_pop();
			push_ref_reg_to_mem(r[3], r[0]);
		}
	}

	push_ref_reg_to_mem(r[1], r[0]);
	r[2] = r[0];
	r[0] = 0x8C001990;
	 
	WriteMem32(r[0], r[2]);

	r[8] = stack_pop();
	r[9] = stack_pop();
	r[10] = stack_pop();
	r[11] = stack_pop();
	r[12] = stack_pop();
	r[13] = stack_pop();
	r[14] = stack_pop();

	Sh4cntx.mac.l = stack_pop();
	Sh4cntx.mac.h = stack_pop();
	Sh4cntx.pr = stack_pop();
	r[0] = 0x8C001994;
	r[2] = 0;
	WriteMem32(r[0], r[2]);
	Sh4cntx.pc = Sh4cntx.pr;
}

void gd_common_subroutine() { // 8C002774
	auto eff_pc = (Sh4cntx.pc - 2);

	if (eff_pc == 0x8C0027A8) {
		goto loc_8C0027A8;
	}
	else {
		verify(eff_pc == 0x8C002774);
	}

	stack_push(r[14]);
	stack_push(r[13]);
	stack_push(r[12]);
	stack_push(r[11]);
	stack_push(r[10]);
	stack_push(Sh4cntx.pr);

	r[14] = r[4];
	r[0] = 0x8C002788;
	r[10] = 0xFFFFF138;
	r[10] += r[0];
	r[12] = 0x88;
	r[13] = 0;
	r[11] = 2;

	Sh4cntx.pc = 0x8C00278E;

	for (;;) {
		r[0] = 0xa0;
		r[2] = ReadMem32(r[0] + r[14]);
		r[2] += 0x18;
		r[3] =  (u8) ReadMem8(r[2] );

		if ((r[12] & r[3]) == 0) {
			r[0] = 0xa8;
			WriteMem32(r[0] + r[14], r[13]);

			break;
		}
		printf("Untested code never reach here\n");
		verify(0);
		Sh4cntx.pr = 0x8C0027A8;
		Sh4cntx.pc = 0x8c0018c0;
		r[0] = 0xa8;
		WriteMem32(r[0] + r[14], r[11]);
		return;

	loc_8C0027A8:
		__noop;
	}

	Sh4cntx.pr = stack_pop();
	r[10] = stack_pop();
	r[11] = stack_pop();
	r[12] = stack_pop();
	r[13] = stack_pop();
	r[14] = stack_pop();
	Sh4cntx.pc = Sh4cntx.pr;
}
 

void gd_popular_function() { //0x8C0027BA

	auto curr_pc = Sh4cntx.pc - 2;

	/*
	RAM:8C0027BA                 mov.l   r14, @-r15 OK 
RAM:8C0027BC                 mov.l   r13, @-r15 OK
RAM:8C0027BE                 mov     r4, r13 OK
RAM:8C0027C0                 mova    h'8C0027CC, r0 ok
RAM:8C0027C2                 mov.l   r12, @-r15  OK 
RAM:8C0027C4                 mov.l   r11, @-r15 OK 
RAM:8C0027C6                 mov.l   r10, @-r15 OK 
RAM:8C0027C8                 sts.l   pr, @-r15 OK 
RAM:8C0027CA                 mov.l   #h'FFFFF0F4, r10 OK 
RAM:8C0027CC                 mov.w   #h'FFFFF904, r11  ok
RAM:8C0027CE                 mov     #0, r12 OK
RAM:8C0027D0                 add     r0, r10 OK
RAM:8C0027D2                 mov     #1, r14 OK
*/


	printf("gd_popular_function\n");
 
	stack_push(r[14]);
	stack_push(r[13]);
	stack_push(r[12]);
	stack_push(r[11]);
	stack_push(r[10]);
	stack_push(Sh4cntx.pr);
	
	r[13] = r[4];
	r[0] = 0x8C0027CC;
	r[10] = 0xFFFFF0F4;
	r[11] = 0xFFFFF904;
	r[12] = 0;
	r[10] += r[0];
	r[14] = 1;

	if (curr_pc != 0x8C0027D6) {


		//gd_common_subroutine();
		if (curr_pc < 0x8C0027DC) {
			Sh4cntx.pc = 0x8C0027D6;
			return;
		} else {
			gd_common_subroutine();
			Sh4cntx.pc = 0x8C0027DC;
			return;
		}
	}
	else {
		if (curr_pc != 0x8C0027D6) {
			die("PC != 0x8C0027D6\n");
		}
		//printf("Will keep going\n");
	}
 

	/*
	RAM:8C0027D4 loc_8C0027D4:                           
RAM:8C0027D4                 mov.w   #h'A8, r0    OK
RAM:8C0027D6                 jsr     @r10         OK    ; 0x8c0018c0
RAM:8C0027D8                 mov.l   r14, @(r0,r13) OK
RAM:8C0027DA                 mov.w   #h'A0, r0 OK
RAM:8C0027DC                 mov.l   @(r0,r13), r3 OK 
RAM:8C0027DE                 add     r11, r3 OK
RAM:8C0027E0                 mov.l   @r3, r2 OK
RAM:8C0027E2                 tst     r14, r2 OK
RAM:8C0027E4                 bt      loc_8C0027D4 OK
RAM:8C0027E6                 mov.w   #h'A8, r0
RAM:8C0027E8                 mov.l   r12, @(r0,r13) ok
RAM:8C0027EA                 lds.l   @r15+, pr
RAM:8C0027EC                 mov.l   @r15+, r10
RAM:8C0027EE                 mov.l   @r15+, r11
RAM:8C0027F0                 mov.l   @r15+, r12
RAM:8C0027F2                 mov.l   @r15+, r13
RAM:8C0027F4                 rts
RAM:8C0027F6                 mov.l   @r15+, r14
	*/

	do {
		r[0] = 0xa8;
		//JSR HERE  8C0027D6 / 0x8c0018c0
		WriteMem32(r[0] + r[13], r[14]);
		r[0] = 0xa0;
		r[3] = ReadMem32(r[0] + r[13]);

		r[3] += r[11];
		r[2] = ReadMem32(r[3]);

		r[14] = (r[14] & r[2]) == 0;
	} while (r[14]);

	r[0] = 0xa8;
	WriteMem32(r[0] + r[13], r[12]);
	Sh4cntx.pr = stack_pop();
	r[10] = stack_pop();
	r[11] = stack_pop();
	r[12] = stack_pop();
	r[13] = stack_pop();
	r[14] = stack_pop();

	Sh4cntx.pc = Sh4cntx.pr;
}


void odd_wait_loop_function() { //0x8C000772
	/*
	RAM:8C000772 odd_wait_loop_function:                  
RAM:8C000772                                         
RAM:8C000772                 mov.l   @(h'28,gbr), r0
RAM:8C000774                 mov     r0, r4
RAM:8C000776
RAM:8C000776 bios_boot_idle_loop:                    
RAM:8C000776                 mov.l   @(h'28,gbr), r0
RAM:8C000778                 cmp/eq  r0, r4
RAM:8C00077A                 bt      bios_boot_idle_loop
RAM:8C00077C                 rts
RAM:8C00077E                 nop
	*/

	printf("odd_wait_loop_function\n");
	r[0] = ReadMem32(Sh4cntx.gbr + (u32)0x28);
	r[4] = r[0];

	while ((r[0] = ReadMem32(Sh4cntx.gbr + (u32)0x28)) == r[4]) {

	}

	Sh4cntx.pc = Sh4cntx.pr;
}

void hook_and_trace_regs() { //8C001CA8

	if (r[6] != 17)
	printf("GDCC r6 = %d\n",r[6]);
	r[6] <<= 2;
	return;
	 
	printf("Get toc2\n");

	for (uint32_t i = 0; i < 32; ++i) {
		printf("r%d  = 0x%x | ", i, r[i]);
		if (i & 7 == 0)
			printf("\n");
	}
	printf("\n");

	/*
		before JUMP

		r0  = 0x0 | r1  = 0x10 | r2  = 0x8c001acc | r3  = 0x8c001aa4 | r4  = 0x8c001348 | r5  = 0x8c0012e8 | 
		r6  = 0x18 | r7  = 0x798 | r8  = 0x8c0011ec | r9  = 0x8c0018c0 | r10  = 0x3 | r11  = 0x6 | r12  = 0x1 | 
		r13  = 0x0 | r14  = 0x8c0012e8 | r15  = 0x8c00f3d0 | r16  = 0x1a0 | r17  = 0x0 | r18  = 0x700000f0 | 
		r19  = 0xffffff0f | r20  = 0xff00001c | r21  = 0x0 | r22  = 0xac00fc00 | r23  = 0x0 | r24  = 0x400 | 
		r25  = 0x400 | r26  = 0x8c000000 | r27  = 0x40000001 | r28  = 0x8c000776 | 
		r29  = 0x8d000000 | r30  = 0x8c000010 | r31  = 0x8c00f400 |

	*/
	/*
	At JUMP
	r0  = 0x8c001204 | r1  = 0x8c001fa8 | r2  = 0x8c001acc | r3  = 0x8c001aa4 | r4  = 0x8c001348 | r5  = 0x8c0012e8 | r6  = 0x8c001264 |
	r7  = 0x798 | r8  = 0x8c0011ec | r9  = 0x8c0018c0 | r10  = 0x3 | r11  = 0x6 | r12  = 0x1 | r13  = 0x0 | r14  = 0x8c0012e8 
	r15  = 0x8c00f3d0 | r16  = 0x1a0 | r17  = 0x0 | r18  = 0x700000f0 | r19  = 0xffffff0f | r20  = 0xff00001c | r21  = 0x0 
    r22  = 0xac00fc00 | r23  = 0x0 | r24  = 0x400 | r25  = 0x400 | r26  = 0x8c000000 | r27  = 0x40000001 | r28  = 0x8c000776 
	r29  = 0x8d000000 | r30  = 0x8c000010 | r31  = 0x8c00f400 |
	*/
 
	/*
	AM:8C0011EC gdcc_call:
RAM:8C0011EC                 mov     #h'30, r1 ; '0'
RAM:8C0011EE                 cmp/hs  r1, r6
RAM:8C0011F0                 bt      locret_8C001200
RAM:8C0011F2                 shll2   r6              ; f_args[2] *= size of dw
RAM:8C0011F4                 mova    h'8C001204, r0
RAM:8C0011F6                 add     r0, r6          ; r6= f_args[2] =+ 0x8c001204
RAM:8C0011F8                 mov.l   @r6, r1         ; r1 = r6[0].l
RAM:8C0011FA                 add     r0, r1
RAM:8C0011FC                 jmp     @r1
RAM:8C0011FE                 nop
	*/
	verify(0);
}


void GDCC_HELPER_FUNC() { //0X8C001118
	//  printf("GDCC_HELPER_FUNC\n");


	/*
RAM:8C001118                 mova    h'8C001180, r0
RAM:8C00111A                 mov.w   #h'1153, r1     ; r0 = 0x8C001180 / r1 = 0x1153
RAM:8C00111C                 sub     r1, r0          ; r0 = 0x8C001180 - 0x1153
RAM:8C00111E                 mov     r0, r2
RAM:8C001120                 mov     #0, r5
RAM:8C001122                 cmp/eq  r5, r4
RAM:8C001124                 bt      loc_8C001142
	*/
	r[0] = 0x8C001180;
	r[1] = 0x1153;
	r[0] -= r[1];
	r[2] = r[0];
	r[5] = 0;
	if (r[5] == r[4])
		goto loc_8C001142;

	/*
RAM:8C001126                 mov     #1, r3
RAM:8C001128                 mov.b   r3, @r2
RAM:8C00112A                 add     #1, r2
RAM:8C00112C                 mov.w   @r2, r1
RAM:8C00112E                 extu.w  r1, r1
RAM:8C001130                 cmp/pl  r1
RAM:8C001132                 bt      loc_8C001138
RAM:8C001134                 rts
RAM:8C001136                 mov     #0, r0
	*/

	r[3] = 1;
	WriteMem8(r[2], r[3]);
	r[2] += 1;
	r[1] = ReadMem16(r[2]);
	r[1] = (uint16_t)r[1];

	if (r[1] > 0)
		goto loc_8C001138;

	r[0] = 0;
	Sh4cntx.pc = Sh4cntx.pr;
	return;

	/*
	RAM:8C001138                 mov     #0, r1
RAM:8C00113A                 mov.b   r1, @r0
RAM:8C00113C                 mov     #h'FFFFFFFF, r0
RAM:8C00113E                 rts
RAM:8C001140                 nop
	*/
loc_8C001138:
	r[1] = 0;
	WriteMem8(r[0], r[1]);
	r[0] = 0xFFFFFFFF;
	Sh4cntx.pc = Sh4cntx.pr;
	return;

	/*
	RAM:8C001142 loc_8C001142:                            
RAM:8C001142                 mov.b   r5, @r2
RAM:8C001144                 rts
RAM:8C001146                 nop
	*/
loc_8C001142:
	WriteMem8(r[2], r[5]);
	Sh4cntx.pc = Sh4cntx.pr;
}

void GDCC_HELPER_FUNC2() { //0x8C0026FE

	//printf("GDCC_HELPER_FUNC2\n");
	/*
RAM:8C0026FE                 mov.w   #h'418, r5
RAM:8C002700                 mov.w   #h'A0, r0
RAM:8C002702                 mov.l   @(r0,r4), r0    ; r0 = *(u32*)(f_args[0] + 0x0a0)
RAM:8C002704                 mov.l   @(r0,r5), r0    ; r0 = *(u32*)(0x0418 + r0)
RAM:8C002706                 cmp/eq  #1, r0
RAM:8C002708                 bf/s    loc_8C002722
	*/

	r[5] = 0x0418;
	r[0] = 0x00a0;
	r[0] = ReadMem32(r[0] + r[4]);
	r[0] = ReadMem32(r[0] + r[5]);
	if ((1 == r[0])) {
		goto loc_8C002722;
	}
	/*
RAM:8C00270A                 mov     #0, r7
RAM:8C00270C                 mov.w   #h'414, r3
RAM:8C00270E                 mov.w   #h'A0, r0
RAM:8C002710                 mov.l   @(r0,r4), r2
RAM:8C002712                 add     r3, r2
RAM:8C002714                 mov.l   r7, @r2
RAM:8C002716                 mov     #1, r6
	*/
	r[7] = 0;
	r[3] = 0x0414;
	r[0] = 0x00a0;
	r[2] = ReadMem32(r[0] + r[4]);
	r[2] += r[3];
	WriteMem32( r[2] , r[7]);
	r[6] = 1;

/*
RAM:8C002718 loc_8C002718:
RAM:8C002718                 mov.l   @(r0,r4), r3
RAM:8C00271A                 add     r5, r3
RAM:8C00271C                 mov.l   @r3, r2
RAM:8C00271E                 tst     r6, r2
RAM:8C002720                 bf      loc_8C002718
*/
loc_8C002718:
	do {
		r[3] = ReadMem32(r[0] + r[4]);
		r[3] += r[5];
		r[2] = ReadMem32(r[3]);
	} while (  !(0 == (r[6] & r[2])));

loc_8C002722:

	/*
	RAM:8C002722                 mov.w   #h'C4, r0
	RAM:8C002724                 mov.l   #h'FFFFE9EC, r3
	RAM:8C002726                 mov.l   r7, @(r0,r4)
	RAM:8C002728                 braf    r3 ; GDCC_HELPER_FUNC
	RAM:8C00272A                 mov     #0, r4
	*/
	r[0] = 0x00c4;
	r[3] = 0xFFFFE9EC;
	WriteMem32(r[0] + r[4], r[7]);
	r[4] = 0;
	Sh4cntx.pc = 0x8C001118; //GDCC_HELPER_FUNC

	//GDCC_HELPER_FUNC();
	
	
}

void GDCC_HELPER_FUNC3() { //0x8C00266C

	//printf("Helper func 3\n");
	/*
	RAM:8C00266C                 mov.l   r14, @-r15
RAM:8C00266E                 mov     r6, r1
RAM:8C002670                 mov.l   r13, @-r15
RAM:8C002672                 mov     r6, r3
RAM:8C002674                 mov.l   #h'FF00, r14
RAM:8C002676                 tst     r7, r7
RAM:8C002678                 mov.l   r11, @-r15
RAM:8C00267A                 and     r14, r3
RAM:8C00267C                 mov.l   #h'FF0000, r11
RAM:8C00267E                 and     r11, r1
RAM:8C002680                 shlr16  r1
RAM:8C002682                 or      r3, r1
RAM:8C002684                 bf/s    loc_8C0026C4
	*/

	stack_push(r[14]);
	stack_push(r[13]);
	stack_push(r[11]);

	r[1] = r[6];
	r[3] = r[6];
	r[14] = 0x0000ff00;
	const bool tst = (r[7] & r[7]) == 0;
	r[3] &= r[14];
	r[11] = 0x00FF0000;
	r[1] &= r[11]  ;
	r[1] >>= 16;
	r[1] |= r[3];

	if (!tst) goto loc_8C0026C4;

	/*
RAM:8C002686                 extu.b  r6, r13
RAM:8C002688                 mov     #h'4C, r0 ; 'L'
RAM:8C00268A                 and     r5, r14
RAM:8C00268C                 mov.w   @(r0,r4), r3
RAM:8C00268E                 add     #h'7C, r0 ; '|'
RAM:8C002690                 mov.w   @(r0,r4), r2
RAM:8C002692                 mov     r1, r0
RAM:8C002694                 add     r3, r2
RAM:8C002696                 mov.w   r2, @r4
RAM:8C002698                 mov.w   r0, @(2,r4)  --
RAM:8C00269A                 mov     r13, r0
RAM:8C00269C                 mov.w   r0, @(4,r4) -- 
RAM:8C00269E                 mov     #0, r0
RAM:8C0026A0                 mov.w   r0, @(6,r4) --
RAM:8C0026A2                 mov     r5, r0
RAM:8C0026A4                 and     r11, r0
RAM:8C0026A6                 shlr16  r0
RAM:8C0026A8                 or      r14, r0
RAM:8C0026AA                 mov.w   r0, @(8,r4)
RAM:8C0026AC                 bra     loc_8C0026F4
RAM:8C0026AE                 extu.b  r5, r0
	*/

	r[13] = (uint8_t)r[6];
	r[0] = 0x4c;
	r[14] &= r[5];
	r[3] = ReadMem16(r[0] + r[4]);
	r[0] += 0x7c;
	r[2] = ReadMem16(r[0] + r[4]);
	r[0] = r[1];
	r[2] += r[3];
	WriteMem16(r[4], r[2]);
	WriteMem16(r[4] + 2, r[0]);
	r[0] = r[13];
	WriteMem16(r[4] + 4, r[0]);
	r[0] = 0;
	WriteMem16(r[4] + 6, r[0]);
	r[0] = r[5];
	r[0] &= r[11];
	r[0] >>= 16;
	r[0] |= r[14];
	WriteMem16(r[4] + 8, r[0]);
	r[0] = (uint8_t)r[5];
	goto loc_8C0026F4;

	/*
RAM:8C0026C4 loc_8C0026C4 : ; CODE XREF : GDCC_HELPER_FUNC3  
RAM : 8C0026C4                 mov     #h'50, r0 ; 'P'
RAM:8C0026C6                 mov.w   @(r0, r4), r3
RAM : 8C0026C8                 add     #h'78, r0 ; 'x'  --OK
RAM:8C0026CA                 mov.w   @(r0, r4), r2 --ok
RAM : 8C0026CC                 add     r3, r2
RAM : 8C0026CE                 mov     r1, r0
RAM : 8C0026D0                 mov.w   r2, @r4
RAM : 8C0026D2                 mov     r5, r3
RAM : 8C0026D4                 mov.w   r0, @(2, r4) --
RAM : 8C0026D6					and r14, r3
RAM : 8C0026D8                 mov     r13, r0
RAM : 8C0026DA                 shlr8   r3  --------------
RAM : 8C0026DC                 mov.w   r0, @(4, r4) --------
RAM : 8C0026DE				and r7, r14
RAM : 8C0026E0                 extu.b  r5, r0
RAM : 8C0026E2                 shll8   r0  ---
RAM : 8C0026E4				or r3, r0
RAM : 8C0026E6                 mov.w   r0, @(6, r4)  --
RAM : 8C0026E8                 mov     r7, r0
RAM : 8C0026EA				and r11, r0
RAM : 8C0026EC                 shlr16  r0
RAM : 8C0026EE					or r14, r0
RAM : 8C0026F0                 mov.w   r0, @(8, r4)
RAM : 8C0026F2                 extu.b  r7, r0
*/

loc_8C0026C4:
	r[0] = 0x50;
	r[3] = ReadMem16(r[0] + r[4]  );
	r[0] += 0x78;
	r[2] = ReadMem16(r[0] + r[4] );
	r[2] += r[3];
	r[0] = r[1];
	WriteMem16(r[4], r[2]);
	r[3] = r[5];
	WriteMem16(r[4] + 2, r[2]);
	r[3] &= r[14];
	r[0] = r[13];
	r[3] >>= 8;
	WriteMem16(r[4] + 4, r[0]);
	r[14] &= r[7];
	r[0] = (uint8_t)r[5];
	r[0] <<= 5;
	r[0] |= r[3];
	WriteMem16(r[4] + 6, r[0]);
	r[0] = r[7];
	r[0] &= r[11];
	r[0] >> 16;
	r[0] |= r[14];
	WriteMem16(r[4] + 8, r[0]);
	r[0] = (uint8_t)r[7];
/*

RAM : 8C0026F4
RAM : 8C0026F4 loc_8C0026F4 : ; CODE XREF : GDCC_HELPER_FUNC3 
RAM : 8C0026F4                 mov.w   r0, @(h'A,r4)
	RAM:8C0026F6                 mov.l   @r15 + , r11
	RAM : 8C0026F8                 mov.l   @r15 + , r13
	RAM : 8C0026FA                 rts
	RAM : 8C0026FC                 mov.l   @r15 + , r14*/

loc_8C0026F4:
	WriteMem16(r[4] + 0xa, r[0]);
	r[11] = stack_pop();
	r[13] = stack_pop();
	r[14] = stack_pop();

	Sh4cntx.pc = Sh4cntx.pr;

}
#undef r
#endif
 

bool reios_syscalls_init() {
	if (g_init_done)
		return true;

	g_init_done = true;

	extern void reios_boot();
	extern void reios_sys_system();
	extern void reios_sys_font();
	extern void reios_sys_flashrom();
	extern void reios_sys_gd();
	extern void reios_sys_misc();
	


	g_syscalls_mgr.register_syscall("reios_boot", &reios_boot, 0xA0000000, k_invalid_syscall);

#ifndef REIOS_WANT_EXPIREMENTAL_OLD_BUILD
	//g_syscalls_mgr.register_syscall("reios_sys_font", &reios_sys_font, 0x8C001002, dc_bios_syscall_font);

	g_syscalls_mgr.register_syscall("reios_sys_misc", &reios_sys_misc, 0x8C000800, dc_bios_syscall_misc);
 	g_syscalls_mgr.register_syscall("reios_sys_system", &reios_sys_system, 0x8C003C00, dc_bios_syscall_system);

	g_syscalls_mgr.register_syscall("reios_sys_flashrom", &reios_sys_flashrom, 0x8C003D00  , dc_bios_syscall_flashrom);
	
	g_syscalls_mgr.register_syscall("try_lock_gd_mutex", &try_lock_gd_mutex, 0x8C001970, k_no_syscall);
	g_syscalls_mgr.register_syscall("release_lock_gd_mutex", &release_lock_gd_mutex, 0x8C00197E, k_no_syscall);
	g_syscalls_mgr.register_syscall("get_reg_base_addr", &get_reg_base_addr, 0x8C001108, k_no_syscall);
	 
	g_syscalls_mgr.register_syscall("exit_from_gd_thread", &exit_from_gd_thread, 0x8C0018C0, k_no_syscall); 

	g_syscalls_mgr.register_syscall("gd_common_subroutine", &gd_common_subroutine, 0x8C002774, k_no_syscall);
	g_syscalls_mgr.register_syscall("gd_common_subroutine_reentry", []() {   gd_common_subroutine(); }, 0x8C0027A8, k_no_syscall);
	g_syscalls_mgr.register_syscall("gd_popular_function_jsr_r10", []() {  gd_popular_function(); }, 0x8C0027D6, k_no_syscall);
	
	g_syscalls_mgr.register_syscall("gd_popular_function", &gd_popular_function, 0x8C0027BA, k_no_syscall);
	g_syscalls_mgr.register_syscall("odd_wait_loop_function", &odd_wait_loop_function, 0x8C000772, k_no_syscall);

	g_syscalls_mgr.register_syscall("hook_and_trace_regs", &hook_and_trace_regs, 0x8C0011F2, k_no_syscall);

	g_syscalls_mgr.register_syscall("GDCC_HELPER_FUNC", &GDCC_HELPER_FUNC, 0x8C001118, k_no_syscall);
	g_syscalls_mgr.register_syscall("GDCC_HELPER_FUNC2", &GDCC_HELPER_FUNC2, 0x8C0026FE, k_no_syscall);
	g_syscalls_mgr.register_syscall("GDCC_HELPER_FUNC3", &GDCC_HELPER_FUNC3, 0x8C00266C, k_no_syscall);
 

 
#else
	g_syscalls_mgr.register_syscall("reios_sys_font", &reios_sys_font, 0x8C001002, dc_bios_syscall_font);
	g_syscalls_mgr.register_syscall("reios_sys_gd", &reios_sys_gd, 0x8C001000, dc_bios_syscall_gd);
	g_syscalls_mgr.register_syscall("reios_sys_system", &reios_sys_system, 0x8C001000, dc_bios_syscall_system);
	g_syscalls_mgr.register_syscall("reios_sys_flashrom", &reios_sys_flashrom, 0x8C001004, dc_bios_syscall_flashrom);
	g_syscalls_mgr.register_syscall("reios_sys_misc", &reios_sys_misc, 0x8C000800, dc_bios_syscall_misc);
	g_syscalls_mgr.register_syscall("gd_do_bioscall", &gd_do_bioscall, 0x8c0010F0, k_invalid_syscall);  
#endif
	return true;
}

 
