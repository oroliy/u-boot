// SPDX-License-Identifier: GPL-2.0+
/* Nexell S5P6818 GMAC glue for the generic Designware Ethernet driver. */

#include <dm.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <stdio.h>
#include <asm/io.h>
#include <phy.h>

#include <asm/arch/nexell.h>
#include <asm/arch/nx_gpio.h>
#include <asm/arch/reset.h>

#include "designware.h"

static void nexell_gmac_config_pad(
	volatile struct nx_gpio_register_set *gpio, unsigned int bit,
	unsigned int function, unsigned int drive)
{
	u32 mask = BIT(bit);
	u32 shift = (bit & 0xf) * 2;
	u32 altfn;

	altfn = readl(&gpio->gpioxaltfn[bit / 16]);
	altfn &= ~(3U << shift);
	altfn |= function << shift;
	writel(altfn, &gpio->gpioxaltfn[bit / 16]);

	/* RGMII is a push-pull peripheral bus with no internal pulls. */
	clrbits_le32(&gpio->gpiox_pullenb, mask);
	clrbits_le32(&gpio->gpiox_pullsel, mask);

	if (drive & 1)
		setbits_le32(&gpio->gpiox_drv1, mask);
	else
		clrbits_le32(&gpio->gpiox_drv1, mask);
	if (drive & 2)
		setbits_le32(&gpio->gpiox_drv0, mask);
	else
		clrbits_le32(&gpio->gpiox_drv0, mask);
}

static void nexell_gmac_pins_init(void)
{
	volatile struct nx_gpio_register_set *gpioe =
		(void *)PHY_BASEADDR_GPIOE;
	static const unsigned int rgmii_pins[] = {
		7, 8, 9, 10, 11,
		14, 15, 16, 17, 18, 19, 20, 21, 24,
	};
	unsigned int i;

	for (i = 0; i < sizeof(rgmii_pins) / sizeof(rgmii_pins[0]); i++)
		nexell_gmac_config_pad(gpioe, rgmii_pins[i], 1, 3);

	/* E22 is the board-level active-low RTL8211 reset, not GMAC RXER. */
	nexell_gmac_config_pad(gpioe, 22, 0, 3);
	setbits_le32(&gpioe->gpioxout, BIT(22));
	setbits_le32(&gpioe->gpioxoutenb, BIT(22));

	printf("x6818: GMAC pins ALT1 E7-E11/E14-E21/E24, reset E22 GPIO "
	       "alt0=0x%08x alt1=0x%08x drv0=0x%08x drv1=0x%08x\n",
	       readl(&gpioe->gpioxaltfn[0]), readl(&gpioe->gpioxaltfn[1]),
	       readl(&gpioe->gpiox_drv0), readl(&gpioe->gpiox_drv1));
}

static void nexell_gmac_phy_reset(void)
{
	volatile struct nx_gpio_register_set *gpioe =
		(void *)PHY_BASEADDR_GPIOE;

	/*
	 * RTL8211E requires PHYRSTB low for at least 10 ms so its internal
	 * regulator is reset, followed by 30 ms for the PHY circuits to settle.
	 */
	setbits_le32(&gpioe->gpioxout, BIT(22));
	udelay(100);
	clrbits_le32(&gpioe->gpioxout, BIT(22));
	mdelay(10);
	setbits_le32(&gpioe->gpioxout, BIT(22));
	mdelay(30);
}

static void nexell_gmac_reset(void)
{
	/* Match the vendor Linux sequence: release, assert, release. */
	nx_rstcon_setrst(RESET_ID_DWC_GMAC, RSTCON_NEGATE);
	udelay(100);
	nx_rstcon_setrst(RESET_ID_DWC_GMAC, RSTCON_ASSERT);
	udelay(100);
	nx_rstcon_setrst(RESET_ID_DWC_GMAC, RSTCON_NEGATE);
	udelay(100);
}

static void nexell_gmac_clock_init(void)
{
	void __iomem *base = (void __iomem *)PHY_BASEADDR_CLKGEN10;
	u32 val;

	/* Vendor code selects external GMAC RX_CLK, divisor 1, no inversion. */
	val = readl(base + 0x00);
	val &= ~BIT(2);
	writel(val, base + 0x00);

	val = readl(base + 0x04);
	val &= ~((0x7U << 2) | (0xffU << 5) | BIT(1));
	val |= 4U << 2;
	writel(val, base + 0x04);

	val = readl(base + 0x00);
	val |= BIT(2);
	writel(val, base + 0x00);

	printf("x6818: GMAC clock EXT_RX clkgen0=0x%08x clkgen1=0x%08x "
	       "enb=0x%08x\n", readl(base + 0x04), readl(base + 0x0c),
	       readl(base + 0x00));
}

static int nexell_gmac_setup(void)
{
	nexell_gmac_pins_init();
	nexell_gmac_clock_init();
	nexell_gmac_reset();
	nexell_gmac_phy_reset();

	return 0;
}

static int nexell_gmac_probe(struct udevice *dev)
{
	struct dw_eth_dev *priv;
	int ret;

	ret = nexell_gmac_setup();
	if (ret)
		return ret;

	ret = designware_eth_probe(dev);
	if (ret)
		return ret;

	priv = dev_get_priv(dev);
	if (priv->phydev) {
		printf("x6818: GMAC PHY addr=%d id=0x%08x interface=%d\n",
		       priv->phydev->addr, priv->phydev->phy_id,
		       priv->phydev->interface);
		printf("x6818: GMAC/PHY probed; autoneg runs in hardware, "
		       "link check deferred to network use\n");
	}

	return 0;
}

static const struct udevice_id nexell_gmac_ids[] = {
	{ .compatible = "nexell,s5p6818-dwmac" },
	{ }
};

U_BOOT_DRIVER(dwmac_nexell) = {
	.name = "dwmac_nexell",
	.id = UCLASS_ETH,
	.of_match = nexell_gmac_ids,
	.of_to_plat = designware_eth_of_to_plat,
	.probe = nexell_gmac_probe,
	.ops = &designware_eth_ops,
	.priv_auto = sizeof(struct dw_eth_dev),
	.plat_auto = sizeof(struct dw_eth_pdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
