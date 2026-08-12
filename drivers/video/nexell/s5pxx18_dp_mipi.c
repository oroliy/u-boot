// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 *
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 */

#include <config.h>
#include <errno.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <log.h>
#include <stdio.h>

#include <asm/io.h>
#include <asm/arch/nexell.h>
#include <asm/arch/tieoff.h>
#include <asm/arch/reset.h>
#include <asm/arch/display.h>

#include "soc/s5pxx18_soc_mipi.h"
#include "soc/s5pxx18_soc_disptop.h"
#include "soc/s5pxx18_soc_disptop_clk.h"
#include "soc/s5pxx18_soc_dpc.h"
#include "soc/s5pxx18_soc_mlc.h"

#define	PLLPMS_1000MHZ		0x33E8
#define	BANDCTL_1000MHZ		0xF
#define PLLPMS_960MHZ       0x2280
#define BANDCTL_960MHZ      0xF
#define	PLLPMS_900MHZ		0x2258
#define	BANDCTL_900MHZ		0xE
#define	PLLPMS_840MHZ		0x2230
#define	BANDCTL_840MHZ		0xD
#define	PLLPMS_750MHZ		0x43E8
#define	BANDCTL_750MHZ		0xC
#define	PLLPMS_660MHZ		0x21B8
#define	BANDCTL_660MHZ		0xB
#define	PLLPMS_600MHZ		0x2190
#define	BANDCTL_600MHZ		0xA
#define	PLLPMS_540MHZ		0x2168
#define	BANDCTL_540MHZ		0x9
#define	PLLPMS_512MHZ		0x03200
#define	BANDCTL_512MHZ		0x9
#define	PLLPMS_480MHZ		0x2281
#define	BANDCTL_480MHZ		0x8
#define	PLLPMS_420MHZ		0x2231
#define	BANDCTL_420MHZ		0x7
#define	PLLPMS_402MHZ		0x2219
#define	BANDCTL_402MHZ		0x7
#define	PLLPMS_330MHZ		0x21B9
#define	BANDCTL_330MHZ		0x6
#define	PLLPMS_300MHZ		0x2191
#define	BANDCTL_300MHZ		0x5
#define	PLLPMS_210MHZ		0x2232
#define	BANDCTL_210MHZ		0x4
#define	PLLPMS_180MHZ		0x21E2
#define	BANDCTL_180MHZ		0x3
#define	PLLPMS_150MHZ		0x2192
#define	BANDCTL_150MHZ		0x2
#define	PLLPMS_100MHZ		0x3323
#define	BANDCTL_100MHZ		0x1
#define	PLLPMS_80MHZ		0x3283
#define	BANDCTL_80MHZ		0x0

#define	MIPI_INDEX		0
#define MIPI_ESC_PRE_VALUE	1
#define MIPI_DSI_IRQ_MASK       29

#define	__io_address(a)	(void *)(uintptr_t)(a)

struct mipi_xfer_msg {
	u8 id, data[2];
	u16 flags;
	const u8 *tx_buf;
	u16 tx_len;
	u8 *rx_buf;
	u16 rx_len;
};

static void mipi_reset(void)
{
	/* tieoff */
	nx_tieoff_set(NX_TIEOFF_MIPI0_NX_DPSRAM_1R1W_EMAA, 3);
	nx_tieoff_set(NX_TIEOFF_MIPI0_NX_DPSRAM_1R1W_EMAB, 3);

	/* reset */
	nx_rstcon_setrst(RESET_ID_MIPI, RSTCON_ASSERT);
	nx_rstcon_setrst(RESET_ID_MIPI_DSI, RSTCON_ASSERT);
	nx_rstcon_setrst(RESET_ID_MIPI_CSI, RSTCON_ASSERT);
	nx_rstcon_setrst(RESET_ID_MIPI_PHY_S, RSTCON_ASSERT);
	nx_rstcon_setrst(RESET_ID_MIPI_PHY_M, RSTCON_ASSERT);

	nx_rstcon_setrst(RESET_ID_MIPI, RSTCON_NEGATE);
	nx_rstcon_setrst(RESET_ID_MIPI_DSI, RSTCON_NEGATE);
	nx_rstcon_setrst(RESET_ID_MIPI_CSI, RSTCON_NEGATE);
	nx_rstcon_setrst(RESET_ID_MIPI_PHY_S, RSTCON_NEGATE);
	nx_rstcon_setrst(RESET_ID_MIPI_PHY_M, RSTCON_NEGATE);
}

static void mipi_init(void)
{
	int clkid = DP_CLOCK_MIPI;
	void *base;
	struct nx_mipi_register_set *regs;

	/*
	 * neet to reset before open
	 */
	mipi_reset();

	base = __io_address(nx_disp_top_clkgen_get_physical_address(clkid));
	nx_disp_top_clkgen_set_base_address(clkid, base);
	nx_disp_top_clkgen_set_clock_pclk_mode(clkid, nx_pclkmode_always);

	base = __io_address(nx_mipi_get_physical_address(0));
	nx_mipi_set_base_address(0, base);

	/* Match the vendor NX_MIPI_OpenModule() D-PHY analog setup. */
	regs = nx_mipi_get_base_address(0);
	writel(0, &regs->csis_dphyctrl_1);
	writel(22 << 24, &regs->csis_dphyctrl);
}

static int mipi_get_phy_pll(int bitrate, unsigned int *pllpms,
			    unsigned int *bandctl)
{
	unsigned int pms, ctl;

	switch (bitrate) {
	case 1000:
		pms = PLLPMS_1000MHZ;
		ctl = BANDCTL_1000MHZ;
		break;
	case 960:
		pms = PLLPMS_960MHZ;
		ctl = BANDCTL_960MHZ;
		break;
	case 900:
		pms = PLLPMS_900MHZ;
		ctl = BANDCTL_900MHZ;
		break;
	case 840:
		pms = PLLPMS_840MHZ;
		ctl = BANDCTL_840MHZ;
		break;
	case 750:
		pms = PLLPMS_750MHZ;
		ctl = BANDCTL_750MHZ;
		break;
	case 660:
		pms = PLLPMS_660MHZ;
		ctl = BANDCTL_660MHZ;
		break;
	case 600:
		pms = PLLPMS_600MHZ;
		ctl = BANDCTL_600MHZ;
		break;
	case 540:
		pms = PLLPMS_540MHZ;
		ctl = BANDCTL_540MHZ;
		break;
	case 512:
		pms = PLLPMS_512MHZ;
		ctl = BANDCTL_512MHZ;
		break;
	case 480:
		pms = PLLPMS_480MHZ;
		ctl = BANDCTL_480MHZ;
		break;
	case 420:
		pms = PLLPMS_420MHZ;
		ctl = BANDCTL_420MHZ;
		break;
	case 402:
		pms = PLLPMS_402MHZ;
		ctl = BANDCTL_402MHZ;
		break;
	case 330:
		pms = PLLPMS_330MHZ;
		ctl = BANDCTL_330MHZ;
		break;
	case 300:
		pms = PLLPMS_300MHZ;
		ctl = BANDCTL_300MHZ;
		break;
	case 210:
		pms = PLLPMS_210MHZ;
		ctl = BANDCTL_210MHZ;
		break;
	case 180:
		pms = PLLPMS_180MHZ;
		ctl = BANDCTL_180MHZ;
		break;
	case 150:
		pms = PLLPMS_150MHZ;
		ctl = BANDCTL_150MHZ;
		break;
	case 100:
		pms = PLLPMS_100MHZ;
		ctl = BANDCTL_100MHZ;
		break;
	case 80:
		pms = PLLPMS_80MHZ;
		ctl = BANDCTL_80MHZ;
		break;
	default:
		return -EINVAL;
	}

	*pllpms = pms;
	*bandctl = ctl;

	return 0;
}

static int mipi_set_phy_lanes(int index, unsigned int lanes)
{
	switch (lanes) {
	case 1:
		nx_mipi_dsi_set_phy(index, 0, 1, 1, 0, 0, 0, 0, 0);
		break;
	case 2:
		nx_mipi_dsi_set_phy(index, 1, 1, 1, 1, 0, 0, 0, 0);
		break;
	case 3:
		nx_mipi_dsi_set_phy(index, 2, 1, 1, 1, 1, 0, 0, 0);
		break;
	case 4:
		nx_mipi_dsi_set_phy(index, 3, 1, 1, 1, 1, 1, 0, 0);
		break;
	default:
		printf("%s: not support data lanes %u\n", __func__, lanes);
		return -EINVAL;
	}

	return 0;
}

static int mipi_set_video_mode(int index, struct dp_sync_info *sync,
			       struct mipi_dsi_device *dsi)
{
	enum nx_mipi_dsi_format dsi_format;
	bool burst = dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST;
	bool eot_enable = !(dsi->mode_flags & MIPI_DSI_MODE_EOT_PACKET);

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB565:
		dsi_format = nx_mipi_dsi_format_rgb565;
		break;
	case MIPI_DSI_FMT_RGB666:
		dsi_format = nx_mipi_dsi_format_rgb666;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		dsi_format = nx_mipi_dsi_format_rgb666_packed;
		break;
	case MIPI_DSI_FMT_RGB888:
		dsi_format = nx_mipi_dsi_format_rgb888;
		break;
	default:
		printf("%s: not support format %d\n", __func__, dsi->format);
		return -EINVAL;
	}

	nx_mipi_dsi_set_config_video_mode(index, 1, 0, burst,
					  nx_mipi_dsi_syncmode_event,
					  eot_enable, 1, 1, 1, 1, 0,
					  dsi_format,
					  sync->h_front_porch,
					  sync->h_back_porch,
					  sync->h_sync_width,
					  sync->v_front_porch,
					  sync->v_back_porch,
					  sync->v_sync_width, 0);

	return 0;
}

static int mipi_prepare(int module, int input,
			struct dp_sync_info *sync, struct dp_ctrl_info *ctrl,
			struct dp_mipi_dev *mipi)
{
	int index = MIPI_INDEX;
	u32 esc_pre_value = MIPI_ESC_PRE_VALUE;
	int lpm = mipi->lpm_trans;
	int ret = 0;

	ret = mipi_get_phy_pll(mipi->hs_bitrate,
			       &mipi->hs_pllpms, &mipi->hs_bandctl);
	if (ret < 0)
		return ret;

	ret = mipi_get_phy_pll(mipi->lp_bitrate,
			       &mipi->lp_pllpms, &mipi->lp_bandctl);
	if (ret < 0)
		return ret;

	debug("%s: mipi lp:%dmhz:0x%x:0x%x, hs:%dmhz:0x%x:0x%x, %s trans\n",
	      __func__, mipi->lp_bitrate, mipi->lp_pllpms, mipi->lp_bandctl,
	      mipi->hs_bitrate, mipi->hs_pllpms, mipi->hs_bandctl,
	      lpm ? "low" : "high");

	/* Match the vendor S5P6818 sequence used for panel DCS commands. */
	if (lpm)
		nx_mipi_dsi_set_pll(index, 1, 0xFFFFFFFF,
				mipi->lp_pllpms, mipi->lp_bandctl, 0, 0);
	else
		nx_mipi_dsi_set_pll(index, 1, 0xFFFFFFFF,
				mipi->hs_pllpms, mipi->hs_bandctl, 0, 0);
	mdelay(20);

#ifdef CONFIG_ARCH_S5P4418
	/*
	 * disable the escape clock generating prescaler
	 * before soft reset.
	 */
	nx_mipi_dsi_set_clock(index, 0, 0, 1, 1, 1, 0, 0, 0, 0, 10);
	mdelay(1);
#endif

	nx_mipi_dsi_software_reset(index);
	nx_mipi_dsi_set_clock(index, 0, 0, 1, 1, 1, 0, 0, 0, 1,
			      esc_pre_value);
	nx_mipi_dsi_set_phy(index, 0, 1, 1, 0, 0, 0, 0, 0);

	if (lpm)
		nx_mipi_dsi_set_escape_lp(index, nx_mipi_dsi_lpmode_lp,
					  nx_mipi_dsi_lpmode_lp);
	else
		nx_mipi_dsi_set_escape_lp(index, nx_mipi_dsi_lpmode_hs,
					  nx_mipi_dsi_lpmode_hs);
	mdelay(20);

	return 0;
}

static int mipi_enable(int module, int input,
		       struct dp_sync_info *sync, struct dp_ctrl_info *ctrl,
		       struct dp_mipi_dev *mipi)
{
	struct mipi_dsi_device *dsi = &mipi->dsi;
	int clkid = DP_CLOCK_MIPI;
	int index = MIPI_INDEX;
	int width = sync->h_active_len;
	int height = sync->v_active_len;
	u32 esc_pre_value = MIPI_ESC_PRE_VALUE;
	int en_prescaler = 1;

	int lpm = mipi->lpm_trans;
	bool command_mode = mipi->command_mode;
	int ret;

	/*
	 * disable the escape clock generating prescaler
	 * before soft reset.
	 */
#ifdef CONFIG_ARCH_S5P4418
	en_prescaler = 0;
#endif

	debug("%s: mode:%s, lanes.%d\n", __func__,
	      command_mode ? "command" : "video", dsi->lanes);

	if (lpm)
		nx_mipi_dsi_set_escape_lp(index,
					  nx_mipi_dsi_lpmode_hs,
					  nx_mipi_dsi_lpmode_hs);

	nx_mipi_dsi_set_pll(index, 1, 0xFFFFFFFF,
			    mipi->hs_pllpms, mipi->hs_bandctl, 0, 0);
	mdelay(1);

	nx_mipi_dsi_set_clock(index, 0, 0, 1, 1, 1, 0, 0, 0, en_prescaler, 10);
	mdelay(1);

	nx_mipi_dsi_software_reset(index);
	nx_mipi_dsi_set_clock(index, 1, 0, 1,
			      1, 1, 1, 1, 1, 1, esc_pre_value);

	ret = mipi_set_phy_lanes(index, dsi->lanes);
	if (ret)
		return ret;

	ret = mipi_set_video_mode(index, sync, dsi);
	if (ret)
		return ret;

	nx_mipi_dsi_set_size(index, width, height);

	/* set mux */
	nx_disp_top_set_mipimux(1, module);

	/* 0 is SPDIF, 1 is the MIPI video clock. */
	nx_disp_top_clkgen_set_clock_source(clkid, 1, ctrl->clk_src_lv0);
	nx_disp_top_clkgen_set_clock_divisor(clkid, 1,
					     ctrl->clk_div_lv1 * ctrl->clk_div_lv0);
	nx_disp_top_clkgen_set_clock_divisor_enable(clkid, 1);

	/* START: CLKGEN, MIPI is started in setup function */
	nx_disp_top_clkgen_set_clock_divisor_enable(clkid, true);
	nx_mipi_dsi_set_enable(index, true);

	return 0;
}

static int nx_mipi_transfer_tx(struct mipi_dsi_device *dsi,
			       struct mipi_xfer_msg *xfer)
{
	const u8 *txb;
	int size, index = 0;
	u32 data;

	if (xfer->tx_len > DSI_TX_FIFO_SIZE)
		printf("warn: tx %d size over fifo %d\n",
		       (int)xfer->tx_len, DSI_TX_FIFO_SIZE);

	/* write payload */
	size = xfer->tx_len;
	txb = xfer->tx_buf;

	while (size >= 4) {
		data = (txb[3] << 24) | (txb[2] << 16) |
		    (txb[1] << 8) | (txb[0]);
		nx_mipi_dsi_write_payload(index, data);
		txb += 4, size -= 4;
		data = 0;
	}

	switch (size) {
	case 3:
		data |= txb[2] << 16;
	case 2:
		data |= txb[1] << 8;
	case 1:
		data |= txb[0];
		nx_mipi_dsi_write_payload(index, data);
		break;
	case 0:
		break;		/* no payload */
	}

	/* write packet hdr */
	data = (xfer->data[1] << 16) | (xfer->data[0] << 8) | xfer->id;

	nx_mipi_dsi_write_pkheader(index, data);

	return 0;
}

static int nx_mipi_transfer_done(struct mipi_dsi_device *dsi)
{
	int index = 0, count = 100;
	u32 value;

	do {
		mdelay(1);
		value = nx_mipi_dsi_read_fifo_status(index);
		if (((1 << 22) & value))
			break;
	} while (count-- > 0);

	if (count < 0)
		return -EINVAL;

	return 0;
}

static int nx_mipi_transfer_rx(struct mipi_dsi_device *dsi,
			       struct mipi_xfer_msg *xfer)
{
	u8 *rxb = xfer->rx_buf;
	int index = 0, rx_len = 0;
	u32 data, count = 0;
	u16 size;
	int err = -EINVAL;

	nx_mipi_dsi_clear_interrupt_pending(index, 18);

	while (1) {
		/* Completes receiving data. */
		if (nx_mipi_dsi_get_interrupt_pending(index, 18))
			break;

		mdelay(1);

		if (count > 500) {
			printf("%s: error recevice data\n", __func__);
			err = -EINVAL;
			goto clear_fifo;
		} else {
			count++;
		}
	}

	data = nx_mipi_dsi_read_fifo(index);

	switch (data & 0x3f) {
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
		if (xfer->rx_len >= 2) {
			rxb[1] = data >> 16;
			rx_len++;
		}

		/* Fall through */
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
		rxb[0] = data >> 8;
		rx_len++;
		xfer->rx_len = rx_len;
		err = rx_len;
		goto clear_fifo;

	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		printf("DSI Error Report: 0x%04x\n", (data >> 8) & 0xffff);
		err = rx_len;
		goto clear_fifo;
	}

	size = (data >> 8) & 0xffff;

	if (size > xfer->rx_len)
		size = xfer->rx_len;
	else if (size < xfer->rx_len)
		xfer->rx_len = size;

	size = xfer->rx_len - rx_len;
	rx_len += size;

	/* Receive payload */
	while (size >= 4) {
		data = nx_mipi_dsi_read_fifo(index);
		rxb[0] = (data >> 0) & 0xff;
		rxb[1] = (data >> 8) & 0xff;
		rxb[2] = (data >> 16) & 0xff;
		rxb[3] = (data >> 24) & 0xff;
		rxb += 4, size -= 4;
	}

	if (size) {
		data = nx_mipi_dsi_read_fifo(index);
		switch (size) {
		case 3:
			rxb[2] = (data >> 16) & 0xff;
		case 2:
			rxb[1] = (data >> 8) & 0xff;
		case 1:
			rxb[0] = data & 0xff;
		}
	}

	if (rx_len == xfer->rx_len)
		err = rx_len;

clear_fifo:
	size = DSI_RX_FIFO_SIZE / 4;
	do {
		data = nx_mipi_dsi_read_fifo(index);
		if (data == DSI_RX_FIFO_EMPTY)
			break;
	} while (--size);

	return err;
}

#define	IS_SHORT(t)	(9 > ((t) & 0x0f))

static int nx_mipi_transfer(struct mipi_dsi_device *dsi,
			    const struct mipi_dsi_msg *msg)
{
	struct mipi_xfer_msg xfer;
	int err, done;

	if (!msg->tx_len)
		return -EINVAL;

	/* set id */
	xfer.id = msg->type | (msg->channel << 6);

	/* short type msg */
	if (IS_SHORT(msg->type)) {
		const char *txb = msg->tx_buf;

		if (msg->tx_len > 2)
			return -EINVAL;

		xfer.tx_len = 0;	/* no payload */
		xfer.data[0] = txb[0];
		xfer.data[1] = (msg->tx_len == 2) ? txb[1] : 0;
		xfer.tx_buf = NULL;
	} else {
		xfer.tx_len = msg->tx_len;
		xfer.data[0] = msg->tx_len & 0xff;
		xfer.data[1] = msg->tx_len >> 8;
		xfer.tx_buf = msg->tx_buf;
	}

	xfer.rx_len = msg->rx_len;
	xfer.rx_buf = msg->rx_buf;
	xfer.flags = msg->flags;

	err = nx_mipi_transfer_tx(dsi, &xfer);

	if (xfer.rx_len)
		err = nx_mipi_transfer_rx(dsi, &xfer);

	done = nx_mipi_transfer_done(dsi);
	if (done < 0 && err >= 0)
		err = done;

	return err;
}

static ssize_t nx_mipi_write_buffer(struct mipi_dsi_device *dsi,
					    const void *data, size_t len)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.tx_buf = data,
		.tx_len = len
	};

	switch (len) {
	case 0:
		return -EINVAL;
	case 1:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE;
		break;
	case 2:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;
	default:
		msg.type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	if (dsi->mode_flags & MIPI_DSI_MODE_LPM)
		msg.flags |= MIPI_DSI_MSG_USE_LPM;

	{
		int ret = nx_mipi_transfer(dsi, &msg);

		/* Panel callbacks use the Linux write_buffer byte-count contract. */
		return ret < 0 ? ret : (ssize_t)len;
	}
}

static void nx_mipi_print_state(int module)
{
	struct nx_mipi_register_set *regs;
	struct nx_disp_top_register_set *top_regs;
	struct nx_disptop_clkgen_register_set *mipi_clkgen;
	struct nx_dpc_register_set *dpc_regs;
	struct nx_mlc_register_set *mlc_regs;
	s32 hstride, vstride;
	u32 fb_addr;
	u32 status;
	u32 pll_stable;
	u32 in_reset;
	u32 hs_clock_ready;

	regs = nx_mipi_get_base_address(MIPI_INDEX);
	if (!regs)
		return;

	mipi_clkgen = nx_disp_top_clkgen_get_base_address(to_mipi_clkgen);
	dpc_regs = nx_dpc_get_base_address(module);
	mlc_regs = nx_mlc_get_base_address(module);

	status = readl(&regs->dsim_status);
	nx_mipi_dsi_get_status(MIPI_INDEX, NULL, NULL, &pll_stable,
			       &in_reset, NULL, &hs_clock_ready);
	printf("MIPI: dp.%d active status=0x%08x pll=%u reset=%u hs_ready=%u "
	       "clk=0x%08x cfg=0x%08x esc=0x%08x res=0x%08x\n",
	       module, status, pll_stable, in_reset, hs_clock_ready,
	       readl(&regs->dsim_clkctrl), readl(&regs->dsim_config),
	       readl(&regs->dsim_escmode), readl(&regs->dsim_mdresol));

	nx_mlc_get_rgblayer_address(module, 0, &fb_addr);
	nx_mlc_get_rgblayer_stride(module, 0, &hstride, &vstride);
	top_regs = nx_disp_top_get_base_address();
	printf("MIPI: MLC top=%d layer0=%d dirty=%d primary=%u addr=0x%08x "
	       "stride=%d/%d DPC enable=%d clk=%d src=%u/%u div=%u/%u "
	       "mux=0x%08x\n",
	       nx_mlc_get_mlc_enable(module),
	       nx_mlc_get_layer_enable(module, 0),
	       nx_mlc_get_dirty_flag(module, 0),
	       top_regs ? readl(&top_regs->tftmpu_mux) : 0,
	       fb_addr,
	       hstride, vstride, nx_dpc_get_dpc_enable(module),
	       nx_dpc_get_clock_divisor_enable(module),
	       nx_dpc_get_clock_source(module, 0),
	       nx_dpc_get_clock_source(module, 1),
	       nx_dpc_get_clock_divisor(module, 0),
	       nx_dpc_get_clock_divisor(module, 1),
	       top_regs ? readl(&top_regs->mipi_mux_ctrl) : 0);
	printf("MIPI: raw dphy=0x%08x/0x%08x pll2=0x%08x sscg2=0x%08x dvo5=0x%08x "
	       "clkgen enb=0x%08x c0=0x%08x c1=0x%08x c2=0x%08x c3=0x%08x\n",
	       regs ? readl(&regs->csis_dphyctrl) : 0,
	       regs ? readl(&regs->csis_dphyctrl_1) : 0,
	       readl(__io_address(PHY_BASEADDR_CLKPWR + 0x010)),
	       readl(__io_address(PHY_BASEADDR_CLKPWR + 0x050)),
	       readl(__io_address(PHY_BASEADDR_CLKPWR + 0x034)),
	       mipi_clkgen ? readl(&mipi_clkgen->clkenb) : 0,
	       mipi_clkgen ? readl(&mipi_clkgen->CLKGEN[0]) : 0,
	       mipi_clkgen ? readl(&mipi_clkgen->CLKGEN[1]) : 0,
	       mipi_clkgen ? readl(&mipi_clkgen->CLKGEN[2]) : 0,
	       mipi_clkgen ? readl(&mipi_clkgen->CLKGEN[3]) : 0);
	printf("MIPI: raw DPC ctrl=0x%08x/0x%08x/0x%08x "
	       "ht=0x%08x hs=0x%08x ha=0x%08x vt=0x%08x "
	       "vs=0x%08x va=0x%08x\n",
	       dpc_regs ? readl(&dpc_regs->dpcctrl0) : 0,
	       dpc_regs ? readl(&dpc_regs->dpcctrl1) : 0,
	       dpc_regs ? readl(&dpc_regs->dpcctrl2) : 0,
	       dpc_regs ? readl(&dpc_regs->dpchtotal) : 0,
	       dpc_regs ? readl(&dpc_regs->dpchswidth) : 0,
	       dpc_regs ? readl(&dpc_regs->dpchastart) : 0,
	       dpc_regs ? readl(&dpc_regs->dpcvtotal) : 0,
	       dpc_regs ? readl(&dpc_regs->dpcvswidth) : 0,
	       dpc_regs ? readl(&dpc_regs->dpcvastart) : 0);
	printf("MIPI: raw MLC top=0x%08x size=0x%08x layer=0x%08x "
	       "addr=0x%08x hstride=0x%08x vstride=0x%08x\n",
	       mlc_regs ? readl(&mlc_regs->mlccontrolt) : 0,
	       mlc_regs ? readl(&mlc_regs->mlcscreensize) : 0,
	       mlc_regs ? readl(&mlc_regs->mlcrgblayer[0].mlccontrol) : 0,
	       mlc_regs ? readl(&mlc_regs->mlcrgblayer[0].mlcaddress) : 0,
	       mlc_regs ? readl(&mlc_regs->mlcrgblayer[0].mlchstride) : 0,
	       mlc_regs ? readl(&mlc_regs->mlcrgblayer[0].mlcvstride) : 0);
}

__weak int nx_mipi_dsi_lcd_bind(struct mipi_dsi_device *dsi)
{
	return 0;
}

/*
 * disply
 * MIPI DSI Setting
 *		(1) Initiallize MIPI(DSIM,DPHY,PLL)
 *		(2) Initiallize LCD
 *		(3) ReInitiallize MIPI(DSIM only)
 *		(4) Turn on display(MLC,DPC,...)
 */
void nx_mipi_display(int module,
		     struct dp_sync_info *sync, struct dp_ctrl_info *ctrl,
		     struct dp_plane_top *top, struct dp_plane_info *planes,
		     struct dp_mipi_dev *dev)
{
	struct dp_plane_info *plane = planes;
	struct mipi_dsi_device *dsi = &dev->dsi;
	int input = module == 0 ? DP_DEVICE_DP0 : DP_DEVICE_DP1;
	int count = top->plane_num;
	int i = 0, ret;

	printf("MIPI: dp.%d\n", module);

	/* map mipi-dsi write callback func */
	dsi->write_buffer = nx_mipi_write_buffer;

	ret = nx_mipi_dsi_lcd_bind(dsi);
	if (ret) {
		printf("Error: bind mipi-dsi lcd driver !\n");
		return;
	}

	dp_control_init(module);
	dp_plane_init(module);

	mipi_init();

	/* set plane */
	dp_plane_screen_setup(module, top);

	for (i = 0; count > i; i++, plane++) {
		if (!plane->enable)
			continue;
		dp_plane_layer_setup(module, plane);
		dp_plane_layer_enable(module, plane, 1);
	}
	dp_plane_screen_enable(module, 1);

	/* set mipi */
	ret = mipi_prepare(module, input, sync, ctrl, dev);
	if (ret) {
		printf("MIPI: host prepare failed (%d)\n", ret);
		return;
	}

	if (dsi->ops && dsi->ops->prepare) {
		ret = dsi->ops->prepare(dsi);
		if (ret) {
			printf("MIPI: panel prepare failed (%d)\n", ret);
			return;
		}
	}

	if (dsi->ops && dsi->ops->enable) {
		ret = dsi->ops->enable(dsi);
		if (ret) {
			printf("MIPI: panel enable failed (%d)\n", ret);
			return;
		}
	}

	ret = mipi_enable(module, input, sync, ctrl, dev);
	if (ret) {
		printf("MIPI: host enable failed (%d)\n", ret);
		return;
	}

	/* set dp control */
	ret = dp_control_setup(module, sync, ctrl);
	if (ret) {
		printf("MIPI: display controller setup failed (%d)\n", ret);
		return;
	}
	dp_control_enable(module, 1);
	nx_mipi_print_state(module);
}
