
#include "bios_hle.hpp"
#define reg_ctx p_reg_context->cntx.r
#define freg_ctx p_reg_context->cntx.xffr

static bool b_sysinfo_init_called = false;
static bool b_initialized = false;
static Sh4RCB* p_reg_context = nullptr;
static GDRomDisc* p_gdrom_disk = nullptr;

static inline void mtx_lock() {
    
}

static inline void mtx_unlock() {
    
}

//XXX TODO Mutex lock : sim atomic OP
bool bios_hle_init(Sh4RCB* p_sh4rcb,GDRomDisc* gdrom) {
    if (b_initialized) {
        bios_hle_shutdown();
    }
    p_reg_context = p_sh4rcb;
    p_gdrom_disk = gdrom;
    b_initialized = true;
    return true;
}

void bios_hle_shutdown() {
    p_reg_context = nullptr;
    p_gdrom_disk = nullptr;
    b_initialized = false;
}

void bios_hle_func_8C0000B4_romfont_fn() {  //@ $8c003b8
    
}

void bios_hle_func_8C0000B8_flashrom_fn() {  //@ $8c003d00
    
}

void bios_hle_func_8C0000BC_misc_gdrom_fn() { // @ $8c001000
    /*
        r6=-1 - MISC superfunction 
        r6=0 - GDROM superfunction 
        r6=1-7 - user defined superfunction
    */
    
    
    if (reg_ctx[6] == -1) {
        //MISC superfunction 
        const misc_syscall_info_type_e sfun = (misc_syscall_info_type_e)reg_ctx[7];
        if (sfun == MISC_INIT) {
            
        } else if (sfun == MISC_SETVECTOR) {
            /*
                Sets/clears the handler for one of the eight superfunctions for this vector. Setting a handler is only allowed if it not currently set.
                Args: 
                r4 = superfunction number (0-7)
                r5 = pointer to handler function, or NULL to clear

                Returns: zero if successful, -1 if setting/clearing the handler fails
            */
            if ((int32_t)reg_ctx[4] < 0) {
                reg_ctx[0] = -1;
                return;
            } else if ((int32_t)reg_ctx[4] > 7) {
                reg_ctx[0] = -1;
                return;
            }
            
            /*
            0x8C001180 - 0x10c0 = 0x8C0000C0
            */
            if ( (reg_ctx[5] == 0)   ){
                //0x8C0000C0
            }
        } else {
            //TODO ERROR
        }
        
    } else if (reg_ctx[6] == 0) {
        //GDROM superfunction 
#ifdef BIOS_HLE_SAFE_MODE
        const gdrom_syscall_info_type_e sfun = (gdrom_syscall_info_type_e)reg_ctx[7];
        if ( ((sfun >= GDROM_SFUNCTION_FIRST) + (sfun <=  GDROM_SEND_COMMAND)) != 1 ) {
            bios_hle_gdrom_superfunctions_fptr[ (uint32_t)reg_ctx[7] ].fn();
        }
#else
        bios_hle_gdrom_superfunction_fptr[ (uint32_t)reg_ctx[7] & ((uint32_t)GDROM_SFUNCTION_COUNT-1U) ].fn();
#endif
        
    } else if ( ((reg_ctx[6] > 0) + (reg_ctx[6]< 8)) != 0 ) {
        //user defined superfunctions
    } else {
        reg_ctx[0] = -1;
    }
}

void bios_hle_func_8C0000B0_sys_fn() { // @ $8c003c00
    /*
        r7=0 - SYSINFO_INIT
        r7=1 - not a valid syscall
        r7=2 - SYSINFO_ICON
        r7=3 - SYSINFO_ID
    */
        
    switch ((system_syscall_info_type_e)reg_ctx[7]) {
        default: 
            reg_ctx[0] = -1;
        return;
        
        case SYSINFO_INIT: {
            //Prepares the other two SYSINFO calls for use by copying the relevant data from the system flashrom into 8C000068-8C00007F
            //Returns zero
            //Args : none
            reg_ctx[0] = 0;
            b_sysinfo_init_called = true;
            return; 
        }
        
        case SYSINFO_INVALID: {
            reg_ctx[0] = 0;
            return;
        }    
        
        //Read an icon from the flashrom. The format those icons are in is not known
        //Returns: number of read bytes if successful, negative if read failed
        //Args : r4 = icon number (0-9, but only 5-9 seems to really be icons)
        //       r5 = destination buffer (704 bytes in size)
        case SYSINFO_ICON: {
            if (!b_sysinfo_init_called) {
                reg_ctx[7] = -1;
                return;
            }
            reg_ctx[0] = 704;
            return; 
        }   
        
        //Query the unique 64 bit ID number of this Dreamcast. SYSINFO_INIT must have been called first.
        //Returns: A pointer to where the ID is stored as 8 contiguous bytes
        //Args : none
        case SYSINFO_ID: {
            
            reg_ctx[0] = 0;
            return; 
        } 
    }
}