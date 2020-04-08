#pragma once
#include "license/bsd"

#include "bios_hle_includes.hpp"

enum gdrom_syscall_info_type_e {
    GDROM_SEND_COMMAND = 0,
    GDROM_CHECK_COMMAND,
    GDROM_MAINLOOP,
    GDROM_INIT,
    GDROM_CHECK_DRIVE,
    GDROM_G1_DMA_END,
    GDROM_REQ_DMA,
    GDROM_CHECK_DMA,
    GDROM_ABORT_COMMAND,
    GDROM_RESET,
    GDROM_SECTOR_MODE,
    GDROM_UNK1,
    GDROM_UNK2,
    GDROM_UNK3,
    GDROM_NOT_IMPL1,
    GDROM_NOT_IMPL2,
    GDROM_SFUNCTION_COUNT,
    GDROM_SFUNCTION_FIRST = GDROM_SEND_COMMAND,
    GDROM_SFUNCTION_LAST = GDROM_NOT_IMPL2
};

void bios_hle_gdrom_superfunction_impl_GDROM_SEND_COMMAND();
void bios_hle_gdrom_superfunction_impl_GDROM_CHECK_COMMAND();
void bios_hle_gdrom_superfunction_impl_GDROM_MAINLOOP();
void bios_hle_gdrom_superfunction_impl_GDROM_INIT();
void bios_hle_gdrom_superfunction_impl_GDROM_CHECK_DRIVE();
void bios_hle_gdrom_superfunction_impl_GDROM_G1_DMA_END();
void bios_hle_gdrom_superfunction_impl_GDROM_REQ_DMA();
void bios_hle_gdrom_superfunction_impl_GDROM_ABORT_COMMAND();
void bios_hle_gdrom_superfunction_impl_GDROM_RESET();
void bios_hle_gdrom_superfunction_impl_GDROM_SECTOR_MODE();
void bios_hle_gdrom_superfunction_impl_GDROM_UNK1();
void bios_hle_gdrom_superfunction_impl_GDROM_UNK2();
void bios_hle_gdrom_superfunction_impl_GDROM_UNK3();
void bios_hle_gdrom_superfunction_impl_GDROM_NOT_IMPL1();
void bios_hle_gdrom_superfunction_impl_GDROM_NOT_IMPL2();
 
static callback_t bios_hle_gdrom_superfunctions_fptr[] = {// respect the ORDER!!!
    {"GDROM_SEND_COMMAND",(int64_t)GDROM_SEND_COMMAND,&bios_hle_gdrom_superfunction_impl_GDROM_SEND_COMMAND},
    {"GDROM_CHECK_COMMAND",(int64_t)GDROM_CHECK_COMMAND,&bios_hle_gdrom_superfunction_impl_GDROM_CHECK_COMMAND},
    {"GDROM_MAINLOOP",(int64_t)GDROM_MAINLOOP,&bios_hle_gdrom_superfunction_impl_GDROM_MAINLOOP},
    {"GDROM_INIT",(int64_t)GDROM_INIT,&bios_hle_gdrom_superfunction_impl_GDROM_INIT},
    {"GDROM_CHECK_DRIVE",(int64_t)GDROM_CHECK_DRIVE,&bios_hle_gdrom_superfunction_impl_GDROM_CHECK_DRIVE},
    {"GDROM_G1_DMA_END",(int64_t)GDROM_G1_DMA_END,&bios_hle_gdrom_superfunction_impl_GDROM_G1_DMA_END},
    {"GDROM_REQ_DMA",(int64_t)GDROM_REQ_DMA,&bios_hle_gdrom_superfunction_impl_GDROM_REQ_DMA},
    {"GDROM_ABORT_COMMAND",(int64_t)GDROM_ABORT_COMMAND,&bios_hle_gdrom_superfunction_impl_GDROM_ABORT_COMMAND},
    {"GDROM_RESET",(int64_t)GDROM_RESET,&bios_hle_gdrom_superfunction_impl_GDROM_RESET},
    {"GDROM_SECTOR_MODE",(int64_t)GDROM_SECTOR_MODE,&bios_hle_gdrom_superfunction_impl_GDROM_SECTOR_MODE},
    {"GDROM_UNK1",(int64_t)GDROM_UNK1,&bios_hle_gdrom_superfunction_impl_GDROM_UNK1},
    {"GDROM_UNK2",(int64_t)GDROM_UNK2,&bios_hle_gdrom_superfunction_impl_GDROM_UNK2},
    {"GDROM_UNK3",(int64_t)GDROM_UNK3,&bios_hle_gdrom_superfunction_impl_GDROM_UNK3},
    {"GDROM_NOT_IMPL1",(int64_t)GDROM_NOT_IMPL1,&bios_hle_gdrom_superfunction_impl_GDROM_NOT_IMPL1},
    {"GDROM_NOT_IMPL2",(int64_t)GDROM_NOT_IMPL2,&bios_hle_gdrom_superfunction_impl_GDROM_NOT_IMPL2},
};
