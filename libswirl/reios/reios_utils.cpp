#include "license/bsd"
#include "reios_utils.h"
#include "hw/sh4/sh4_mem.h"

//guest set
void guest_mem_set(const uint32_t dst_addr, const uint8_t val, const uint32_t sz) { // TODO optimize by using pages efficiently
	for (uint32_t i = dst_addr, j = i + sz; i < j;) {
		WriteMem8(i++, val);
	}
}

//vmem 2 vmem addr cp
void guest_to_guest_memcpy(const uint32_t dst_addr, const uint32_t src_addr, const uint32_t sz) { // TODO optimize by using pages efficiently
	for (uint32_t i = src_addr, j = i + sz, k = dst_addr; i < j; ++i) {
		WriteMem8(k++, ReadMem8(i));
	}
}

//vmem to host addr cpy
void guest_to_host_memcpy(void* dst_addr, const uint32_t src_addr, const uint32_t sz) { // TODO optimize by using pages efficiently
	uint8_t* dst = static_cast<uint8_t*>(dst_addr);

	for (uint32_t i = src_addr, j = i + sz; i < j; ++i) {
		*(dst++) = ReadMem8(i);
	}
}

//host mem to vmem addr cpy
void host_to_guest_memcpy(const uint32_t dst_addr, const void* src_addr, const uint32_t sz) { // TODO optimize by using pages efficiently
	const uint8_t* src = static_cast<const uint8_t*>(src_addr);
	const uint8_t* src_end = src + sz;

	for (uint32_t loc = dst_addr; src < src_end;)
		WriteMem8(loc++, *(src++));
}

const uint32_t reios_locate_flash_cfg_addr() {
	constexpr uint32_t len = 16 * 1024;
	constexpr uint32_t base_addr = 0x8021c000;
	constexpr uint32_t end_addr = base_addr + len;
	constexpr const char magic[] = "KATANA_FLASH____";
	constexpr uint32_t magic_sz = sizeof(magic) - sizeof(char);

	uint8_t tmp[magic_sz];

	//Optimize this sh#t by getting page
	for (auto i = 0; i < magic_sz; ++i) {
		tmp[i] = ReadMem8(base_addr + i);
	}

	/*printf("Got (%u) :",magic_sz);
	for (auto i = 0; i < magic_sz; ++i)
		printf("%c", tmp[i]);
	printf("\n");*/
	if (memcmp(tmp, magic, magic_sz) != 0) {
		//printf("MAgic not found \n");
		return 0;
	}

	//printf("MAgic found %c%c%c!\n", tmp[0], tmp[1], tmp[2]);

	uint32_t res = std::numeric_limits<uint32_t>::max();

	for (uint32_t i = base_addr + 0x80, j = i + len; i < j; i += 0x40) {
		const uint16_t t = ReadMem16(i + 0);
		if ((t == 0x0500)) {
			res = i;
		}
	}
	return res;
}


