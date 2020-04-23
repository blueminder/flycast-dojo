
#pragma once
#include "license/bsd"

#include "bios_hle_gdrom_opcodes.hpp"
#include "bios_hle_gdrom_superfuncs.hpp"
#include "bios_hle_flashrom.hpp"
#include "bios_hle_gdrom_queue.hpp"
#include "bios_hle_font.hpp"
#include "bios_hle_hooks.hpp"
#include "bios_hle_includes.hpp"


bool bios_hle_init(uint8_t* _rom, uint8_t* _flash, Sh4RCB* _sh4rcb, GDRomDisc* _gdrom);
bool bios_hle_init_hybrid(uint8_t* _rom, uint8_t* _flash, Sh4RCB* _sh4rcb, GDRomDisc* _gdrom);
void bios_hle_shutdown();

