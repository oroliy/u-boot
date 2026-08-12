// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell S5P6818 UART driver.
 *
 * The UART uses the Samsung S5P register layout, not ARM PL011. The clock
 * and reset setup follows the vendor U-Boot implementation.
 */

#include <dm.h>
#include <errno.h>
#include <serial.h>
#include <vsprintf.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/nexell.h>
#include <asm/arch/reset.h>
#include <linux/delay.h>
#include <linux/err.h>

DECLARE_GLOBAL_DATA_PTR;

#define S5P6818_RX_FIFO_COUNT_MASK	GENMASK(7, 0)
#define S5P6818_RX_FIFO_FULL		BIT(8)
#define S5P6818_TX_FIFO_COUNT_MASK	GENMASK(23, 16)
#define S5P6818_TX_FIFO_FULL		BIT(24)

union s5p6818_baud_rest {
	u16 slot;
	u8 value;
};

struct s5p6818_uart_regs {
	u32 ulcon;
	u32 ucon;
	u32 ufcon;
	u32 umcon;
	u32 utrstat;
	u32 uerstat;
	u32 ufstat;
	u32 umstat;
	u8 utxh;
	u8 reserved0[3];
	u8 urxh;
	u8 reserved1[3];
	u32 ubrdiv;
	union s5p6818_baud_rest rest;
};

struct s5p6818_serial_plat {
	struct s5p6818_uart_regs *regs;
	ulong clock;
	u8 port_id;
	bool skip_init;
};

static const u16 s5p6818_udivslot[] = {
	0x0000, 0x0080, 0x0808, 0x0888,
	0x2222, 0x4924, 0x4a52, 0x54aa,
	0x5555, 0xd555, 0xd5d5, 0xddd5,
	0xdddd, 0xdfdd, 0xdfdf, 0xffdf,
};

static void s5p6818_serial_hw_init(struct s5p6818_uart_regs *regs)
{
	writel(0x3, &regs->ufcon);
	writel(0x0, &regs->umcon);
	writel(0x3, &regs->ulcon);
	writel(0x245, &regs->ucon);
}

static int s5p6818_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct s5p6818_serial_plat *plat = dev_get_plat(dev);
	u32 divisor;

	if (plat->skip_init)
		return 0;

	if (!plat->clock || !baudrate)
		return -EINVAL;

	divisor = plat->clock / baudrate;
	writel(divisor / 16 - 1, &plat->regs->ubrdiv);
	writew(s5p6818_udivslot[divisor % 16], &plat->regs->rest.slot);

	return 0;
}

static int s5p6818_serial_probe(struct udevice *dev)
{
	struct s5p6818_serial_plat *plat = dev_get_plat(dev);
	char clock_name[16];
	struct clk *clk;
	long rate;
	int reset_id;

	if (plat->skip_init)
		return 0;

	reset_id = RESET_ID_UART0 + plat->port_id;
	if (reset_id > RESET_ID_UART5)
		return -EINVAL;

	snprintf(clock_name, sizeof(clock_name), "nx-uart.%u", plat->port_id);
	clk = clk_get(clock_name);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	mdelay(1);
	nx_rstcon_setrst(reset_id, RSTCON_ASSERT);
	udelay(10);
	nx_rstcon_setrst(reset_id, RSTCON_NEGATE);
	udelay(10);

	clk_disable(clk);
	rate = clk_set_rate(clk, plat->clock);
	if (rate <= 0)
		return -EINVAL;
	if (clk_enable(clk))
		return -EIO;
	plat->clock = rate;

	s5p6818_serial_hw_init(plat->regs);

	return 0;
}

static int s5p6818_serial_getc(struct udevice *dev)
{
	struct s5p6818_serial_plat *plat = dev_get_plat(dev);
	u32 status = readl(&plat->regs->ufstat);

	if (!(status & (S5P6818_RX_FIFO_COUNT_MASK | S5P6818_RX_FIFO_FULL)))
		return -EAGAIN;

	readl(&plat->regs->uerstat);
	return readb(&plat->regs->urxh);
}

static int s5p6818_serial_putc(struct udevice *dev, const char ch)
{
	struct s5p6818_serial_plat *plat = dev_get_plat(dev);

	if (readl(&plat->regs->ufstat) & S5P6818_TX_FIFO_FULL)
		return -EAGAIN;

	writeb(ch, &plat->regs->utxh);
	return 0;
}

static int s5p6818_serial_pending(struct udevice *dev, bool input)
{
	struct s5p6818_serial_plat *plat = dev_get_plat(dev);
	u32 status = readl(&plat->regs->ufstat);

	if (input)
		return status & (S5P6818_RX_FIFO_COUNT_MASK |
				 S5P6818_RX_FIFO_FULL);

	return (status & S5P6818_TX_FIFO_COUNT_MASK) >> 16;
}

static int s5p6818_serial_of_to_plat(struct udevice *dev)
{
	struct s5p6818_serial_plat *plat = dev_get_plat(dev);

	plat->regs = dev_read_addr_ptr(dev);
	if (!plat->regs)
		return -EINVAL;

	plat->clock = dev_read_u32_default(dev, "clock-frequency", 50000000);
	plat->port_id = dev_read_u32_default(dev, "id", dev_seq(dev));
	plat->skip_init = dev_read_bool(dev, "skip-init");

	return 0;
}

static const struct dm_serial_ops s5p6818_serial_ops = {
	.putc = s5p6818_serial_putc,
	.pending = s5p6818_serial_pending,
	.getc = s5p6818_serial_getc,
	.setbrg = s5p6818_serial_setbrg,
};

static const struct udevice_id s5p6818_serial_ids[] = {
	{ .compatible = "nexell,s5p6818-uart" },
	{ }
};

U_BOOT_DRIVER(serial_s5p6818) = {
	.name = "serial_s5p6818",
	.id = UCLASS_SERIAL,
	.of_match = s5p6818_serial_ids,
	.of_to_plat = s5p6818_serial_of_to_plat,
	.plat_auto = sizeof(struct s5p6818_serial_plat),
	.probe = s5p6818_serial_probe,
	.ops = &s5p6818_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

#ifdef CONFIG_DEBUG_UART_S5P6818

#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
	struct s5p6818_uart_regs *regs =
		(struct s5p6818_uart_regs *)CONFIG_VAL(DEBUG_UART_BASE);
	u32 divisor;

	if (IS_ENABLED(CONFIG_DEBUG_UART_SKIP_INIT))
		return;

	s5p6818_serial_hw_init(regs);
	divisor = CONFIG_DEBUG_UART_CLOCK / CONFIG_BAUDRATE;
	writel(divisor / 16 - 1, &regs->ubrdiv);
	writew(s5p6818_udivslot[divisor % 16], &regs->rest.slot);
}

static inline void _debug_uart_putc(int ch)
{
	struct s5p6818_uart_regs *regs =
		(struct s5p6818_uart_regs *)CONFIG_VAL(DEBUG_UART_BASE);

	while (readl(&regs->ufstat) & S5P6818_TX_FIFO_FULL)
		;
	writeb(ch, &regs->utxh);
}

DEBUG_UART_FUNCS

#endif
