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

/*
 * The iROM/BL2 hand-off leaves the selected SDMMC port in the power scratch
 * register.  Port 2 is the on-board eMMC (U-Boot mmc 2); port 0 is the SD
 * socket (U-Boot mmc 0).  Keep CONFIG_ROOT_DEV as the fail-safe for USB or
 * older firmware that does not provide the SDMMC boot-mode marker.
 */
static int x6818_boot_mmc_dev(void)
{
	u32 boot_mode = readl(PHY_BASEADDR_CLKPWR + SYSRSTCONFIG);

	if ((boot_mode & BOOTMODE_MASK) == BOOTMODE_SDMMC) {
		u32 port = readl(SCR_ARM_SECOND_BOOT_REG1);

		if (port == EMMC_PORT_NUM)
			return 2;
		if (port == SD_PORT_NUM)
			return 0;
	}

	return CONFIG_ROOT_DEV;
}

/* Keep the splash path independent of saved U-Boot environment variables.
 * The boot partition (${rootdev}:1) is FAT32 on both SD and eMMC layouts, so try
 * fatload first and keep ext4load as a fallback for ext4 boot partitions. */
#define X6818_SPLASH_LOAD_CMD \
	"if fatload mmc ${rootdev}:1 0x78000000 logo.bmp; then true; " \
	"else ext4load mmc ${rootdev}:1 0x78000000 logo.bmp; fi"
#define X6818_SPLASH_SHOW_CMD "bmp display 0x78000000"

int x6818_display_builtin_logo(struct udevice *dev);
int x6818_display_power_on(void);
int x6818_display_enable_rgb_pins(void);
int x6818_display_enable_backlight(void);

static int x6818_set_rootdev(void)
{
	int ret = env_set_ulong("rootdev", x6818_boot_mmc_dev());

	if (ret)
		printf("x6818: cannot select boot MMC (%d)\n", ret);

	return ret;
}

#ifdef CONFIG_TARGET_X6818_ARM64
static int x6818_console_settings(void)
{
	int ret = x6818_set_rootdev();

	/* The saved environment may predate the screen console. Change only
	 * the live copy, before console_init_r() replays the early log buffer.
	 */
	if (!ret)
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
#else
EVENT_SPY_SIMPLE(EVT_SETTINGS_R, x6818_set_rootdev);
#endif

int board_mmc_bootdev(void)
{
	return x6818_boot_mmc_dev();
}

/* call from common/env_mmc.c */
int mmc_get_env_dev(void)
{
	return x6818_boot_mmc_dev();
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
	u32 rst = readl((void __iomem *)CLKPWR_RESETSTATUS) & RESETSTATUS_MASK;
	const char *cause;

	if (rst & RESETSTATUS_POR)
		cause = "power-on";
	else if (rst & RESETSTATUS_WDT)
		cause = "watchdog";
	else if (rst & RESETSTATUS_SW)
		cause = "software";
	else if (rst & RESETSTATUS_GPIO)
		cause = "external";
	else
		cause = "unknown";

	printf("Board: Nexell S5P6818 x6818 (reset: %s, 0x%x)\n",
	       cause, rst);
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

	/* Publish reset reason for Linux /proc/cmdline */
	{
		u32 rst = readl((void __iomem *)CLKPWR_RESETSTATUS) &
			  RESETSTATUS_MASK;
		const char *reason = "unknown";

		if (rst & RESETSTATUS_POR)
			reason = "power-on";
		else if (rst & RESETSTATUS_WDT)
			reason = "watchdog";
		else if (rst & RESETSTATUS_SW)
			reason = "software";
		else if (rst & RESETSTATUS_GPIO)
			reason = "external";
		env_set("reset_reason", reason);
	}

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
		ret = fdt_setprop_string(blob, nodeoff, "bootargs", args);
		if (ret)
			return ret;
	}

#if CONFIG_IS_ENABLED(VIDEO)
	{
		struct udevice *vdev;
		if (!uclass_first_device_err(UCLASS_VIDEO, &vdev)) {
			struct video_uc_plat *plat = dev_get_uclass_plat(vdev);
			if (plat && plat->base && plat->size) {
				int fboff = fdt_node_offset_by_compatible(blob, -1, "simple-framebuffer");
				if (fboff >= 0) {
					const fdt32_t *ph = fdt_getprop(blob, fboff, "memory-region", &len);
					if (ph && len == 4) {
						u32 phandle = fdt32_to_cpu(*ph);
						int rsvoff = fdt_node_offset_by_phandle(blob, phandle);
						if (rsvoff >= 0) {
							int parent = fdt_parent_offset(blob, rsvoff);
							int na = fdt_address_cells(blob, parent);
							int ns = fdt_size_cells(blob, parent);
							fdt32_t reg[4];
							int idx = 0;

							if (na < 1 || na > 2 || ns < 1 || ns > 2) {
								na = 2;
								ns = 2;
							}
							if (na == 2)
								reg[idx++] = cpu_to_fdt32((u64)plat->base >> 32);
							reg[idx++] = cpu_to_fdt32((u32)plat->base);
							if (ns == 2)
								reg[idx++] = cpu_to_fdt32((u64)plat->size >> 32);
							reg[idx++] = cpu_to_fdt32((u32)plat->size);

							fdt_setprop(blob, rsvoff, "reg", reg, idx * sizeof(fdt32_t));
						}
					}
				}
			}
		}
	}
#endif

	return 0;
}
#endif
