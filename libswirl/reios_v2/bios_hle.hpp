
#pragma once
#include "license/bsd"

#include "bios_hle_gdrom_opcodes.hpp"
#include "bios_hle_gdrom_superfuncs.hpp"
#include "bios_hle_flashrom.hpp"
#include "bios_hle_font.hpp"




bool bios_hle_init(Sh4RCB* p_sh4rcb,GDRomDisc* gdrom);
void bios_hle_shutdown();

