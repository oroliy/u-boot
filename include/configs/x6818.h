/* SPDX-License-Identifier: GPL-2.0+
 *
 * Nexell S5P6818 x6818 board config (AArch32).
 *
 * Trimmed from include/configs/s5p4418_nanopi2.h; RAM map corrected to the
 * x6818 (1 GB @ 0x40000000).
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/sizes.h>
#include <asm/arch/nexell.h>

/*-----------------------------------------------------------------------
 * System memory (x6818: 1 GB @ 0x40000000)
 */
#define CFG_SYS_SDRAM_BASE		0x40000000
#define CFG_SYS_SDRAM_SIZE		SZ_1G		/* refined by dram_init_banksize() */

/* load addresses (all inside the 1 GB RAM window) */
#define BMP_LOAD_ADDR			0x78000000
#define INITRD_START			0x49000000
#define KERNEL_DTB_ADDR			0x4A000000
#define SPLASH_ADDR			BMP_LOAD_ADDR

/*-----------------------------------------------------------------------
 * ENV
 */
#define BLOADER_MMC							\
	"ext4load mmc ${rootdev}:${bootpart} "

#define CFG_EXTRA_ENV_SETTINGS					\
	"initrd_high=0xffffffff\0"				\
	"rootdev=" __stringify(CONFIG_ROOT_DEV) "\0"		\
	"rootpart=" __stringify(CONFIG_ROOT_PART) "\0"		\
	"bootpart=" __stringify(CONFIG_BOOT_PART) "\0"		\
	"kernel=zImage\0"					\
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0"	\
	"dtb_name=s5p6818-x6818.dtb\0"				\
	"dtb_addr=" __stringify(KERNEL_DTB_ADDR) "\0"		\
	"initrd_name=ramdisk.img\0"				\
	"initrd_addr=" __stringify(INITRD_START) "\0"		\
	"initrd_size=0x600000\0"				\
	"ethact=dwmac.c0060000\0"				\
	"ethprime=RTL8211\0"				\
	"ethaddr=00:e2:1c:ba:e8:60\0"				\
	"ipaddr=10.1.1.99\0"				\
	"serverip=10.1.1.100\0"				\
	"netmask=255.255.255.0\0"				\
	"gatewayip=10.1.1.1\0"				\
	"legacy_kernel_block=0x5000\0"				\
	"legacy_kernel_sectors=0x3000\0"				\
	"load_dtb="						\
		BLOADER_MMC "${dtb_addr} ${dtb_name}\0"		\
	"load_kernel="						\
		BLOADER_MMC "${loadaddr} ${kernel}\0"		\
	"load_initrd="						\
			BLOADER_MMC "${initrd_addr} ${initrd_name}; "	\
			"setenv initrd_size 0x${filesize}\0"		\
	"legacy_mmcboot="						\
			"echo Booting legacy uImage from mmc ${rootdev} ...; "\
			"mmc dev ${rootdev}; "					\
			"mmc read ${loadaddr} ${legacy_kernel_block} "\
				"${legacy_kernel_sectors}; "				\
			"bootm ${loadaddr}\0"					\
	"mmcboot="						\
			"echo Booting from mmc ${rootdev}:${rootpart} ...; " \
			"if fstype mmc ${rootdev}:${bootpart}; then "\
				"if run load_kernel && run load_dtb; then "\
					"bootz ${loadaddr} - ${dtb_addr}; " \
				"else "						\
					"run legacy_mmcboot; "				"fi; "\
			"else "							\
				"run legacy_mmcboot; "					\
			"fi\0"						\
	"bootcmd=run mmcboot\0"

#endif /* __CONFIG_H__ */
