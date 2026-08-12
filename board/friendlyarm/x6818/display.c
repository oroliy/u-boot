// SPDX-License-Identifier: GPL-2.0+
/*
 * x6818 WY070ML MIPI panel support.
 *
 * The panel sequence is carried over from the vendor U-Boot.  The current
 * Nexell MIPI host already provides the DSI transport; this file supplies
 * the board-specific reset and short DCS writes.
 */

#include <errno.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <stdio.h>
#include <asm/io.h>
#include <asm/arch/display.h>
#include <asm/arch/clk.h>
#include <asm/arch/mipi_display.h>
#include <asm/arch/nexell.h>
#include <asm/arch/nx_gpio.h>
#include <asm/arch/pwm.h>

#define PANEL_RESET_BIT	(1U << 8)
#define LCD_POWER_GPIO	10
#define BACKLIGHT_EN_GPIO	12
#define BACKLIGHT_PWM_GPIO	1

static void nx_gpio_config_direct(
	volatile struct nx_gpio_register_set *gpio, unsigned int bit,
	unsigned int function, int output, int value)
{
	u32 mask = 1U << bit;
	u32 shift = (bit & 0xf) * 2;
	u32 altfn;

	altfn = readl(&gpio->gpioxaltfn[bit / 16]);
	altfn &= ~(3U << shift);
	altfn |= function << shift;
	writel(altfn, &gpio->gpioxaltfn[bit / 16]);

	/* Match the vendor pad setup: no internal pull, explicit drive value. */
	clrbits_le32(&gpio->gpiox_pullenb, mask);
	clrbits_le32(&gpio->gpiox_pullsel, mask);
	clrbits_le32(&gpio->gpiox_drv0, mask);
	clrbits_le32(&gpio->gpiox_drv1, mask);

	if (value)
		setbits_le32(&gpio->gpioxout, mask);
	else
		clrbits_le32(&gpio->gpioxout, mask);

	if (output)
		setbits_le32(&gpio->gpioxoutenb, mask);
	else
		clrbits_le32(&gpio->gpioxoutenb, mask);
}

static void wy070ml_reset(void)
{
	volatile struct nx_gpio_register_set *gpio =
		(void *)PHY_BASEADDR_GPIOB;

	/* Keep GPIOB8 in GPIO mode in case pinctrl was not probed yet. */
	nx_gpio_config_direct(gpio, 8, 0, 1, 1);
	/* Match the vendor board pad: pull-up on the active-high reset line. */
	setbits_le32(&gpio->gpiox_pullenb, PANEL_RESET_BIT);
	setbits_le32(&gpio->gpiox_pullsel, PANEL_RESET_BIT);

	setbits_le32(&gpio->gpioxout, PANEL_RESET_BIT);
	mdelay(20);
	clrbits_le32(&gpio->gpioxout, PANEL_RESET_BIT);
	mdelay(20);
	setbits_le32(&gpio->gpioxout, PANEL_RESET_BIT);
	mdelay(120);
}

static int wy070ml_write(struct mipi_dsi_device *dsi, u8 command, u8 value)
{
	u8 data[2] = { command, value };

	return dsi->write_buffer(dsi, data, sizeof(data)) == sizeof(data) ?
		0 : -EIO;
}

static int wy070ml_prepare(struct mipi_dsi_device *dsi)
{
	static const u8 init[] = {
		0x80, 0x47,
		0x81, 0x40,
		0x82, 0x04,
		0x83, 0x77,
		0x84, 0x0f,
		0x85, 0x70,
		0x86, 0x70,
	};
	u8 reset = 0x01;
	unsigned int i;
	ssize_t ret;

	wy070ml_reset();

	ret = dsi->write_buffer(dsi, &reset, sizeof(reset));
	if (ret != sizeof(reset))
		return ret < 0 ? ret : -EIO;
	mdelay(30);

	for (i = 0; i < sizeof(init); i += 2) {
		ret = wy070ml_write(dsi, init[i], init[i + 1]);
		if (ret)
			return ret;
	}

	printf("MIPI: WY070ML DCS init complete\n");

	return 0;
}

static int wy070ml_enable(struct mipi_dsi_device *dsi)
{
	return 0;
}

static struct mipi_panel_ops wy070ml_ops = {
	.prepare = wy070ml_prepare,
	.enable = wy070ml_enable,
};

int x6818_display_power_on(void)
{
	volatile struct nx_gpio_register_set *gpioc =
		(void *)PHY_BASEADDR_GPIOC;

	/* The panel must be powered before reset and any DCS command. */
	nx_gpio_config_direct(gpioc, LCD_POWER_GPIO, 1, 1, 1);
	printf("x6818: LCD power enabled (GPIOC10)\n");

	return 0;
}

int x6818_display_enable_backlight(void)
{
	volatile struct nx_gpio_register_set *gpiod =
		(void *)PHY_BASEADDR_GPIOD;
	volatile struct nx_gpio_register_set *gpioe =
		(void *)PHY_BASEADDR_GPIOE;
	const struct s5p_timer *pwm = (void *)PHY_BASEADDR_PWM;
	struct clk *pwm_clk = clk_get(DEV_NAME_PWM ".0");
	u32 altfn;
	unsigned long pwm_rate = 0;
	int ret_init;
	int ret_config;
	int ret_enable;

	/* Vendor board: backlight enable GPIOE12. */
	nx_gpio_config_direct(gpioe, BACKLIGHT_EN_GPIO, 0, 1, 1);

	/* Vendor board also drives the backlight through PWM0 on GPIOD1. */
	/* PWM0_OUT is ALT1 on GPIOD1; ALT0 leaves the pin as GPIO. */
	nx_gpio_config_direct(gpiod, BACKLIGHT_PWM_GPIO, 1, 0, 0);
	ret_init = s5p_pwm_init(0, MUX_DIV_1, 0);
	ret_config = ret_init ? ret_init : s5p_pwm_config(0, 500000, 1000000);
	ret_enable = ret_config ? ret_config : s5p_pwm_enable(0);
	if (!IS_ERR(pwm_clk))
		pwm_rate = clk_get_rate(pwm_clk);

	altfn = readl(&gpiod->gpioxaltfn[0]);
	printf("x6818: LCD backlight enabled (GPIOE12 PWM0/GPIOD1)\n");
	printf("x6818: backlight GPIOD1 alt=%u pwm_rate=%lu init=%d config=%d "
	       "enable=%d tcon=0x%08x tcfg0=0x%08x "
	       "tcfg1=0x%08x tcntb0=0x%08x tcmpb0=0x%08x\n",
	       (altfn >> (BACKLIGHT_PWM_GPIO * 2)) & 3,
	       pwm_rate, ret_init, ret_config, ret_enable,
	       readl(&pwm->tcon), readl(&pwm->tcfg0), readl(&pwm->tcfg1),
	       readl(&pwm->tcntb0), readl(&pwm->tcmpb0));

	return 0;
}

int nx_mipi_dsi_lcd_bind(struct mipi_dsi_device *dsi)
{
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;
	dsi->ops = &wy070ml_ops;

	return 0;
}
