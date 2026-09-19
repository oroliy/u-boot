// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 x6818 board init (AArch32 and AArch64 BL33).
 *
 * Trimmed from board/friendlyarm/nanopi2/board.c: keeps the Nexell
 * DDR-info-register RAM detection. Display and splash setup are described by
 * the board DT and the x6818 display glue.
  */

#include <config.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <event.h>
#include <fdt_support.h>
#include <log.h>
#include <phy.h>
#include <video.h>
#include <asm/global_data.h>
#include <asm/io.h>

#include <asm/arch/nexell.h>

#include "boot-console.h"

DECLARE_GLOBAL_DATA_PTR;

/* The vendor x6818 U-Boot boots the OS from eMMC, dwmmc.2. */
static int mmc_boot_dev = CONFIG_ROOT_DEV;

/* Keep the splash path independent of saved U-Boot environment variables.
 * The boot partition (mmc 2:1) is FAT32 on both SD and eMMC layouts, so try
 * fatload first and keep ext4load as a fallback for ext4 boot partitions. */
#define X6818_SPLASH_LOAD_CMD \
	"if fatload mmc 2:1 0x78000000 logo.bmp; then true; " \
	"else ext4load mmc 2:1 0x78000000 logo.bmp; fi"
#define X6818_SPLASH_SHOW_CMD "bmp display 0x78000000"

int x6818_display_builtin_logo(struct udevice *dev);
int x6818_display_power_on(void);
int x6818_display_enable_rgb_pins(void);
int x6818_display_enable_backlight(void);

#ifdef CONFIG_TARGET_X6818_ARM64
static int x6818_console_settings(void)
{
	int ret;

	/* The saved environment may predate the screen console. Change only
	 * the live copy, before console_init_r() replays the early log buffer.
	 */
	ret = env_set("stdout", "serial,vidconsole");
	if (!ret)
		ret = env_set("stderr", "serial,vidconsole");
	if (ret)
		printf("x6818: cannot select dual console (%d)\n", ret);

	/* Power and mux must be ready when the console probes the video device. */
	x6818_display_power_on();
	x6818_display_enable_rgb_pins();
	return 0;
}

EVENT_SPY_SIMPLE(EVT_SETTINGS_R, x6818_console_settings);
#endif

int board_mmc_bootdev(void)
{
	return mmc_boot_dev;
}

/* call from common/env_mmc.c */
int mmc_get_env_dev(void)
{
	return mmc_boot_dev;
}

/*
 * The x6818 board uses the RTL8211E's strap-selected RGMII delays.  The
 * modern generic driver programs the extended delay register for plain
 * "rgmii" and clears those strap values, while the vendor U-Boot only reset
 * the PHY and started autonegotiation.  Preserve the vendor behaviour here.
 */
#define X6818_RTL8211E_PHY_ID		0x001cc915
#define X6818_RTL8211E_PHY_ID_MASK	0x00ffffff

int board_phy_config(struct phy_device *phydev)
{
	int ret;

	if ((phydev->phy_id & X6818_RTL8211E_PHY_ID_MASK) ==
	    X6818_RTL8211E_PHY_ID) {
		printf("x6818: RTL8211E preserving strap RGMII delays\n");
		ret = phy_reset(phydev);
		if (ret)
			return ret;

		return genphy_config_aneg(phydev);
	}

	if (phydev->drv && phydev->drv->config)
		return phydev->drv->config(phydev);

	return 0;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board: Nexell S5P6818 x6818\n");
	return 0;
}
#endif

int board_early_init_f(void)
{
	return 0;
}

int board_init(void)
{
	if (IS_ENABLED(CONFIG_SILENT_CONSOLE))
		gd->flags |= GD_FLG_SILENT;

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	struct udevice *video = NULL;
	int ret;

	if (IS_ENABLED(CONFIG_SILENT_CONSOLE))
		gd->flags &= ~GD_FLG_SILENT;

	/* Probe the panel before the splash file can affect display init. */
	if (IS_ENABLED(CONFIG_VIDEO)) {
		ret = x6818_display_power_on();
		if (ret)
			printf("x6818: LCD power init failed (%d)\n", ret);

		ret = x6818_display_enable_rgb_pins();
		if (ret)
			printf("x6818: RGB pin init failed (%d)\n", ret);

		ret = uclass_first_device_err(UCLASS_VIDEO, &video);
		if (ret)
			printf("x6818: display init failed (%d)\n", ret);
		else {
			ret = x6818_display_enable_backlight();
			if (ret)
				printf("x6818: backlight init failed (%d)\n", ret);
		}
	}

	/* Boot logs own the screen by default. A manual splash remains opt-in. */
	if (env_get_yesno("show_splash") != 1)
		return 0;

	/* Load and show the splash before autoboot can be interrupted. */
	if (video && IS_ENABLED(CONFIG_CMD_FAT) && IS_ENABLED(CONFIG_CMD_BMP)) {
		ret = run_command(X6818_SPLASH_LOAD_CMD, 0);
		if (!ret) {
			ret = run_command(X6818_SPLASH_SHOW_CMD, 0);
			if (!ret)
				printf("x6818: logo.bmp displayed\n");
		}

		if (ret) {
			printf("x6818: logo.bmp unavailable, using built-in logo\n");
			ret = x6818_display_builtin_logo(video);
			if (ret)
				printf("x6818: built-in logo failed (%d)\n", ret);
			else
				printf("x6818: built-in logo displayed\n");
		}
	} else if (video) {
		ret = x6818_display_builtin_logo(video);
		if (ret)
			printf("x6818: built-in logo failed (%d)\n", ret);
		else
			printf("x6818: built-in logo displayed\n");
	}

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
	if (!IS_ENABLED(CONFIG_ARM64)) {
		gd->bd->bi_arch_number = 4330;
		gd->bd->bi_boot_params = CFG_SYS_SDRAM_BASE + 0x00000100;
	}

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
	char args[2048];
	const char *input;
	int len, ret;

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

	if (IS_ENABLED(CONFIG_TARGET_X6818_ARM64)) {
		nodeoff = fdt_find_or_add_subnode(blob, 0, "chosen");
		if (nodeoff < 0)
			return nodeoff;
		input = fdt_getprop(blob, nodeoff, "bootargs", &len);
		if (input && (len <= 0 || !memchr(input, '\0', len)))
			return -EINVAL;
		ret = x6818_console_bootargs(args, sizeof(args), input);
		if (ret) {
			printf("x6818: bootargs too long for dual console\n");
			return ret;
		}
		return fdt_setprop_string(blob, nodeoff, "bootargs", args);
	}

	return 0;
}
#endif
