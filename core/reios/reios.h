#ifndef REIOS_H
#define REIOS_H

#include "types.h"
#include "imgread/common.h"

struct DiskIdentifier
{
	char hardware_id[17];
	char maker_id[17];
	char device_info[17];
	char area_symbols[9];
	char peripherals[9];
	char product_number[11];
	char product_version[7];
	char releasedate[17];
	char boot_filename[17];
	char software_company[17];
	char software_name[129];
	bool windows_ce;
};

bool reios_init(u8* rom, u8* flash);
void reios_reset();
void reios_term();
void DYNACALL reios_trap(u32 op);

const DiskIdentifier& reios_disk_id();
DiskIdentifier reios_disk_id(Disc *disc, u8 *dest = NULL, int nsectors = 1);

extern DiskIdentifier CurrentDiskIdentifier;

#define REIOS_OPCODE 0x085B

#endif //REIOS_H
