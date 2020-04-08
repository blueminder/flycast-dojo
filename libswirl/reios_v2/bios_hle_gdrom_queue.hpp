#pragma once
#include "license/bsd"
#include <stdint.h>
#include "hw/sh4/sh4_mem.h"

#include "hw/naomi/naomi_cart.h"
#include "libswirl.h"
 
bool bios_hle_gdrom_queue_init();
void bios_hle_gdrom_queue_shutdown();