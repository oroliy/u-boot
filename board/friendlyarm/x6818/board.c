// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 x6818 board init (AArch32, headless).
 *
 * Trimmed from board/friendlyarm/nanopi2/board.c: keeps the Nexell
 * DDR-info-register RAM detection and the SD-vs-eMMC boot-device decode,
 * drops all display/onewire/backlight/splash code.
  */

#include <config.h>
#include <command.h>
#include <fdt_support.h>
#include <log.h>
#include <asm/global_data.h>
#include <asm/io.h>

#include <asm/arch/nexell.h>

DECLARE_GLOBAL_DATA_PTR;

/* DEFAULT mmc dev for eMMC boot (dwmmc.2) */
static int mmc_boot_dev;

int board_mmc_bootdev(void)
{
	return mmc_boot_dev;
}

/* call from common/env_mmc.c */
int mmc_get_env_dev(void)
{
	return mmc_boot_dev;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board: Nexell S5P6818 x6818\n");
	return 0;
}
#endif

/* -------------------------------------------------------------------------- */

#define	MMC_BOOT_CH0		(0)
#define	MMC_BOOT_CH1		(1 <<  3)
#define	MMC_BOOT_CH2		(1 << 19)

static void bd_bootdev_init(void)
{
	unsigned int rst = readl(PHY_BASEADDR_CLKPWR + SYSRSTCONFIG);

	rst &= (1 << 19) | (1 << 3);
	if (rst == MMC_BOOT_CH0) {
		/* mmc dev 1 for SD boot */
		mmc_boot_dev = 1;
	}
}

/* -------------------------------------------------------------------------- */

int board_early_init_f(void)
{
	return 0;
}

int board_init(void)
{
	bd_bootdev_init();

	if (IS_ENABLED(CONFIG_SILENT_CONSOLE))
		gd->flags |= GD_FLG_SILENT;

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	if (IS_ENABLED(CONFIG_SILENT_CONSOLE))
		gd->flags &= ~GD_FLG_SILENT;

	return 0;
}
#endif

/* u-boot dram initialize */
int dram_init(void)
{
	gd->ram_size = CFG_SYS_SDRAM_SIZE;
	return 0;
}

/* u-boot dram board specific */
int dram_init_banksize(void)
{
#define SCR_USER_SIG6_READ		(SCR_ALIVE_BASE + 0x0F0)
	unsigned int reg_val = readl(SCR_USER_SIG6_READ);

	/* set global data memory */
	gd->bd->bi_boot_params = CFG_SYS_SDRAM_BASE + 0x00000100;

	gd->bd->bi_dram[0].start = CFG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size  = CFG_SYS_SDRAM_SIZE;

	/* Number of Row: 14 bits */
	if ((reg_val >> 28) == 14)
		gd->bd->bi_dram[0].size -= 0x20000000;

	/* Number of Memory Chips */
	if ((reg_val & 0x3) > 1) {
		gd->bd->bi_dram[1].start = 0x80000000;
		gd->bd->bi_dram[1].size  = 0x40000000;
	}
	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int nodeoff;
	unsigned int rootdev;

	if (board_mmc_bootdev() > 0) {
		rootdev = fdt_getprop_u32_default(blob, "/board", "sdidx", 2);
		if (rootdev) {
			/* find or create "/chosen" node. */
			nodeoff = fdt_find_or_add_subnode(blob, 0, "chosen");
			if (nodeoff >= 0)
				fdt_setprop_u32(blob, nodeoff, "linux,rootdev",
						rootdev);
		}
	}

	return 0;
}
#endif
