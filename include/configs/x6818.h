/* SPDX-License-Identifier: GPL-2.0+
 *
 * Nexell S5P6818 x6818 board config.
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
#ifdef CONFIG_ARM64
/* The top 32 MiB contains BL31 and its secure runtime data. */
#define CFG_SYS_SDRAM_SIZE		(SZ_1G - SZ_32M)
#else
#define CFG_SYS_SDRAM_SIZE		SZ_1G
#endif

/* load addresses (all inside the 1 GB RAM window) */
#define BMP_LOAD_ADDR			0x78000000
#define INITRD_START			0x49000000
#define KERNEL_DTB_ADDR			0x4A000000
#define SPLASH_ADDR			BMP_LOAD_ADDR

/*-----------------------------------------------------------------------
 * ENV
 */
#define BLOADER_MMC							\
	"load mmc ${rootdev}:${bootpart} "

/* The AArch64 image is a complete OpenWrt system; keep its root contract
 * in the compiled default environment and restore it before every MMC boot.
 * mmcboot replaces the fallback device name with the current partition UUID,
 * so the same command covers both the eMMC and SD-card image.
 */
#define X6818_DEFAULT_BOOTARGS					\
	"console=ttySAC0,115200 earlycon=s5p6818,mmio,0xc00a1000 " \
	"root=/dev/mmcblk0p2 rootwait rw fstools_overlay_fstype=ext4"
#define X6818_BOOTARGS						\
	"bootargs=" X6818_DEFAULT_BOOTARGS "\0"
#define X6818_FALLBACK_BOOTARGS				\
	"if test ${rootdev} = 0; then "			\
		"setenv bootargs \"console=ttySAC0,115200 " \
			"earlycon=s5p6818,mmio,0xc00a1000 " \
			"root=/dev/mmcblk1p2 rootwait rw " \
			"fstools_overlay_fstype=ext4\"; " \
	"else "						\
		"setenv bootargs \"" X6818_DEFAULT_BOOTARGS "\"; " \
	"fi; "
#define X6818_SET_BOOTARGS					\
	"setenv rootpartuuid; "					\
	"if part uuid mmc ${rootdev}:${rootpart} rootpartuuid; then " \
		"setenv bootargs \"console=ttySAC0,115200 " \
			"earlycon=s5p6818,mmio,0xc00a1000 " \
			"root=PARTUUID=${rootpartuuid} rootwait rw " \
			"fstools_overlay_fstype=ext4\"; " \
	"else "						\
	"echo Failed to identify the MMC root partition; " \
	X6818_FALLBACK_BOOTARGS				\
	"fi; "

#ifdef CONFIG_ARM64
#define X6818_KERNEL_NAME	"kernel=Image\0"
#define X6818_DTB_NAME		"dtb_name=s5p6818-x6818-nexell-timer.dtb\0"
#define X6818_FIT_SETTINGS					\
	"fit_addr=0x50000000\0"					\
	"fit_name=openwrt-nexell-s5p6818_arm64-" 		\
		"nexell_x6818_arm64-fit-initramfs\0"		\
	"tftp_fit=tftp ${fit_addr} ${fit_name}\0"			\
	"fitboot=echo Booting AArch64 FIT from ${fit_addr} ...; "	\
		"bootm ${fit_addr}\0"
#define X6818_MMCBOOT						\
	"mmcboot="							\
		"echo Booting AArch64 Image from mmc ${rootdev}:${bootpart} ...; " \
		"if mmc dev ${rootdev} && mmc rescan; then "	\
			X6818_SET_BOOTARGS				\
			"if run load_kernel && run load_dtb; then "	\
				"booti ${loadaddr} - ${dtb_addr}; "	\
			"fi; "						\
		"fi\0"
#else
#define X6818_KERNEL_NAME	"kernel=zImage\0"
#define X6818_DTB_NAME		"dtb_name=s5p6818-x6818.dtb\0"
#define X6818_FIT_SETTINGS
#define X6818_MMCBOOT						\
	"legacy_kernel_block=0x5000\0"				\
	"legacy_kernel_sectors=0x3000\0"				\
	"legacy_mmcboot="						\
		"echo Booting legacy uImage from mmc ${rootdev} ...; "	\
		"mmc dev ${rootdev}; "					\
		"mmc read ${loadaddr} ${legacy_kernel_block} "		\
			"${legacy_kernel_sectors}; "			\
		"bootm ${loadaddr}\0"					\
	"mmcboot="							\
		"echo Booting from mmc ${rootdev}:${rootpart} ...; " \
		"if mmc dev ${rootdev} && mmc rescan; then "	\
			X6818_SET_BOOTARGS				\
			"if fstype mmc ${rootdev}:${bootpart}; then " \
				"if run load_kernel && run load_dtb; then "	\
					"bootz ${loadaddr} - ${dtb_addr}; "	\
				"else "						\
					"run legacy_mmcboot; "				\
				"fi; "						\
			"else "							\
				"run legacy_mmcboot; "				\
			"fi; "						\
		"fi\0"
#endif

#define CFG_EXTRA_ENV_SETTINGS					\
	"stdin=serial\0"						\
	"stdout=serial,vidconsole\0"				\
	"stderr=serial,vidconsole\0"				\
	"initrd_high=0xffffffff\0"				\
	"rootdev=" __stringify(CONFIG_ROOT_DEV) "\0"		\
	"rootpart=" __stringify(CONFIG_ROOT_PART) "\0"		\
	"bootpart=" __stringify(CONFIG_BOOT_PART) "\0"		\
	X6818_BOOTARGS						\
	X6818_KERNEL_NAME					\
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0"	\
	X6818_DTB_NAME						\
	X6818_FIT_SETTINGS					\
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
	"load_dtb="						\
		BLOADER_MMC "${dtb_addr} ${dtb_name}\0"		\
	"load_kernel="						\
		BLOADER_MMC "${loadaddr} ${kernel}\0"		\
	"load_initrd="						\
			BLOADER_MMC "${initrd_addr} ${initrd_name}; "	\
			"setenv initrd_size 0x${filesize}\0"		\
	X6818_MMCBOOT						\
	"bootcmd=run mmcboot\0"

#endif /* __CONFIG_H__ */
