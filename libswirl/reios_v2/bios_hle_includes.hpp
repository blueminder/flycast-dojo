#pragma once
#include "license/bsd"
#include <stdint.h>
#include "hw/sh4/sh4_mem.h"
#include "hw/naomi/naomi_cart.h"
#include "libswirl.h"

#define bios_hle_trace(__x__ ,...) printf(__x__,__VA_ARGS__)
#define BIOS_HLE_SAFE_MODE
#define SH4_EXT_OP_REIOS_V2_TRAP (0x085B)
#define HLE_GENERATE_FUNCTION_INTERFACE(__ret_type__,__base__,__ext__,__args__) __ret_type__   __base__ ## __ext__ ## (__args__)
#define HLE_GENERATE_FUNCTION_INTERFACE_NO_ARGS(__ret_type__,__base__,__ext__ ) __ret_type__   __base__ ## __ext__ ## ()
#define HLE_ALIGN_REGION(__X__) DECL_ALIGN(__X__)


#if UINTPTR_MAX == UINT64_MAX
HLE_ALIGN_REGION(32) struct
#else
HLE_ALIGN_REGION(16) struct
#endif
callback_t {
    const char* name;
    int64_t id;
    void (*fn)(void);
};

enum system_syscall_info_type_e {
    SYSINFO_INIT = 0,
    SYSINFO_INVALID,
    SYSINFO_ICON,
    SYSINFO_ID
};

enum misc_syscall_info_type_e {
    MISC_INIT = 0,
    MISC_SETVECTOR
};

enum romfont_syscall_info_type_e {
    ROMFONT_ADDRESS = 0,
    ROMFONT_LOCK,
    ROMFONT_UNLOCK
};

enum flashrom_syscall_info_type_e {
    FLASHROM_INFO = 0,
    FLASHROM_READ,
    FLASHROM_WRITE,
    FLASHROM_DELETE
};

enum gdrom_status_e {
    GDROM_STATUS_ERROR = -1,
    GDROM_STATUS_IDLE = 0,
    GDROM_STATUS_BUSY = 1,
    GDROM_STATUS_READY = 2,
    GDROM_STATUS_ABORT = 3,
    GDROM_STATUS_COUNT = 5,
    GDROM_STATUS_FIRST = GDROM_STATUS_ERROR,
    GDROM_STATUS_LAST = GDROM_STATUS_ABORT,
};

enum gdrom_result_e {
    GDROM_RESULT_OK = 0,
    GDROM_RESULT_SYSTEM_ERROR = 1,
    GDROM_RESULT_NO_DISC = 2,
    GDROM_RESULT_INVALID_COMMAND = 5,
    GDROM_RESULT_DISC_CHANGE = 6,
    GDROM_RESULT_COUNT = 5,
    GDROM_RESULT_FIRST = GDROM_RESULT_OK,
    GDROM_RESULT_LAST = GDROM_RESULT_DISC_CHANGE
};


static const char* k_gdrom_result_strings [] = {
    "GDROM_STATUS_ERROR", //watchout -1
    "GDROM_STATUS_IDLE",
    "GDROM_STATUS_BUSY",
    "GDROM_STATUS_READY",
    "GDROM_STATUS_ABORT",
};

static const char* k_gdrom_status_strings [] = {
    "GDROM_STATUS_ERROR", //watchout -1
    "GDROM_STATUS_IDLE",
    "GDROM_STATUS_BUSY",
    "GDROM_STATUS_READY",
    "GDROM_STATUS_ABORT",
};

static const char* k_romfont_syscall_info_strings [] = {
    "ROMFONT_ADDRESS",
    "ROMFONT_LOCK",
    "ROMFONT_UNLOCK"
};

static const char* k_flashrom_syscall_info_strings [] = {
    "FLASHROM_INFO",
    "FLASHROM_READ",
    "FLASHROM_WRITE",
    "FLASHROM_DELETE"
};


static const char* k_system_syscall_info_strings [] = {
    "SYSINFO_INIT",
    "SYSINFO_INVALID",
    "SYSINFO_ICON",
    "SYSINFO_ID"
};

static const char* k_misc_syscall_info_strings [] = {
    "MISC_INIT",
    "MISC_SETVECTOR",
};


