// SPDX-License-Identifier: GPL-2.0+
/*
 * S5P6818 AArch64 MMU map.
 *
 * BL31 occupies the top 32 MiB of the 1 GiB DRAM window. Keep that range
 * outside U-Boot's normal-memory map so speculative accesses cannot reach
 * secure firmware.
 */

#include <asm/armv8/mmu.h>
#include <linux/sizes.h>

static struct mm_region s5p6818_mem_map[] = {
	{
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = SZ_1G - SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE,
	}, {
		.virt = 0xc0000000UL,
		.phys = 0xc0000000UL,
		.size = SZ_256M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	}, {
		/* List terminator */
	}
};

struct mm_region *mem_map = s5p6818_mem_map;
