// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 Samsung Electronics
 *
 * Donghwa Lee <dh09.lee@samsung.com>
 */

#include <config.h>
#include <errno.h>
#include <linux/err.h>
#include <stdio.h>
#include <asm/io.h>
#include <asm/arch/pwm.h>
#include <asm/arch/clk.h>
#if defined(CONFIG_ARCH_NEXELL)
#include <asm/arch/nexell.h>
#include <asm/arch/reset.h>
#endif

#if defined(CONFIG_ARCH_NEXELL)
static struct clk *s5p_pwm_get_clk(int pwm_id)
{
	char name[16];

	snprintf(name, sizeof(name), "%s.%d", DEV_NAME_PWM, pwm_id);
	return clk_get(name);
}

/* The vendor PWM path releases reset and enables CLKGEN13 before touching PWM. */
static int s5p_pwm_prepare(int pwm_id)
{
	struct clk *clk;
	int rate;

	clk = s5p_pwm_get_clk(pwm_id);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	nx_rstcon_setrst(RESET_ID_PWM, RSTCON_ASSERT);
	nx_rstcon_setrst(RESET_ID_PWM, RSTCON_NEGATE);

	/* 25 MHz gives a 3.125 MHz PWM input after the /8 prescaler. */
	rate = clk_set_rate(clk, 25000000);
	if (rate < 0)
		return rate;

	return clk_enable(clk);
}
#endif

int s5p_pwm_enable(int pwm_id)
{
	const struct s5p_timer *pwm =
#if defined(CONFIG_ARCH_NEXELL)
			(struct s5p_timer *)PHY_BASEADDR_PWM;
#else
			(struct s5p_timer *)samsung_get_base_timer();
#endif
	unsigned long tcon;

	tcon = readl(&pwm->tcon);
	tcon |= TCON_START(pwm_id);

	writel(tcon, &pwm->tcon);

	return 0;
}

void s5p_pwm_disable(int pwm_id)
{
	const struct s5p_timer *pwm =
#if defined(CONFIG_ARCH_NEXELL)
			(struct s5p_timer *)PHY_BASEADDR_PWM;
#else
			(struct s5p_timer *)samsung_get_base_timer();
#endif
	unsigned long tcon;

	tcon = readl(&pwm->tcon);
	tcon &= ~TCON_START(pwm_id);

	writel(tcon, &pwm->tcon);
}

static unsigned long pwm_calc_tin(int pwm_id, unsigned long freq)
{
	unsigned long tin_parent_rate;
	unsigned int div;

#if defined(CONFIG_ARCH_NEXELL)
	unsigned int pre_div;
	const struct s5p_timer *pwm =
		(struct s5p_timer *)PHY_BASEADDR_PWM;
	unsigned int val;
	struct clk *clk = s5p_pwm_get_clk(pwm_id);

	if (IS_ERR(clk))
		return 0;

	tin_parent_rate = clk_get_rate(clk);
#else
	tin_parent_rate = get_pwm_clk();
#endif

#if defined(CONFIG_ARCH_NEXELL)
	val = readl(&pwm->tcfg0);

	if (pwm_id < 2)
		div = ((val >> 0) & 0xff) + 1;
	else
		div = ((val >> 8) & 0xff) + 1;

	val = readl(&pwm->tcfg1);
	val = (val >> MUX_DIV_SHIFT(pwm_id)) & 0xF;
	pre_div = (1UL << val);

	if (!tin_parent_rate)
		return 0;

	freq = tin_parent_rate / div / pre_div;

	return freq;
#else
	for (div = 2; div <= 16; div *= 2) {
		if ((tin_parent_rate / (div << 16)) < freq)
			return tin_parent_rate / div;
	}

	return tin_parent_rate / 16;
#endif
}

#define NS_IN_SEC 1000000000UL

int s5p_pwm_config(int pwm_id, int duty_ns, int period_ns)
{
	const struct s5p_timer *pwm =
#if defined(CONFIG_ARCH_NEXELL)
			(struct s5p_timer *)PHY_BASEADDR_PWM;
#else
			(struct s5p_timer *)samsung_get_base_timer();
#endif
	unsigned int offset;
	unsigned long tin_rate;
	unsigned long tin_ns;
	unsigned long frequency;
	unsigned long tcon;
	unsigned long tcnt;
	unsigned long tcmp;

	/*
	 * We currently avoid using 64bit arithmetic by using the
	 * fact that anything faster than 1GHz is easily representable
	 * by 32bits.
	 */
	if (period_ns > NS_IN_SEC || duty_ns > NS_IN_SEC || period_ns == 0)
		return -ERANGE;

	if (duty_ns > period_ns)
		return -EINVAL;

	frequency = NS_IN_SEC / period_ns;

	/* Check to see if we are changing the clock rate of the PWM */
	tin_rate = pwm_calc_tin(pwm_id, frequency);
	if (!tin_rate)
		return -EINVAL;

	tin_ns = NS_IN_SEC / tin_rate;
	if (!tin_ns)
		return -ERANGE;

	if (IS_ENABLED(CONFIG_ARCH_NEXELL))
		/* The counter starts at zero. */
		tcnt = (period_ns / tin_ns) - 1;
	else
		tcnt = period_ns / tin_ns;

	/* Note, counters count down */
	tcmp = duty_ns / tin_ns;
	tcmp = tcnt - tcmp;

	/* Update the PWM register block. */
	offset = pwm_id * 3;
	if (pwm_id < 4) {
		writel(tcnt, &pwm->tcntb0 + offset);
		writel(tcmp, &pwm->tcmpb0 + offset);
	}

	tcon = readl(&pwm->tcon);
	tcon |= TCON_UPDATE(pwm_id);
	if (pwm_id < 4)
		tcon |= TCON_AUTO_RELOAD(pwm_id);
	else
		tcon |= TCON4_AUTO_RELOAD;
	writel(tcon, &pwm->tcon);

	tcon &= ~TCON_UPDATE(pwm_id);
	writel(tcon, &pwm->tcon);

	return 0;
}

int s5p_pwm_init(int pwm_id, int div, int invert)
{
	u32 val;
	const struct s5p_timer *pwm =
#if defined(CONFIG_ARCH_NEXELL)
			(struct s5p_timer *)PHY_BASEADDR_PWM;
#else
			(struct s5p_timer *)samsung_get_base_timer();
#endif
	unsigned long ticks_per_period;
	unsigned int offset, prescaler;

#if defined(CONFIG_ARCH_NEXELL)
	int ret = s5p_pwm_prepare(pwm_id);

	if (ret)
		return ret;
#endif

	/*
	 * Timer Freq(HZ) =
	 *	PWM_CLK / { (prescaler_value + 1) * (divider_value) }
	 */

	val = readl(&pwm->tcfg0);
	if (pwm_id < 2) {
		prescaler = PRESCALER_0;
		val &= ~0xff;
		val |= (prescaler & 0xff);
	} else {
		prescaler = PRESCALER_1;
		val &= ~(0xff << 8);
		val |= (prescaler & 0xff) << 8;
	}
	writel(val, &pwm->tcfg0);
	val = readl(&pwm->tcfg1);
	val &= ~(0xf << MUX_DIV_SHIFT(pwm_id));
	val |= (div & 0xf) << MUX_DIV_SHIFT(pwm_id);
	writel(val, &pwm->tcfg1);

	if (pwm_id == 4) {
		/*
		 * TODO(sjg): Use this as a countdown timer for now. We count
		 * down from the maximum value to 0, then reset.
		 */
		ticks_per_period = -1UL;
	} else {
		const unsigned long pwm_hz = 1000;
#if defined(CONFIG_ARCH_NEXELL)
		struct clk *clk = s5p_pwm_get_clk(pwm_id);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
		unsigned long timer_rate_hz = clk_get_rate(clk) /
#else
		unsigned long timer_rate_hz = get_pwm_clk() /
#endif
			((prescaler + 1) * (1 << div));

		ticks_per_period = timer_rate_hz / pwm_hz;
	}

	/* set count value */
	offset = pwm_id * 3;

	writel(ticks_per_period, &pwm->tcntb0 + offset);

	val = readl(&pwm->tcon) & ~(0xf << TCON_OFFSET(pwm_id));
	if (invert && (pwm_id < 4))
		val |= TCON_INVERTER(pwm_id);
	writel(val, &pwm->tcon);

	s5p_pwm_enable(pwm_id);

	return 0;
}
