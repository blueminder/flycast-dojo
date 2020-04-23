
#include "bios_hle.hpp"

#define reg_ctx p_reg_context->cntx.r
#define freg_ctx p_reg_context->cntx.xffr
#define dc_bios_syscall_system				0x8C0000B0

#define dc_bios_syscall_font				0x8C0000B4

#define dc_bios_syscall_flashrom			0x8C0000B8

#define dc_bios_syscall_gd					0x8C0000BC

#define dc_bios_syscall_misc				0x8c0000E0



//At least one game (ooga) uses this directly

#define dc_bios_entrypoint_gd_do_bioscall	0x8c0010F0



#define SYSINFO_ID_ADDR 0x8C001010
struct bios_t {
    uint8_t* rom;
    uint8_t* flash;

    bios_t() : rom(nullptr), flash(nullptr) {}
};

static bool b_sysinfo_init_called = false;
static bool b_initialized = false;
static Sh4RCB* p_reg_context = nullptr;
static GDRomDisc* p_gdrom_disk = nullptr;
static bios_t bios;

bool bios_hle_func_8C0000B0_sys_fn();
bool bios_hle_func_8C0000BC_misc_gdrom_fn_2();
bool bios_hle_func_8C0000BC_misc_gdrom_fn();

u16 bios_hle_func_8C0000BC_misc_gdrom_fn_original;

u32 rehook_address = 0;
u16 rehook_old_opcode = 0;
u32 rehook_expected_pc = 0;

/*

    register_hook(0x8C001000, reios_sys_system);
    register_hook(0x8C001002, reios_sys_font);
    register_hook(0x8C001004, reios_sys_flashrom);
    register_hook(0x8C001006, reios_sys_gd);
    register_hook(0x8C001008, reios_sys_misc);


        memset(GetMemPtr(0x8C000000, 0), 0xFF, 64 * 1024);

    setup_syscall(hook_addr(&reios_sys_system), dc_bios_syscall_system);
    setup_syscall(hook_addr(&reios_sys_font), dc_bios_syscall_font);
    setup_syscall(hook_addr(&reios_sys_flashrom), dc_bios_syscall_flashrom);
    setup_syscall(hook_addr(&reios_sys_gd), dc_bios_syscall_gd);
    setup_syscall(hook_addr(&reios_sys_misc), dc_bios_syscall_misc);

    WriteMem32(dc_bios_entrypoint_gd_do_bioscall, REIOS_OPCODE);
    //Infinitive loop for arm !
    WriteMem32(0x80800000, 0xEAFFFFFE);

*/
static inline void mtx_lock() {
    
}

static inline void mtx_unlock() {
    
}

extern char ip_bin[256];

extern char reios_hardware_id[17];

extern char reios_maker_id[17];

extern char reios_device_info[17];

extern char reios_area_symbols[9];

extern char reios_peripherals[9];

extern char reios_product_number[11];

extern char reios_product_version[7];

extern char reios_releasedate[17];

extern char reios_boot_filename[17];

extern char reios_software_company[17];

extern char reios_software_name[129];

extern char reios_bootfile[32];


extern u32 base_fad ;

extern bool descrambl ;

const char* reios_locate_ip() {

    if (g_GDRDisc->GetDiscType() == GdRom) {

        base_fad = 45150;

        descrambl = false;

    }
    else {

        u8 ses[6];

        g_GDRDisc->GetSessionInfo(ses, 0);

        g_GDRDisc->GetSessionInfo(ses, ses[2]);

        base_fad = (ses[3] << 16) | (ses[4] << 8) | (ses[5] << 0);

        descrambl = true;

    }



    printf("reios: loading ip.bin from FAD: %d\n", base_fad);



    g_GDRDisc->ReadSector(GetMemPtr(0x8c008000, 0), base_fad, 16, 2048);



    memset(reios_bootfile, 0, sizeof(reios_bootfile));

    memcpy(reios_bootfile, GetMemPtr(0x8c008060, 0), 16);



    printf("reios: bootfile is '%s'\n", reios_bootfile);



    for (int i = 15; i >= 0; i--) {

        if (reios_bootfile[i] != ' ')

            break;

        reios_bootfile[i] = 0;

    }

    return reios_bootfile;

}

void bios_hle_boot() {

    printf("================\n");
    printf("HLE REIOS_V2 BOOT\n");
    printf("================\n");
    extern void reios_setup_state(u32 s);
    extern bool reios_locate_bootfile(const char* bootfile);
     

   // bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000B0_sys_fn , 0x8C0000B0);
    return;
    //bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000BC_misc_gdrom_fn,0x8C001006  );

    //bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000BC_misc_gdrom_fn,0x8C001008);



    //bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000BC_misc_gdrom_fn,dc_bios_entrypoint_gd_do_bioscall);


   // WriteMem32(0x8C0000B0, SH4_EXT_OP_REIOS_V2_TRAP);
    //WriteMem32(dc_bios_entrypoint_gd_do_bioscall, SH4_EXT_OP_REIOS_V2_TRAP);

   // WriteMem32(dc_bios_entrypoint_gd_do_bioscall, REIOS_OPCODE);
    //Infinitive loop for arm !
    //WriteMem32(0x80800000, 0xEAFFFFFE);
    

    const char* bootfile = reios_locate_ip();

    if (!bootfile || !reios_locate_bootfile(bootfile))
        msgboxf("Failed to locate bootfile", MBX_ICONERROR);


    printf("\n\nREIOS2 : BOOT\n\n");
    reios_setup_state(0xac008300);

    /*if (settings.reios.ElfFile.size()) {

        if (!reios_loadElf(settings.reios.ElfFile)) {

            msgboxf("Failed to open %s\n", MBX_ICONERROR, settings.reios.ElfFile.c_str());

        }

        reios_setup_state(0x8C010000);

    }

    else*/ {

       /* if (DC_PLATFORM == DC_PLATFORM_DREAMCAST)*/ {



        }

       /* else {

            verify(DC_PLATFORM == DC_PLATFORM_NAOMI);



            u32* sz = (u32*)naomi_cart_GetPtr(0x368, 4);

            if (!sz) {

                msgboxf("Naomi boot failure", MBX_ICONERROR);

            }



            int size = *sz;



            verify(size < RAM_SIZE && naomi_cart_GetPtr(size - 1, 1) && "Invalid cart size");



            WriteMemBlock_nommu_ptr(0x0c020000, (u32*)naomi_cart_GetPtr(0, size), size);



            reios_setuo_naomi(0x0c021000);

        }*/

    }
}

bool bios_hle_init(uint8_t* _rom, uint8_t* _flash ,Sh4RCB* _sh4rcb, GDRomDisc* _gdrom)  {
    printf("INIT REIOS_V2\n");  
    if (b_initialized) {
        bios_hle_shutdown();
    }
    bios.flash = _flash;
    bios.rom = _rom;
    p_reg_context = _sh4rcb;
    p_gdrom_disk = _gdrom;
    bios_hle_hooks_init(_sh4rcb);
    uint16_t* rom16 = (uint16_t*)_rom;

    rom16[0] = SH4_EXT_OP_REIOS_V2_TRAP;
    //bios_hle_hooks_register(0xA0000000, bios_hle_boot);

  //  bios_hle_hooks_register(0x8C001000, bios_hle_func_8C0000B0_sys_fn);



   // bios_hle_hooks_register(0x8C001006, bios_hle_func_8C0000BC_misc_gdrom_fn);

    //bios_hle_hooks_register(0x8C001008, bios_hle_func_8C0000BC_misc_gdrom_fn);



    //bios_hle_hooks_register(dc_bios_entrypoint_gd_do_bioscall, &bios_hle_func_8C0000BC_misc_gdrom_fn);

    bios_hle_boot();


    b_initialized = true;
    return true;
}

bool bios_hle_init_hybrid(uint8_t* _rom, uint8_t* _flash, Sh4RCB* _sh4rcb, GDRomDisc* _gdrom) {
   

    printf("====================\n");
    printf("REIOS_V2_HYBRID INIT\n");
    printf("====================\n");

    //rehook_address = 0;
    if  (!b_initialized) {
        bios.flash = _flash;
        bios.rom = _rom;
        p_reg_context = _sh4rcb;
        p_gdrom_disk = _gdrom;
        bios_hle_hooks_init(_sh4rcb);
        //b_initialized = true;
        extern void reios_setup_state(u32 s);
        extern bool reios_locate_bootfile(const char* bootfile);
        const char* bootfile = reios_locate_ip();
       
    }
    u16 tmp = bios_hle_hooks_register_function(0x8C001000, bios_hle_func_8C0000BC_misc_gdrom_fn_2);

    if (tmp != SH4_EXT_OP_REIOS_V2_TRAP)
        bios_hle_func_8C0000BC_misc_gdrom_fn_original = tmp;
    //0X8c003c00
   

    //bios_hle_hooks_register_set_syscall(0x8C0000B0 , 0x8C001000);
   
    // bios_hle_hooks_register(0x8C001006, bios_hle_func_8C0000BC_misc_gdrom_fn);
   //  bios_hle_hooks_register(0x8C001008, bios_hle_func_8C0000BC_misc_gdrom_fn);
    // bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000BC_misc_gdrom_fn, 0x8C001006);
    // bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000BC_misc_gdrom_fn, 0x8C001008);


     //bios_hle_hooks_register(dc_bios_entrypoint_gd_do_bioscall, &bios_hle_func_8C0000BC_misc_gdrom_fn);

  // bios_hle_hooks_patch_syscall(&bios_hle_func_8C0000B0_sys_fn, 0x8C0000B0);


     
    return true;
}

void bios_hle_shutdown() {
    bios_hle_hooks_shutdown();
    p_reg_context = nullptr;
    p_gdrom_disk = nullptr;
    b_initialized = false;
}

void bios_hle_func_8C0000B4_romfont_fn() {  //@ $8c003b8
    
}

void bios_hle_func_8C0000B8_flashrom_fn() {  //@ $8c003d00
    
}


/*

    simple scenario :

    pc = 0 

    old_op = read16(pc)
    write @op , 

    
*/

u32 prev_pr_addr = 0;

bool bios_hle_func_8C0000BC_misc_gdrom_fn_2() {
    //unpatch and let it run
    WriteMem16(0x8C001000, bios_hle_func_8C0000BC_misc_gdrom_fn_original);

    prev_pr_addr = p_reg_context->cntx.pr ;
    // patch return address

    // store original opcode
    rehook_old_opcode = ReadMem16(p_reg_context->cntx.pc);
    WriteMem16(p_reg_context->cntx.pc, SH4_EXT_OP_REIOS_V2_TRAP);

    // function to repatch on next REIOS_OP
    rehook_address = 0x8C001000;
    rehook_expected_pc = p_reg_context->cntx.pr;

    p_reg_context->cntx.pc = rehook_address + 2;// -= 2; // re execute the opcode
    return false; // let original function run
}

bool bios_hle_func_8C0000BC_misc_gdrom_fn() { // @ $8c001000
    /*
        r6=-1 - MISC superfunction 
        r6=0 - GDROM superfunction 
        r6=1-7 - user defined superfunction
    */
    
    printf("bios_hle_func_8C0000BC_misc_gdrom_fn r6 %d r7 %d\n", reg_ctx[6], reg_ctx[7]);
    printf("CALLING GD\n");
    return true;
    extern void gdrom_hle_op();
    gdrom_hle_op();
    return true;
#if FUCK
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

        printf("CALLING GD\n");
        extern void gdrom_hle_op();
        gdrom_hle_op();
        return;
        //GDROM superfunction 
#ifdef BIOS_HLE_SAFE_MODE
        const gdrom_syscall_info_type_e sfun = (gdrom_syscall_info_type_e)reg_ctx[7];
        if ( ((sfun >= GDROM_SFUNCTION_FIRST) + (sfun <= GDROM_SFUNCTION_LAST)) != 1 ) {
            printf("Call %s\n", bios_hle_gdrom_superfunctions_fptr[(uint32_t)reg_ctx[7]].name);
            bios_hle_gdrom_superfunctions_fptr[ (uint32_t)reg_ctx[7] ].fn();
        }
#else
        printf("Call %s\n", bios_hle_gdrom_superfunctions_fptr[(uint32_t)reg_ctx[7]].name);
        bios_hle_gdrom_superfunction_fptr[ (uint32_t)reg_ctx[7] & ((uint32_t)GDROM_SFUNCTION_COUNT-1U) ].fn();
#endif
        
    } else if ( ((reg_ctx[6] > 0) + (reg_ctx[6]< 8)) != 0 ) {
        //user defined superfunctions
    } else {
        reg_ctx[0] = -1;
    }
#endif
}

#if OOPS
bool bios_hle_func_8C0000B0_sys_fn() { // @ $8c003c00
    /*
        r7=0 - SYSINFO_INIT
        r7=1 - not a valid syscall
        r7=2 - SYSINFO_ICON
        r7=3 - SYSINFO_ID
    */
        

    printf("bios_hle_func_8C0000B0_sys_fn r7 = %d  : %d\n", reg_ctx[7], Sh4cntx.r[7]);

    
    switch ((system_syscall_info_type_e)reg_ctx[7]) {
        default: 
           // reg_ctx[0] = -1;
        return;
        
        case SYSINFO_INIT: {
            //Prepares the other two SYSINFO calls for use by copying the relevant data from the system flashrom into 8C000068-8C00007F
            //Returns zero
            //Args : none
           // reg_ctx[0] = 0;
            b_sysinfo_init_called = true;
            return; 
        }
        
        case SYSINFO_INVALID: {
           // reg_ctx[0] = 0;
            return;
        }    
        
        //Read an icon from the flashrom. The format those icons are in is not known
        //Returns: number of read bytes if successful, negative if read failed
        //Args : r4 = icon number (0-9, but only 5-9 seems to really be icons)
        //       r5 = destination buffer (704 bytes in size)
        case SYSINFO_ICON: {
            if (!b_sysinfo_init_called) {
                reg_ctx[7] = -1;
              //  return;
            }
            reg_ctx[0] = 704;
            return; 
        }   
        
        //Query the unique 64 bit ID number of this Dreamcast. SYSINFO_INIT must have been called first.
        //Returns: A pointer to where the ID is stored as 8 contiguous bytes
        //Args : none
        case SYSINFO_ID: {
            reg_ctx[0] = 0x8c000068;;
            return;
            WriteMem32(SYSINFO_ID_ADDR + 0, 0xe1e2e3e4);
            WriteMem32(SYSINFO_ID_ADDR + 4, 0xe5e6e7e8);

            reg_ctx[0] = SYSINFO_ID_ADDR;
            return; 
        } 
    }
}
#endif