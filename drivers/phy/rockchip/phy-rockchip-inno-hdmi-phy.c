/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/rockchip/cpu.h>
#include <linux/slab.h>

#define INNO_HDMI_PHY_TIMEOUT_LOOP_COUNT	1000

#define UPDATE(x, h, l)		(((x) << (l)) & GENMASK((h), (l)))

/* REG: 0x00 */
#define PRE_PLL_REFCLK_SEL_MASK			BIT(0)
#define PRE_PLL_REFCLK_SEL_PCLK			BIT(0)
#define PRE_PLL_REFCLK_SEL_OSCCLK		0
/* REG: 0x01 */
#define BYPASS_RXSENSE_EN_MASK			BIT(2)
#define BYPASS_RXSENSE_EN			BIT(2)
#define BYPASS_PWRON_EN_MASK			BIT(1)
#define BYPASS_PWRON_EN				BIT(1)
#define BYPASS_PLLPD_EN_MASK			BIT(0)
#define BYPASS_PLLPD_EN				BIT(0)
/* REG: 0x02 */
#define BYPASS_PDATA_EN_MASK			BIT(4)
#define BYPASS_PDATA_EN				BIT(4)
#define PDATAEN_MASK				BIT(0)
#define PDATAEN_DISABLE				BIT(0)
#define PDATAEN_ENABLE				0
/* REG: 0x03 */
#define BYPASS_AUTO_TERM_RES_CAL		BIT(7)
#define AUDO_TERM_RES_CAL_SPEED_14_8(x)		UPDATE(x, 6, 0)
/* REG: 0x04 */
#define AUDO_TERM_RES_CAL_SPEED_7_0(x)		UPDATE(x, 7, 0)
/* REG: 0xaa */
#define POST_PLL_CTRL_MASK			BIT(0)
#define POST_PLL_CTRL_MANUAL			BIT(0)
/* REG: 0xe0 */
#define POST_PLL_POWER_MASK			BIT(5)
#define POST_PLL_POWER_DOWN			BIT(5)
#define POST_PLL_POWER_UP			0
#define PRE_PLL_POWER_MASK			BIT(4)
#define PRE_PLL_POWER_DOWN			BIT(4)
#define PRE_PLL_POWER_UP			0
#define RXSENSE_CLK_CH_MASK			BIT(3)
#define RXSENSE_CLK_CH_ENABLE			BIT(3)
#define RXSENSE_DATA_CH2_MASK			BIT(2)
#define RXSENSE_DATA_CH2_ENABLE			BIT(2)
#define RXSENSE_DATA_CH1_MASK			BIT(1)
#define RXSENSE_DATA_CH1_ENABLE			BIT(1)
#define RXSENSE_DATA_CH0_MASK			BIT(0)
#define RXSENSE_DATA_CH0_ENABLE			BIT(0)
/* REG: 0xe1 */
#define BANDGAP_MASK				BIT(4)
#define BANDGAP_ENABLE				BIT(4)
#define BANDGAP_DISABLE				0
#define TMDS_DRIVER_MASK			GENMASK(3, 0)
#define TMDS_DRIVER_ENABLE			UPDATE(0xf, 3, 0)
#define TMDS_DRIVER_DISABLE			0
/* REG: 0xe2 */
#define PRE_PLL_FB_DIV_8_MASK			BIT(7)
#define PRE_PLL_FB_DIV_8_SHIFT			7
#define PRE_PLL_FB_DIV_8(x)			UPDATE(x, 7, 7)
#define PCLK_VCO_DIV_5_MASK			BIT(5)
#define PCLK_VCO_DIV_5_SHIFT			5
#define PCLK_VCO_DIV_5(x)			UPDATE(x, 5, 5)
#define PRE_PLL_PRE_DIV_MASK			GENMASK(4, 0)
#define PRE_PLL_PRE_DIV(x)			UPDATE(x, 4, 0)
/* REG: 0xe3 */
#define PRE_PLL_FB_DIV_7_0(x)			UPDATE(x, 7, 0)
/* REG: 0xe4 */
#define PRE_PLL_PCLK_DIV_B_MASK			GENMASK(6, 5)
#define PRE_PLL_PCLK_DIV_B_SHIFT		5
#define PRE_PLL_PCLK_DIV_B(x)			UPDATE(x, 6, 5)
#define PRE_PLL_PCLK_DIV_A_MASK			GENMASK(4, 0)
#define PRE_PLL_PCLK_DIV_A_SHIFT		0
#define PRE_PLL_PCLK_DIV_A(x)			UPDATE(x, 4, 0)
/* REG: 0xe5 */
#define PRE_PLL_PCLK_DIV_C_MASK			GENMASK(6, 5)
#define PRE_PLL_PCLK_DIV_C_SHIFT		5
#define PRE_PLL_PCLK_DIV_C(x)			UPDATE(x, 6, 5)
#define PRE_PLL_PCLK_DIV_D_MASK			GENMASK(4, 0)
#define PRE_PLL_PCLK_DIV_D_SHIFT		0
#define PRE_PLL_PCLK_DIV_D(x)			UPDATE(x, 4, 0)
/* REG: 0xe6 */
#define PRE_PLL_TMDSCLK_DIV_C_MASK		GENMASK(5, 4)
#define PRE_PLL_TMDSCLK_DIV_C(x)		UPDATE(x, 5, 4)
#define PRE_PLL_TMDSCLK_DIV_A_MASK		GENMASK(3, 2)
#define PRE_PLL_TMDSCLK_DIV_A(x)		UPDATE(x, 3, 2)
#define PRE_PLL_TMDSCLK_DIV_B_MASK		GENMASK(1, 0)
#define PRE_PLL_TMDSCLK_DIV_B(x)		UPDATE(x, 1, 0)
/* REG: 0xe8 */
#define PRE_PLL_LOCK_STATUS			BIT(0)
/* REG: 0xe9 */
#define POST_PLL_POST_DIV_EN_MASK		GENMASK(7, 6)
#define POST_PLL_POST_DIV_ENABLE		UPDATE(3, 7, 6)
#define POST_PLL_POST_DIV_DISABLE		0
#define POST_PLL_PRE_DIV_MASK			GENMASK(4, 0)
#define POST_PLL_PRE_DIV(x)			UPDATE(x, 4, 0)
/* REG: 0xea */
#define POST_PLL_FB_DIV_7_0(x)			UPDATE(x, 7, 0)
/* REG: 0xeb */
#define POST_PLL_FB_DIV_8_MASK			BIT(7)
#define POST_PLL_FB_DIV_8(x)			UPDATE(x, 7, 7)
#define POST_PLL_POST_DIV_MASK			GENMASK(5, 4)
#define POST_PLL_POST_DIV(x)			UPDATE(x, 5, 4)
#define POST_PLL_LOCK_STATUS			BIT(0)
/* REG: 0xee */
#define TMDS_CH_TA_MASK				GENMASK(7, 4)
#define TMDS_CH_TA_ENABLE			UPDATE(0xf, 7, 4)
#define TMDS_CH_TA_DISABLE			0
/* REG: 0xef */
#define TMDS_CLK_CH_TA(x)			UPDATE(x, 7, 6)
#define TMDS_DATA_CH2_TA(x)			UPDATE(x, 5, 4)
#define TMDS_DATA_CH1_TA(x)			UPDATE(x, 3, 2)
#define TMDS_DATA_CH0_TA(x)			UPDATE(x, 1, 0)
/* REG: 0xf0 */
#define TMDS_DATA_CH2_PRE_EMPHASIS_MASK		GENMASK(5, 4)
#define TMDS_DATA_CH2_PRE_EMPHASIS(x)		UPDATE(x, 5, 4)
#define TMDS_DATA_CH1_PRE_EMPHASIS_MASK		GENMASK(3, 2)
#define TMDS_DATA_CH1_PRE_EMPHASIS(x)		UPDATE(x, 3, 2)
#define TMDS_DATA_CH0_PRE_EMPHASIS_MASK		GENMASK(1, 0)
#define TMDS_DATA_CH0_PRE_EMPHASIS(x)		UPDATE(x, 1, 0)
/* REG: 0xf1 */
#define TMDS_CLK_CH_OUTPUT_SWING(x)		UPDATE(x, 7, 4)
#define TMDS_DATA_CH2_OUTPUT_SWING(x)		UPDATE(x, 3, 0)
/* REG: 0xf2 */
#define TMDS_DATA_CH1_OUTPUT_SWING(x)		UPDATE(x, 7, 4)
#define TMDS_DATA_CH0_OUTPUT_SWING(x)		UPDATE(x, 3, 0)




/* REG: 0x01 */
#define RK3328_BYPASS_RXSENSE_EN			BIT(2)
#define RK3328_BYPASS_POWERON_EN			BIT(1)
#define RK3328_BYPASS_PLLPD_EN				BIT(0)
/* REG: 0x02 */
#define RK3328_INT_POL_HIGH				BIT(7)
#define RK3328_BYPASS_PDATA_EN				BIT(4)
#define RK3328_PDATA_EN					BIT(0)
/* REG:0x05 */
#define RK3328_INT_TMDS_CLK(x)				UPDATE(x, 7, 4)
#define RK3328_INT_TMDS_D2(x)				UPDATE(x, 3, 0)
/* REG:0x07 */
#define RK3328_INT_TMDS_D1(x)				UPDATE(x, 7, 4)
#define RK3328_INT_TMDS_D0(x)				UPDATE(x, 3, 0)
/* for all RK3328_INT_TMDS_*, ESD_DET as defined in 0xc8-0xcb */
#define RK3328_INT_AGND_LOW_PULSE_LOCKED		BIT(3)
#define RK3328_INT_RXSENSE_LOW_PULSE_LOCKED		BIT(2)
#define RK3328_INT_VSS_AGND_ESD_DET			BIT(1)
#define RK3328_INT_AGND_VSS_ESD_DET			BIT(0)
/* REG: 0xa0 */
#define RK3328_PCLK_VCO_DIV_5_MASK			BIT(1)
#define RK3328_PCLK_VCO_DIV_5(x)			UPDATE(x, 1, 1)
#define RK3328_PRE_PLL_POWER_DOWN			BIT(0)
/* REG: 0xa1 */
#define RK3328_PRE_PLL_PRE_DIV_MASK			GENMASK(5, 0)
#define RK3328_PRE_PLL_PRE_DIV(x)			UPDATE(x, 5, 0)
/* REG: 0xa2 */
/* unset means center spread */
#define RK3328_SPREAD_SPECTRUM_MOD_DOWN			BIT(7)
#define RK3328_SPREAD_SPECTRUM_MOD_DISABLE		BIT(6)
#define RK3328_PRE_PLL_FRAC_DIV_DISABLE			UPDATE(3, 5, 4)
#define RK3328_PRE_PLL_FB_DIV_11_8_MASK			GENMASK(3, 0)
#define RK3328_PRE_PLL_FB_DIV_11_8(x)			UPDATE((x) >> 8, 3, 0)
/* REG: 0xa3 */
#define RK3328_PRE_PLL_FB_DIV_7_0(x)			UPDATE(x, 7, 0)
/* REG: 0xa4*/
#define RK3328_PRE_PLL_TMDSCLK_DIV_C_MASK		GENMASK(1, 0)
#define RK3328_PRE_PLL_TMDSCLK_DIV_C(x)			UPDATE(x, 1, 0)
#define RK3328_PRE_PLL_TMDSCLK_DIV_B_MASK		GENMASK(3, 2)
#define RK3328_PRE_PLL_TMDSCLK_DIV_B(x)			UPDATE(x, 3, 2)
#define RK3328_PRE_PLL_TMDSCLK_DIV_A_MASK		GENMASK(5, 4)
#define RK3328_PRE_PLL_TMDSCLK_DIV_A(x)			UPDATE(x, 5, 4)
/* REG: 0xa5 */
#define RK3328_PRE_PLL_PCLK_DIV_B_SHIFT			5
#define RK3328_PRE_PLL_PCLK_DIV_B_MASK			GENMASK(6, 5)
#define RK3328_PRE_PLL_PCLK_DIV_B(x)			UPDATE(x, 6, 5)
#define RK3328_PRE_PLL_PCLK_DIV_A_MASK			GENMASK(4, 0)
#define RK3328_PRE_PLL_PCLK_DIV_A(x)			UPDATE(x, 4, 0)
/* REG: 0xa6 */
#define RK3328_PRE_PLL_PCLK_DIV_C_SHIFT			5
#define RK3328_PRE_PLL_PCLK_DIV_C_MASK			GENMASK(6, 5)
#define RK3328_PRE_PLL_PCLK_DIV_C(x)			UPDATE(x, 6, 5)
#define RK3328_PRE_PLL_PCLK_DIV_D_MASK			GENMASK(4, 0)
#define RK3328_PRE_PLL_PCLK_DIV_D(x)			UPDATE(x, 4, 0)
/* REG: 0xa9 */
#define RK3328_PRE_PLL_LOCK_STATUS			BIT(0)
/* REG: 0xaa */
#define RK3328_POST_PLL_POST_DIV_ENABLE			GENMASK(3, 2)
#define RK3328_POST_PLL_REFCLK_SEL_TMDS			BIT(1)
#define RK3328_POST_PLL_POWER_DOWN			BIT(0)
/* REG:0xab */
#define RK3328_POST_PLL_FB_DIV_8(x)			UPDATE((x) >> 8, 7, 7)
#define RK3328_POST_PLL_PRE_DIV(x)			UPDATE(x, 4, 0)
/* REG: 0xac */
#define RK3328_POST_PLL_FB_DIV_7_0(x)			UPDATE(x, 7, 0)
/* REG: 0xad */
#define RK3328_POST_PLL_POST_DIV_MASK			GENMASK(1, 0)
#define RK3328_POST_PLL_POST_DIV_2			0x0
#define RK3328_POST_PLL_POST_DIV_4			0x1
#define RK3328_POST_PLL_POST_DIV_8			0x3
/* REG: 0xaf */
#define RK3328_POST_PLL_LOCK_STATUS			BIT(0)
/* REG: 0xb0 */
#define RK3328_BANDGAP_ENABLE				BIT(2)
/* REG: 0xb2 */
#define RK3328_TMDS_CLK_DRIVER_EN			BIT(3)
#define RK3328_TMDS_D2_DRIVER_EN			BIT(2)
#define RK3328_TMDS_D1_DRIVER_EN			BIT(1)
#define RK3328_TMDS_D0_DRIVER_EN			BIT(0)
#define RK3328_TMDS_DRIVER_ENABLE		(RK3328_TMDS_CLK_DRIVER_EN | \
						RK3328_TMDS_D2_DRIVER_EN | \
						RK3328_TMDS_D1_DRIVER_EN | \
						RK3328_TMDS_D0_DRIVER_EN)
/* REG:0xc5 */
#define RK3328_BYPASS_TERM_RESISTOR_CALIB		BIT(7)
#define RK3328_TERM_RESISTOR_CALIB_SPEED_14_8(x)	UPDATE((x) >> 8, 6, 0)
/* REG:0xc6 */
#define RK3328_TERM_RESISTOR_CALIB_SPEED_7_0(x)		UPDATE(x, 7, 0)
/* REG:0xc7 */
#define RK3328_TERM_RESISTOR_50				UPDATE(0, 2, 1)
#define RK3328_TERM_RESISTOR_62_5			UPDATE(1, 2, 1)
#define RK3328_TERM_RESISTOR_75				UPDATE(2, 2, 1)
#define RK3328_TERM_RESISTOR_100			UPDATE(3, 2, 1)
/* REG 0xc8 - 0xcb */
#define RK3328_ESD_DETECT_MASK				GENMASK(7, 6)
#define RK3328_ESD_DETECT_340MV				(0x0 << 6)
#define RK3328_ESD_DETECT_280MV				(0x1 << 6)
#define RK3328_ESD_DETECT_260MV				(0x2 << 6)
#define RK3328_ESD_DETECT_240MV				(0x3 << 6)
/* resistors can be used in parallel */
#define RK3328_TMDS_TERM_RESIST_MASK			GENMASK(5, 0)
#define RK3328_TMDS_TERM_RESIST_75			BIT(5)
#define RK3328_TMDS_TERM_RESIST_150			BIT(4)
#define RK3328_TMDS_TERM_RESIST_300			BIT(3)
#define RK3328_TMDS_TERM_RESIST_600			BIT(2)
#define RK3328_TMDS_TERM_RESIST_1000			BIT(1)
#define RK3328_TMDS_TERM_RESIST_2000			BIT(0)
/* REG: 0xd1 */
#define RK3328_PRE_PLL_FRAC_DIV_23_16(x)		UPDATE((x) >> 16, 7, 0)
/* REG: 0xd2 */
#define RK3328_PRE_PLL_FRAC_DIV_15_8(x)			UPDATE((x) >> 8, 7, 0)
/* REG: 0xd3 */
#define RK3328_PRE_PLL_FRAC_DIV_7_0(x)			UPDATE(x, 7, 0)




enum inno_hdmi_phy_type {
	INNO_HDMI_PHY_RK3228,
	INNO_HDMI_PHY_RK3328
};

struct phy_config {
	unsigned long	tmdsclock;
	u8		regs[14];
};

struct inno_hdmi_phy_drv_data;

struct inno_hdmi_phy {
	struct device *dev;
	struct regmap *regmap;

	int irq;

	struct phy *phy;
	struct clk *sysclk;
	struct phy_config *phy_cfg;

	/* platform data */
	struct inno_hdmi_phy_drv_data *plat_data;

	/* efuse flag */
	bool efuse_flag;

	/* clk provider */
	struct clk_hw hw;
	struct clk *pclk;
	unsigned long pixclock;
	unsigned long tmdsclock;
};

struct pre_pll_config {
	unsigned long pixclock;
	unsigned long tmdsclock;
	u8 prediv;
	u16 fbdiv;
	u8 tmds_div_a;
	u8 tmds_div_b;
	u8 tmds_div_c;
	u8 pclk_div_a;
	u8 pclk_div_b;
	u8 pclk_div_c;
	u8 pclk_div_d;
	u8 vco_div_5_en;
	u32 fracdiv;
};

struct rk3328_hdmi_pll_config {
	unsigned char bus_width;
	unsigned long pixclock_start;
	unsigned long pixclock_end;
	unsigned char tmds_div_a;
	unsigned char tmds_div_b;
	unsigned char tmds_div_c;
	unsigned char pixclk_div_a;
	unsigned char pixclk_div_b;
	unsigned char pixclk_div_d;
	unsigned char vco_div_5_en;
	unsigned char tmds_div;
	unsigned char pixclock_div;
};

struct post_pll_config {
	unsigned long tmdsclock;
	u8 prediv;
	u16 fbdiv;
	u8 postdiv;
	u8 version;
};

struct inno_hdmi_phy_ops {
	void (*init)(struct inno_hdmi_phy *inno);
	int (*power_on)(struct inno_hdmi_phy *inno,
			const struct post_pll_config *cfg,
			const struct phy_config *phy_cfg);
	void (*power_off)(struct inno_hdmi_phy *inno);
	int (*pre_pll_update)(struct inno_hdmi_phy *inno,
			      const struct pre_pll_config *cfg);
	unsigned long (*recalc_rate)(struct inno_hdmi_phy *inno,
				     unsigned long parent_rate);
};

struct inno_hdmi_phy_drv_data {
	enum inno_hdmi_phy_type		dev_type;
	const struct inno_hdmi_phy_ops	*ops;
	const struct phy_config		*phy_cfg_table;
};

/*
	bus_width,    pixclock_start, pixclock_end,
	tmds_div_a,   tmds_div_b,     tmds_div_c,
	pixclk_div_a, pixclk_div_b,   pixclk_div_d,
	vco_div_5_en, tmds_div,       pixclock_div;
*/
static const struct rk3328_hdmi_pll_config rk3328_hdmi_pll_cfg_table[] ={
	//10bit Pix_Clock<=272MHz
	{10,  21000000,  25000000,  3, 2, 2, 10, 3, 5, 0, 80, 100},
	{10,  25000000,  40000000,  1, 3, 3,  1, 3, 8, 0, 64,  80},
	{10,  40000000,  50000000,  3, 1, 1,  1, 3, 5, 0, 40,  50},
	{10,  50000000,  80000000,  1, 2, 2,  1, 3, 4, 0, 32,  40},
	{10,  80000000, 100000000,  2, 1, 1,  1, 3, 3, 0, 24,  30},
	{10, 100000000, 160000000,  1, 1, 1,  1, 3, 2, 0, 16,  20},
	{10, 160000000, 200000000,  1, 0, 0,  1, 3, 1, 0,  8,  10}, // Jitter:vco<2GHz at 160MHz-200MHz
	{10, 200000000, 272000000,  1, 0, 0,  1, 3, 1, 0,  8,  10},
	//10bit Pix_Clock>272MHz(TMDS_Data_Clock>340MHz)
	{10, 272000000, 320000000,  0, 1, 1,  1, 3, 1, 0,  8,  10},
	{10, 320000000, 600000000,  0, 0, 0,  1, 3, 1, 1,  4,   5}, // Jitter:vco<2GHz at 320MHz-400MHz

	//8bit Pix_Clock<=340MHz
	{ 8,  21000000,  25000000,  2, 3, 3,  6, 3, 8, 0, 96,  96},
	{ 8,  25000000,  40000000,  3, 2, 2,  1, 3, 8, 0, 80,  80},
	{ 8,  40000000,  50000000,  1, 3, 3,  1, 2, 8, 0, 64,  64},
	{ 8,  50000000,  80000000,  3, 1, 1,  1, 3, 4, 0, 40,  40},
	{ 8,  80000000, 100000000,  1, 2, 2,  1, 2, 4, 0, 32,  32},
	{ 8, 100000000, 130000000,  2, 1, 1,  1, 2, 3, 0, 24,  24},
	{ 8, 130000000, 200000000,  1, 1, 1,  1, 2, 2, 0, 16,  16},
	{ 8, 200000000, 260000000,  2, 0, 0,  1, 1, 2, 0, 12,  12},
	{ 8, 260000000, 340000000,  1, 0, 0,  1, 0, 2, 0,  8,   8},
	//8bit Pix_Clock>340MHz(TMDS_Data_Clock>340MHz)
	{ 8, 340000000, 400000000,  0, 3, 1,  1, 0, 2, 0,  8,   8},
	{ 8, 400000000, 600000000,  0, 2, 0,  1, 0, 1, 0,  4,   4}, // Jitter:vco<2GHz at 400MHz-500MHzi
	{ /* sentinel */ }
};

/*
 * If only using integer freq div can't get frequency we want, frac
 * freq div is needed. For example, pclk 88.75 Mhz and tmdsclk
 * 110.9375 Mhz must use frac div 0xF00000. The actual frequency is different
 * from the target frequency. Such as the tmds clock 110.9375 Mhz,
 * the actual tmds clock we get is 110.93719 Mhz. It is important
 * to note that RK322X platforms do not support frac div.
 */
static struct pre_pll_config pre_pll_cfg_table={ /* sentinel */ };

static const struct post_pll_config post_pll_cfg_table[] = {
	{33750000,  1, 40, 8, 1},
	{33750000,  1, 80, 8, 2},
	{33750000,  1, 10, 2, 4},
	{74250000,  1, 40, 8, 1},
	{74250000, 18, 80, 8, 2},
	{148500000, 2, 40, 4, 3},
	{297000000, 4, 40, 2, 3},
	{594000000, 8, 40, 1, 3},
	{     ~0UL, 0,  0, 0, 0}
};

static const struct phy_config rk3228_phy_cfg[] = {
	{	165000000, {
			0xaa, 0x00, 0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00,
		},
	}, {
		340000000, {
			0xaa, 0x15, 0x6a, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00,
		},
	}, {
		594000000, {
			0xaa, 0x15, 0x7a, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00,
		},
	}, {
		~0UL, {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00,
		},
	}
};

static const struct phy_config rk3328_phy_cfg[] = {
	{	165000000, {
			0x07, 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x08, 0x08,
			0x00, 0xac, 0xcc, 0xcc, 0xcc,
		},
	}, {
		340000000, {
			0x0b, 0x0d, 0x0d, 0x0d, 0x07, 0x15, 0x08, 0x08, 0x08,
			0x3f, 0xac, 0xcc, 0xcd, 0xdd,
		},
	}, {
		594000000, {
			0x10, 0x1a, 0x1a, 0x1a, 0x07, 0x15, 0x08, 0x08, 0x08,
			0x00, 0xac, 0xcc, 0xcc, 0xcc,
		},
	}, {
		~0UL, {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00,
		},
	}
};

static inline struct inno_hdmi_phy *to_inno_hdmi_phy(struct clk_hw *hw)
{
	return container_of(hw, struct inno_hdmi_phy, hw);
}

static inline void inno_write(struct inno_hdmi_phy *inno, u32 reg, u8 val)
{
	regmap_write(inno->regmap, reg * 4, val);
}

static inline u8 inno_read(struct inno_hdmi_phy *inno, u32 reg)
{
	u32 val;

	regmap_read(inno->regmap, reg * 4, &val);

	return val;
}

static inline void inno_update_bits(struct inno_hdmi_phy *inno, u8 reg,
				    u8 mask, u8 val)
{
	regmap_update_bits(inno->regmap, reg * 4, mask, val);
}

#define inno_poll(inno, reg, val, cond, sleep_us, timeout_us) \
	regmap_read_poll_timeout((inno)->regmap, (reg) * 4, val, cond, \
				 sleep_us, timeout_us)

static unsigned int inno_hdmi_phy_get_tmdsclk(struct inno_hdmi_phy *inno, unsigned int rate)
{
	int bus_width = phy_get_bus_width(inno->phy);
	unsigned int tmdsclk;

	switch (bus_width) {
	case 4:
		tmdsclk = rate / 2;
		break;
	case 5:
		tmdsclk = rate * 5 / 8;
		break;
	case 6:
		tmdsclk = rate * 3 / 4;
		break;
	case 10:
		tmdsclk = rate * 5 / 4;
		break;
	case 12:
		tmdsclk = rate * 3 / 2;
		break;
	case 16:
		tmdsclk = rate * 2;
		break;
	default:
		tmdsclk = rate;
	}

	return tmdsclk;
}

static irqreturn_t inno_hdmi_phy_hardirq(int irq, void *dev_id)
{
	struct inno_hdmi_phy *inno = dev_id;
	int intr_stat1, intr_stat2, intr_stat3;

	if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3228)
		return IRQ_NONE;

	intr_stat1 = inno_read(inno, 0x04);
	intr_stat2 = inno_read(inno, 0x06);
	intr_stat3 = inno_read(inno, 0x08);

	if (intr_stat1)
		inno_write(inno, 0x04, intr_stat1);
	if (intr_stat2)
		inno_write(inno, 0x06, intr_stat2);
	if (intr_stat3)
		inno_write(inno, 0x08, intr_stat3);

	if (intr_stat1 || intr_stat2 || intr_stat3)
		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

static irqreturn_t inno_hdmi_phy_irq(int irq, void *dev_id)
{
	struct inno_hdmi_phy *inno = dev_id;

	if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3228)
		return IRQ_NONE;
	/* set pdata_en to 0 */
	inno_update_bits(inno, 0x02, 1, 0);

	udelay(10);

	/* set pdata_en to 1 */
	inno_update_bits(inno, 0x02, 1, 1);

	return IRQ_HANDLED;
}

static int inno_hdmi_phy_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long parent_rate);

static int inno_hdmi_phy_power_on(struct phy *phy)
{
	struct inno_hdmi_phy *inno = phy_get_drvdata(phy);
	const struct post_pll_config *cfg = post_pll_cfg_table;
	const struct phy_config *phy_cfg = inno->plat_data->phy_cfg_table;
	u32 tmdsclock = inno_hdmi_phy_get_tmdsclk(inno, inno->pixclock);
	u32 chipversion = 1;

	if (inno->phy_cfg)
		phy_cfg = inno->phy_cfg;

	if (!tmdsclock) {
		dev_err(inno->dev, "TMDS clock is zero!\n");
		return -EINVAL;
	}

	if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3328 &&
	    rockchip_get_cpu_version())
		chipversion = 2;
	else if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3228 &&
		 tmdsclock <= 33750000 && inno->efuse_flag)
		chipversion = 4;

	for (; cfg->tmdsclock != ~0UL; cfg++)
		if (tmdsclock <= cfg->tmdsclock &&
		    cfg->version & chipversion)
			break;

	for (; phy_cfg->tmdsclock != ~0UL; phy_cfg++)
		if (tmdsclock <= phy_cfg->tmdsclock)
			break;

	if (cfg->tmdsclock == ~0UL || phy_cfg->tmdsclock == ~0UL)
		return -EINVAL;

	dev_dbg(inno->dev, "Inno HDMI PHY Power On\n");
	inno_hdmi_phy_clk_set_rate(&inno->hw, inno->pixclock, 24000000);

	if (inno->plat_data->ops->power_on)
		return inno->plat_data->ops->power_on(inno, cfg, phy_cfg);
	else
		return -EINVAL;
}

static int inno_hdmi_phy_power_off(struct phy *phy)
{
	struct inno_hdmi_phy *inno = phy_get_drvdata(phy);

	if (inno->plat_data->ops->power_off)
		inno->plat_data->ops->power_off(inno);

	inno->tmdsclock = 0;
	dev_dbg(inno->dev, "Inno HDMI PHY Power Off\n");

	return 0;
}

static const struct phy_ops inno_hdmi_phy_ops = {
	.owner	   = THIS_MODULE,
	.power_on  = inno_hdmi_phy_power_on,
	.power_off = inno_hdmi_phy_power_off,
};

static
struct pre_pll_config *inno_hdmi_phy_get_pre_pll_cfg(struct inno_hdmi_phy *inno,
						     unsigned long pixclock, unsigned long parent_rate)
{
	const struct rk3328_hdmi_pll_config *table = rk3328_hdmi_pll_cfg_table;
	struct pre_pll_config *cfg = &pre_pll_cfg_table;
	int bus_width = phy_get_bus_width(inno->phy);
	unsigned long tmdsclock = inno_hdmi_phy_get_tmdsclk(inno, pixclock);
	unsigned char prediv=1;
	unsigned long fvco;
	unsigned char fbdiv;
	unsigned int  fracdiv;
	unsigned long mod;

	for(; table->bus_width != 0; table++)
	{
		if(bus_width == table->bus_width &&
			pixclock > table->pixclock_start &&
			pixclock <= table->pixclock_end)
		{
			break;
		}
	}

	if (table->bus_width == 0)
	{
		printk("FAIL:inno_hdmi_phy_get_pre_pll_cfg:\nbus_width=%d,pixclock=%lu\n",bus_width,pixclock);
		return ERR_PTR(-EINVAL);
	}

	fvco = pixclock * table->pixclock_div;

	mod = do_div(fvco, parent_rate * prediv);
	fbdiv = fvco;

	mod *= 1<<24;
	do_div(mod, parent_rate * prediv);
	fracdiv = mod;

	cfg->pixclock = pixclock;
	cfg->tmdsclock = tmdsclock;
	cfg->prediv = prediv;
	cfg->fbdiv = fbdiv;
	cfg->tmds_div_a = table->tmds_div_a;
	cfg->tmds_div_b = table->tmds_div_b;
	cfg->tmds_div_c = table->tmds_div_c;
	cfg->pclk_div_a = table->pixclk_div_a;
	cfg->pclk_div_b = table->pixclk_div_b;
	cfg->pclk_div_c = 3;
	cfg->pclk_div_d = table->pixclk_div_d;
	cfg->vco_div_5_en = table->vco_div_5_en;
	cfg->fracdiv = fracdiv;

	printk("xiaoren:inno_hdmi_phy_get_pre_pll_cfg:\nbus_width=%d,pixclock=%lu,tmds=%lu\n",bus_width,pixclock,tmdsclock);

	return cfg;
}

static int inno_hdmi_phy_clk_is_prepared(struct clk_hw *hw)
{
	struct inno_hdmi_phy *inno = to_inno_hdmi_phy(hw);
	u8 status;

	if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3228)
		status = inno_read(inno, 0xe0) & PRE_PLL_POWER_MASK;
	else
		status = inno_read(inno, 0xa0) & 1;

	return status ? 0 : 1;
}

static int inno_hdmi_phy_clk_prepare(struct clk_hw *hw)
{
	struct inno_hdmi_phy *inno = to_inno_hdmi_phy(hw);

	if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3228)
		inno_update_bits(inno, 0xe0, PRE_PLL_POWER_MASK,
				 PRE_PLL_POWER_UP);
	else
		inno_update_bits(inno, 0xa0, 1, 0);

	return 0;
}

static void inno_hdmi_phy_clk_unprepare(struct clk_hw *hw)
{
	struct inno_hdmi_phy *inno = to_inno_hdmi_phy(hw);

	if (inno->plat_data->dev_type == INNO_HDMI_PHY_RK3228)
		inno_update_bits(inno, 0xe0, PRE_PLL_POWER_MASK,
				 PRE_PLL_POWER_DOWN);
	else
		inno_update_bits(inno, 0xa0, 1, 1);
}

static unsigned long inno_hdmi_phy_clk_recalc_rate(struct clk_hw *hw,
						   unsigned long parent_rate)
{
	struct inno_hdmi_phy *inno = to_inno_hdmi_phy(hw);
	unsigned long frac;
	u8 nd, no_a, no_b, no_d;
	u64 vco;
	u16 nf;

	nd = inno_read(inno, 0xa1) & RK3328_PRE_PLL_PRE_DIV_MASK;
	nf = ((inno_read(inno, 0xa2) & RK3328_PRE_PLL_FB_DIV_11_8_MASK) << 8);
	nf |= inno_read(inno, 0xa3);
	vco = parent_rate * nf;

	if (!(inno_read(inno, 0xa2) & RK3328_PRE_PLL_FRAC_DIV_DISABLE)) {
		frac = inno_read(inno, 0xd3) |
		       (inno_read(inno, 0xd2) << 8) |
		       (inno_read(inno, 0xd1) << 16);
		vco += DIV_ROUND_CLOSEST(parent_rate * frac, (1 << 24));
	}

	if (inno_read(inno, 0xa0) & RK3328_PCLK_VCO_DIV_5_MASK) {
		do_div(vco, nd * 5);
	} else {
		no_a = inno_read(inno, 0xa5) & RK3328_PRE_PLL_PCLK_DIV_A_MASK;
		no_b = inno_read(inno, 0xa5) & RK3328_PRE_PLL_PCLK_DIV_B_MASK;
		no_b >>= RK3328_PRE_PLL_PCLK_DIV_B_SHIFT;
		no_b += 2;
		no_d = inno_read(inno, 0xa6) & RK3328_PRE_PLL_PCLK_DIV_D_MASK;

		do_div(vco, (nd * (no_a == 1 ? no_b : no_a) * no_d * 2));
	}

	inno->pixclock = DIV_ROUND_CLOSEST((unsigned long)vco, 1000) * 1000;

	dev_dbg(inno->dev, "%s rate %lu vco %llu\n",
		__func__, inno->pixclock, vco);

		return inno->pixclock;
}

static long inno_hdmi_phy_clk_round_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long *parent_rate)
{
	const struct rk3328_hdmi_pll_config *table = rk3328_hdmi_pll_cfg_table;

	rate = (rate / 1000) * 1000;

	for(; table->bus_width != 0; table++)
	{
		if(rate > table->pixclock_start && rate <= table->pixclock_end)
			break;
	}

	if (table->bus_width == 0)
	{
		printk("FAIL:inno_hdmi_phy_clk_round_rate:\nbus_width=%d,rate=%lu\n",table->bus_width,rate);
		return -EINVAL;
	}

	printk("xiaoren:inno_hdmi_phy_clk_round_rate:\nbus_width=%d,rate=%lu\n",table->bus_width,rate);

	return rate;
}

static int inno_hdmi_phy_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long parent_rate)
{
	struct inno_hdmi_phy *inno = to_inno_hdmi_phy(hw);
	struct pre_pll_config *cfg = &pre_pll_cfg_table;
	unsigned long tmdsclock = inno_hdmi_phy_get_tmdsclk(inno, rate);
	u32 val;
	int ret;

	dev_dbg(inno->dev, "%s rate=%lu tmdsclk=%lu\n",
		__func__, rate, tmdsclock);

	//if (inno->pixclock == rate && inno->tmdsclock == tmdsclock)
	//	return 0;

	cfg = inno_hdmi_phy_get_pre_pll_cfg(inno, rate, parent_rate);
	if (IS_ERR(cfg))
		return PTR_ERR(cfg);

	inno_update_bits(inno, 0xa0, RK3328_PRE_PLL_POWER_DOWN,
			 RK3328_PRE_PLL_POWER_DOWN);

	/* Configure pre-pll */
	inno_update_bits(inno, 0xa0, RK3328_PCLK_VCO_DIV_5_MASK,
			 RK3328_PCLK_VCO_DIV_5(cfg->vco_div_5_en));
	inno_write(inno, 0xa1, RK3328_PRE_PLL_PRE_DIV(cfg->prediv));

	val = RK3328_SPREAD_SPECTRUM_MOD_DISABLE;
	if (!cfg->fracdiv)
		val |= RK3328_PRE_PLL_FRAC_DIV_DISABLE;
	inno_write(inno, 0xa2, RK3328_PRE_PLL_FB_DIV_11_8(cfg->fbdiv) | val);
	inno_write(inno, 0xa3, RK3328_PRE_PLL_FB_DIV_7_0(cfg->fbdiv));
	inno_write(inno, 0xa5, RK3328_PRE_PLL_PCLK_DIV_A(cfg->pclk_div_a) |
		   RK3328_PRE_PLL_PCLK_DIV_B(cfg->pclk_div_b));
	inno_write(inno, 0xa6, RK3328_PRE_PLL_PCLK_DIV_C(cfg->pclk_div_c) |
		   RK3328_PRE_PLL_PCLK_DIV_D(cfg->pclk_div_d));
	inno_write(inno, 0xa4, RK3328_PRE_PLL_TMDSCLK_DIV_C(cfg->tmds_div_c) |
		   RK3328_PRE_PLL_TMDSCLK_DIV_A(cfg->tmds_div_a) |
		   RK3328_PRE_PLL_TMDSCLK_DIV_B(cfg->tmds_div_b));
	inno_write(inno, 0xd3, RK3328_PRE_PLL_FRAC_DIV_7_0(cfg->fracdiv));
	inno_write(inno, 0xd2, RK3328_PRE_PLL_FRAC_DIV_15_8(cfg->fracdiv));
	inno_write(inno, 0xd1, RK3328_PRE_PLL_FRAC_DIV_23_16(cfg->fracdiv));

	inno_update_bits(inno, 0xa0, RK3328_PRE_PLL_POWER_DOWN, 0);

	/* Wait for Pre-PLL lock */
	ret = inno_poll(inno, 0xa9, val, val & RK3328_PRE_PLL_LOCK_STATUS,
			1000, 10000);
	if (ret) {
		dev_err(inno->dev, "Pre-PLL locking failed\n");
		return ret;
	}

	inno->pixclock = rate;
	inno->tmdsclock = tmdsclock;

	return 0;
}

static const struct clk_ops inno_hdmi_phy_clk_ops = {
	.prepare = inno_hdmi_phy_clk_prepare,
	.unprepare = inno_hdmi_phy_clk_unprepare,
	.is_prepared = inno_hdmi_phy_clk_is_prepared,
	.recalc_rate = inno_hdmi_phy_clk_recalc_rate,
	.round_rate = inno_hdmi_phy_clk_round_rate,
	.set_rate = inno_hdmi_phy_clk_set_rate,
};

static int inno_hdmi_phy_clk_register(struct inno_hdmi_phy *inno)
{
	struct device *dev = inno->dev;
	struct device_node *np = dev->of_node;
	struct clk_init_data init;
	struct clk *refclk;
	const char *parent_name;
	int ret;

	refclk = devm_clk_get(dev, "refclk");
	if (IS_ERR(refclk)) {
		dev_err(dev, "failed to get ref clock\n");
		return PTR_ERR(refclk);
	}

	parent_name = __clk_get_name(refclk);

	init.parent_names = &parent_name;
	init.num_parents = 1;
	init.flags = 0;
	init.name = "pin_hd20_pclk";
	init.ops = &inno_hdmi_phy_clk_ops;

	/* optional override of the clock name */
	of_property_read_string(np, "clock-output-names", &init.name);

	inno->hw.init = &init;

	inno->pclk = devm_clk_register(dev, &inno->hw);
	if (IS_ERR(inno->pclk)) {
		ret = PTR_ERR(inno->pclk);
		dev_err(dev, "failed to register clock: %d\n", ret);
		return ret;
	}

	ret = of_clk_add_provider(np, of_clk_src_simple_get, inno->pclk);
	if (ret) {
		dev_err(dev, "failed to register OF clock provider: %d\n", ret);
		return ret;
	}

	return 0;
}

static int
inno_hdmi_phy_rk3228_power_on(struct inno_hdmi_phy *inno,
			      const struct post_pll_config *cfg,
			      const struct phy_config *phy_cfg)
{
	int pll_tries;
	u32 m, v;

	/* pdata_en disable */
	inno_update_bits(inno, 0x02, PDATAEN_MASK, PDATAEN_DISABLE);

	/* Power down Post-PLL */
	inno_update_bits(inno, 0xe0, PRE_PLL_POWER_MASK, PRE_PLL_POWER_DOWN);
	inno_update_bits(inno, 0xe0, POST_PLL_POWER_MASK, POST_PLL_POWER_DOWN);

	/* Post-PLL update */
	m = POST_PLL_PRE_DIV_MASK;
	v = POST_PLL_PRE_DIV(cfg->prediv);
	inno_update_bits(inno, 0xe9, m, v);

	m = POST_PLL_FB_DIV_8_MASK;
	v = POST_PLL_FB_DIV_8(cfg->fbdiv >> 8);
	inno_update_bits(inno, 0xeb, m, v);
	inno_write(inno, 0xea, POST_PLL_FB_DIV_7_0(cfg->fbdiv));

	if (cfg->postdiv == 1) {
		/* Disable Post-PLL post divider */
		m = POST_PLL_POST_DIV_EN_MASK;
		v = POST_PLL_POST_DIV_DISABLE;
		inno_update_bits(inno, 0xe9, m, v);
	} else {
		/* Enable Post-PLL post divider */
		m = POST_PLL_POST_DIV_EN_MASK;
		v = POST_PLL_POST_DIV_ENABLE;
		inno_update_bits(inno, 0xe9, m, v);

		m = POST_PLL_POST_DIV_MASK;
		v = POST_PLL_POST_DIV(cfg->postdiv / 2 - 1);
		inno_update_bits(inno, 0xeb, m, v);
	}

	for (v = 0; v < 4; v++)
		inno_write(inno, 0xef + v, phy_cfg->regs[v]);

	/* Power up Post-PLL */
	inno_update_bits(inno, 0xe0, POST_PLL_POWER_MASK, POST_PLL_POWER_UP);
	inno_update_bits(inno, 0xe0, PRE_PLL_POWER_MASK, PRE_PLL_POWER_UP);

	/* BandGap enable */
	inno_update_bits(inno, 0xe1, BANDGAP_MASK, BANDGAP_ENABLE);

	/* TMDS driver enable */
	inno_update_bits(inno, 0xe1, TMDS_DRIVER_MASK, TMDS_DRIVER_ENABLE);

	/* Wait for post PLL lock */
	pll_tries = 0;
	while (!(inno_read(inno, 0xeb) & POST_PLL_LOCK_STATUS)) {
		if (pll_tries == INNO_HDMI_PHY_TIMEOUT_LOOP_COUNT) {
			dev_err(inno->dev, "Post-PLL unlock\n");
			return -ETIMEDOUT;
		}

		pll_tries++;
		usleep_range(100, 110);
	}

	if (cfg->tmdsclock > 340000000)
		msleep(100);

	/* pdata_en enable */
	inno_update_bits(inno, 0x02, PDATAEN_MASK, PDATAEN_ENABLE);
	return 0;
}

static void inno_hdmi_phy_rk3228_power_off(struct inno_hdmi_phy *inno)
{
	/* TMDS driver Disable */
	inno_update_bits(inno, 0xe1, TMDS_DRIVER_MASK, TMDS_DRIVER_DISABLE);

	/* BandGap Disable */
	inno_update_bits(inno, 0xe1, BANDGAP_MASK, BANDGAP_DISABLE);

	/* Post-PLL power down */
	inno_update_bits(inno, 0xe0, POST_PLL_POWER_MASK, POST_PLL_POWER_DOWN);
}

static void inno_hdmi_phy_rk3228_init(struct inno_hdmi_phy *inno)
{
	u32 m, v;
	struct nvmem_cell *cell;
	unsigned char *efuse_buf;
	size_t len;

	/*
	 * Use phy internal register control
	 * rxsense/poweron/pllpd/pdataen signal.
	 */
	m = BYPASS_RXSENSE_EN_MASK | BYPASS_PWRON_EN_MASK |
	    BYPASS_PLLPD_EN_MASK;
	v = BYPASS_RXSENSE_EN | BYPASS_PWRON_EN | BYPASS_PLLPD_EN;
	inno_update_bits(inno, 0x01, m, v);
	inno_update_bits(inno, 0x02, BYPASS_PDATA_EN_MASK, BYPASS_PDATA_EN);

	/*
	 * reg0xe9 default value is 0xe4, reg0xea is 0x50.
	 * if phy had been set in uboot, one of them will be different.
	 */
	if ((inno_read(inno, 0xe9) != 0xe4 || inno_read(inno, 0xea) != 0x50)) {
		dev_info(inno->dev, "phy had been powered up\n");
		inno->phy->power_count = 1;
	} else {
		inno_hdmi_phy_rk3228_power_off(inno);
		/* manual power down post-PLL */
		inno_update_bits(inno, 0xaa,
				 POST_PLL_CTRL_MASK, POST_PLL_CTRL_MANUAL);
	}

	cell = nvmem_cell_get(inno->dev, "hdmi_phy_flag");
	if (IS_ERR(cell)) {
		dev_err(inno->dev,
			"failed to get id cell: %ld\n", PTR_ERR(cell));
		return;
	}
	efuse_buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (len == 1)
		inno->efuse_flag = efuse_buf[0] ? true : false;
	kfree(efuse_buf);
}

static int
inno_hdmi_phy_rk3228_pre_pll_update(struct inno_hdmi_phy *inno,
				    const struct pre_pll_config *cfg)
{
	int pll_tries;
	u32 m, v;

	/* Power down PRE-PLL */
	inno_update_bits(inno, 0xe0, PRE_PLL_POWER_MASK, PRE_PLL_POWER_DOWN);

	m = PRE_PLL_FB_DIV_8_MASK | PCLK_VCO_DIV_5_MASK | PRE_PLL_PRE_DIV_MASK;
	v = PRE_PLL_FB_DIV_8(cfg->fbdiv >> 8) |
	    PCLK_VCO_DIV_5(cfg->vco_div_5_en) | PRE_PLL_PRE_DIV(cfg->prediv);
	inno_update_bits(inno, 0xe2, m, v);

	inno_write(inno, 0xe3, PRE_PLL_FB_DIV_7_0(cfg->fbdiv));

	m = PRE_PLL_PCLK_DIV_B_MASK | PRE_PLL_PCLK_DIV_A_MASK;
	v = PRE_PLL_PCLK_DIV_B(cfg->pclk_div_b) |
	    PRE_PLL_PCLK_DIV_A(cfg->pclk_div_a);
	inno_update_bits(inno, 0xe4, m, v);

	m = PRE_PLL_PCLK_DIV_C_MASK | PRE_PLL_PCLK_DIV_D_MASK;
	v = PRE_PLL_PCLK_DIV_C(cfg->pclk_div_c) |
	    PRE_PLL_PCLK_DIV_D(cfg->pclk_div_d);
	inno_update_bits(inno, 0xe5, m, v);

	m = PRE_PLL_TMDSCLK_DIV_C_MASK | PRE_PLL_TMDSCLK_DIV_A_MASK |
	    PRE_PLL_TMDSCLK_DIV_B_MASK;
	v = PRE_PLL_TMDSCLK_DIV_C(cfg->tmds_div_c) |
	    PRE_PLL_TMDSCLK_DIV_A(cfg->tmds_div_a) |
	    PRE_PLL_TMDSCLK_DIV_B(cfg->tmds_div_b);
	inno_update_bits(inno, 0xe6, m, v);

	/* Power up PRE-PLL */
	inno_update_bits(inno, 0xe0, PRE_PLL_POWER_MASK, PRE_PLL_POWER_UP);

	/* Wait for Pre-PLL lock */
	pll_tries = 0;
	while (!(inno_read(inno, 0xe8) & PRE_PLL_LOCK_STATUS)) {
		if (pll_tries == INNO_HDMI_PHY_TIMEOUT_LOOP_COUNT) {
			dev_err(inno->dev, "Pre-PLL unlock\n");
			return -ETIMEDOUT;
		}

		pll_tries++;
		usleep_range(100, 110);
	}

	return 0;
}

static int
inno_hdmi_phy_rk3328_power_on(struct inno_hdmi_phy *inno,
			      const struct post_pll_config *cfg,
			      const struct phy_config *phy_cfg)
{
	u32 val;
	u64 temp;

	/* set pdata_en to 0 */
	inno_update_bits(inno, 0x02, 1, 0);
	/* Power off post PLL */
	inno_update_bits(inno, 0xaa, 1, 1);

	val = cfg->fbdiv & 0xff;
	inno_write(inno, 0xac, val);
	if (cfg->postdiv == 1) {
		inno_write(inno, 0xaa, 2);
		val = (cfg->fbdiv >> 8) | cfg->prediv;
		inno_write(inno, 0xab, val);
	} else {
		val = (cfg->postdiv / 2) - 1;
		inno_write(inno, 0xad, val);
		val = (cfg->fbdiv >> 8) | cfg->prediv;
		inno_write(inno, 0xab, val);
		inno_write(inno, 0xaa, 0x0e);
	}

	for (val = 0; val < 14; val++)
		inno_write(inno, 0xb5 + val, phy_cfg->regs[val]);

	/* bit[7:6] of reg c8/c9/ca/c8 is ESD detect threshold:
	 * 00 - 340mV
	 * 01 - 280mV
	 * 10 - 260mV
	 * 11 - 240mV
	 * default is 240mV, now we set it to 340mV
	 */
	inno_write(inno, 0xc8, 0);
	inno_write(inno, 0xc9, 0);
	inno_write(inno, 0xca, 0);
	inno_write(inno, 0xcb, 0);

	if (phy_cfg->tmdsclock > 340000000) {
		/* Set termination resistor to 100ohm */
		val = clk_get_rate(inno->sysclk) / 100000;
		inno_write(inno, 0xc5, ((val >> 8) & 0xff) | 0x80);
		inno_write(inno, 0xc6, val & 0xff);
		inno_write(inno, 0xc7, 3 << 1);
		inno_write(inno, 0xc5, ((val >> 8) & 0xff));
	} else {
		inno_write(inno, 0xc5, 0x81);
		/* clk termination resistor is 50ohm */
		if (phy_cfg->tmdsclock > 165000000)
			inno_write(inno, 0xc8, 0x30);
		/* data termination resistor is 150ohm */
		inno_write(inno, 0xc9, 0x10);
		inno_write(inno, 0xca, 0x10);
		inno_write(inno, 0xcb, 0x10);
	}

	/* set TMDS sync detection counter length */
	temp = 47520000000;
	do_div(temp, inno->tmdsclock);
	inno_write(inno, 0xd8, (temp >> 8) & 0xff);
	inno_write(inno, 0xd9, temp & 0xff);

	/* Power up post PLL */
	inno_update_bits(inno, 0xaa, 1, 0);
	/* Power up tmds driver */
	inno_update_bits(inno, 0xb0, 4, 4);
	inno_write(inno, 0xb2, 0x0f);

	/* Wait for post PLL lock */
	for (val = 0; val < 5; val++) {
		if (inno_read(inno, 0xaf) & 1)
			break;
		usleep_range(1000, 2000);
	}
	if (!(inno_read(inno, 0xaf) & 1)) {
		dev_err(inno->dev, "HDMI PHY Post PLL unlock\n");
		return -ETIMEDOUT;
	}
	if (phy_cfg->tmdsclock > 340000000)
		msleep(100);
	/* set pdata_en to 1 */
	inno_update_bits(inno, 0x02, 1, 1);

	/* Enable PHY IRQ */
	inno_write(inno, 0x05, 0x22);
	inno_write(inno, 0x07, 0x22);
	return 0;
}

static void inno_hdmi_phy_rk3328_power_off(struct inno_hdmi_phy *inno)
{
	/* Power off driver */
	inno_write(inno, 0xb2, 0);
	/* Power off band gap */
	inno_update_bits(inno, 0xb0, 4, 0);
	/* Power off post pll */
	inno_update_bits(inno, 0xaa, 1, 1);

	/* Disable PHY IRQ */
	inno_write(inno, 0x05, 0);
	inno_write(inno, 0x07, 0);
}

static void inno_hdmi_phy_rk3328_init(struct inno_hdmi_phy *inno)
{
	/*
	 * Use phy internal register control
	 * rxsense/poweron/pllpd/pdataen signal.
	 */
	inno_write(inno, 0x01, 0x07);
	inno_write(inno, 0x02, 0x91);

	/*
	 * reg0xc8 default value is 0xc0, if phy had been set in uboot,
	 * the value of bit[7:6] will be zero.
	 */
	if ((inno_read(inno, 0xc8) & 0xc0) == 0) {
		dev_info(inno->dev, "phy had been powered up\n");
		inno->phy->power_count = 1;
	} else {
		/* manual power down post-PLL */
		inno_hdmi_phy_rk3328_power_off(inno);
	}
}

static int
inno_hdmi_phy_rk3328_pre_pll_update(struct inno_hdmi_phy *inno,
				    const struct pre_pll_config *cfg)
{
	u32 val;

	/* Power off PLL */
	inno_update_bits(inno, 0xa0, 1, 1);
	/* Configure pre-pll */
	inno_update_bits(inno, 0xa0, 2, (cfg->vco_div_5_en & 1) << 1);
	inno_write(inno, 0xa1, cfg->prediv);
	if (cfg->fracdiv)
		val = ((cfg->fbdiv >> 8) & 0x0f) | 0xc0;
	else
		val = ((cfg->fbdiv >> 8) & 0x0f) | 0xf0;
	inno_write(inno, 0xa2, val);
	inno_write(inno, 0xa3, cfg->fbdiv & 0xff);
	val = (cfg->pclk_div_a & 0x1f) |
	      ((cfg->pclk_div_b & 3) << 5);
	inno_write(inno, 0xa5, val);
	val = (cfg->pclk_div_d & 0x1f) |
	      ((cfg->pclk_div_c & 3) << 5);
	inno_write(inno, 0xa6, val);
	val = ((cfg->tmds_div_a & 3) << 4) |
	      ((cfg->tmds_div_b & 3) << 2) |
	      (cfg->tmds_div_c & 3);
	inno_write(inno, 0xa4, val);

	if (cfg->fracdiv) {
		val = cfg->fracdiv & 0xff;
		inno_write(inno, 0xd3, val);
		val = (cfg->fracdiv >> 8) & 0xff;
		inno_write(inno, 0xd2, val);
		val = (cfg->fracdiv >> 16) & 0xff;
		inno_write(inno, 0xd1, val);
	} else {
		inno_write(inno, 0xd3, 0);
		inno_write(inno, 0xd2, 0);
		inno_write(inno, 0xd1, 0);
	}

	/* Power up PLL */
	inno_update_bits(inno, 0xa0, 1, 0);

	/* Wait for PLL lock */
	for (val = 0; val < 5; val++) {
		if (inno_read(inno, 0xa9) & 1)
			break;
		usleep_range(1000, 2000);
	}
	if (val == 5) {
		dev_err(inno->dev, "Pre-PLL unlock\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static unsigned long
inno_hdmi_rk3328_phy_pll_recalc_rate(struct inno_hdmi_phy *inno,
				     unsigned long parent_rate)
{
	unsigned long frac;
	u8 nd, no_a, no_b, no_d;
	u16 nf;
	u64 vco = parent_rate;

	nd = inno_read(inno, 0xa1) & 0x3f;
	nf = ((inno_read(inno, 0xa2) & 0x0f) << 8) | inno_read(inno, 0xa3);
	vco *= nf;
	if ((inno_read(inno, 0xa2) & 0x30) == 0) {
		frac = inno_read(inno, 0xd3) |
		       (inno_read(inno, 0xd2) << 8) |
		       (inno_read(inno, 0xd1) << 16);
		vco += DIV_ROUND_CLOSEST(parent_rate * frac, (1 << 24));
	}
	if (inno_read(inno, 0xa0) & 2) {
		do_div(vco, nd * 5);
	} else {
		no_a = inno_read(inno, 0xa5) & 0x1f;
		no_b = ((inno_read(inno, 0xa5) >> 5) & 7) + 2;
		no_d = inno_read(inno, 0xa6) & 0x1f;
		if (no_a == 1)
			do_div(vco, nd * no_b * no_d * 2);
		else
			do_div(vco, nd * no_a * no_d * 2);
	}

	frac = vco;
	inno->pixclock = DIV_ROUND_CLOSEST(frac, 1000) * 1000;

	dev_dbg(inno->dev, "%s rate %lu\n", __func__, inno->pixclock);

	return frac;
}

static unsigned long
inno_hdmi_rk3228_phy_pll_recalc_rate(struct inno_hdmi_phy *inno,
				     unsigned long parent_rate)
{
	u8 nd, no_a, no_b, no_d;
	u16 nf;
	u64 vco = parent_rate;

	nd = inno_read(inno, 0xe2) & 0x1f;
	nf = ((inno_read(inno, 0xe2) & 0x80) << 1) | inno_read(inno, 0xe3);
	vco *= nf;

	if ((inno_read(inno, 0xe2) >> 5) & 0x1) {
		do_div(vco, nd * 5);
	} else {
		no_a = inno_read(inno, 0xe4) & 0x1f;
		if (!no_a)
			no_a = 1;
		no_b = ((inno_read(inno, 0xe4) >> 5) & 0x3) + 2;
		no_d = inno_read(inno, 0xe5) & 0x1f;

		if (no_a == 1)
			do_div(vco, nd * no_b * no_d * 2);
		else
			do_div(vco, nd * no_a * no_d * 2);
	}

	inno->pixclock = vco;

	dev_dbg(inno->dev, "%s rate %lu\n", __func__, inno->pixclock);

	return inno->pixclock;
}

static const struct inno_hdmi_phy_ops rk3228_hdmi_phy_ops = {
	.init = inno_hdmi_phy_rk3228_init,
	.power_on = inno_hdmi_phy_rk3228_power_on,
	.power_off = inno_hdmi_phy_rk3228_power_off,
	.pre_pll_update = inno_hdmi_phy_rk3228_pre_pll_update,
	.recalc_rate = inno_hdmi_rk3228_phy_pll_recalc_rate,
};

static const struct inno_hdmi_phy_ops rk3328_hdmi_phy_ops = {
	.init = inno_hdmi_phy_rk3328_init,
	.power_on = inno_hdmi_phy_rk3328_power_on,
	.power_off = inno_hdmi_phy_rk3328_power_off,
	.pre_pll_update = inno_hdmi_phy_rk3328_pre_pll_update,
	.recalc_rate = inno_hdmi_rk3328_phy_pll_recalc_rate,
};

static const struct inno_hdmi_phy_drv_data rk3228_hdmi_phy_drv_data = {
	.dev_type = INNO_HDMI_PHY_RK3228,
	.ops = &rk3228_hdmi_phy_ops,
	.phy_cfg_table = rk3228_phy_cfg,
};

static const struct inno_hdmi_phy_drv_data rk3328_hdmi_phy_drv_data = {
	.dev_type = INNO_HDMI_PHY_RK3328,
	.ops = &rk3328_hdmi_phy_ops,
	.phy_cfg_table = rk3328_phy_cfg,
};

static const struct of_device_id inno_hdmi_phy_of_match[] = {
	{ .compatible = "rockchip,rk3228-hdmi-phy",
	  .data = &rk3228_hdmi_phy_drv_data
	},
	{ .compatible = "rockchip,rk3328-hdmi-phy",
	  .data = &rk3328_hdmi_phy_drv_data
	},
	{}
};
MODULE_DEVICE_TABLE(of, inno_hdmi_phy_of_match);

static const struct regmap_config inno_hdmi_phy_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0x400,
};

static
int inno_hdmi_update_phy_table(struct inno_hdmi_phy *inno, u32 *config,
			       struct phy_config *phy_cfg,
			       int phy_table_size)
{
	int i, j;

	for (i = 0; i < phy_table_size; i++) {
		phy_cfg[i].tmdsclock =
			(unsigned long)config[i * 15];

		for (j = 0; j < 14; j++)
			phy_cfg[i].regs[j] = (u8)config[i * 15 + 1 + j];
	}

	/*
	 * The last set of phy cfg is used to indicate whether
	 * there is no more phy cfg data.
	 */
	phy_cfg[i].tmdsclock = ~0UL;
	for (j = 0; j < 14; j++)
		phy_cfg[i].regs[j] = 0;

	return 0;
}

#define PHY_TAB_LEN 60

static int inno_hdmi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct inno_hdmi_phy *inno;
	const struct of_device_id *match;
	struct phy_provider *phy_provider;
	struct resource *res;
	void __iomem *regs;
	u32 *phy_config;
	int ret, val, phy_table_size;

	inno = devm_kzalloc(dev, sizeof(*inno), GFP_KERNEL);
	if (!inno)
		return -ENOMEM;

	inno->dev = dev;

	match = of_match_node(inno_hdmi_phy_of_match, pdev->dev.of_node);
	inno->plat_data = (struct inno_hdmi_phy_drv_data *)match->data;
	if (!inno->plat_data || !inno->plat_data->ops)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	inno->sysclk = devm_clk_get(inno->dev, "sysclk");
	if (IS_ERR(inno->sysclk)) {
		ret = PTR_ERR(inno->sysclk);
		dev_err(inno->dev, "Unable to get inno phy sysclk: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(inno->sysclk);
	if (ret) {
		dev_err(inno->dev, "Cannot enable inno phy sysclk: %d\n", ret);
		return ret;
	}

	inno->regmap = devm_regmap_init_mmio(dev, regs,
					     &inno_hdmi_phy_regmap_config);
	if (IS_ERR(inno->regmap)) {
		ret = PTR_ERR(inno->regmap);
		dev_err(dev, "failed to init regmap: %d\n", ret);
		goto err_regsmap;
	}

	inno->phy = devm_phy_create(dev, NULL, &inno_hdmi_phy_ops);
	if (IS_ERR(inno->phy)) {
		dev_err(dev, "failed to create HDMI PHY\n");
		ret = PTR_ERR(inno->phy);
		goto err_regsmap;
	}

	if (of_get_property(np, "rockchip,phy-table", &val)) {
		if (val % PHY_TAB_LEN || !val) {
			dev_err(dev, "Invalid phy cfg table format!\n");
			return -EINVAL;
		}

		phy_config = kmalloc(val, GFP_KERNEL);
		if (!phy_config) {
			dev_err(dev, "kmalloc phy table failed\n");
			return -ENOMEM;
		}

		phy_table_size = val / PHY_TAB_LEN;
		/* Effective phy cfg data and the end of phy cfg table */
		inno->phy_cfg = devm_kzalloc(dev, val + PHY_TAB_LEN,
					     GFP_KERNEL);
		if (!inno->phy_cfg) {
			kfree(phy_config);
			return -ENOMEM;
		}
		of_property_read_u32_array(np, "rockchip,phy-table",
					   phy_config, val / sizeof(u32));
		ret = inno_hdmi_update_phy_table(inno, phy_config,
						 inno->phy_cfg,
						 phy_table_size);
		if (ret) {
			kfree(phy_config);
			return ret;
		}
		kfree(phy_config);
	} else {
		dev_dbg(dev, "use default hdmi phy table\n");
	}

	phy_set_drvdata(inno->phy, inno);
	phy_set_bus_width(inno->phy, 8);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "failed to register PHY provider\n");
		ret = PTR_ERR(phy_provider);
		goto err_regsmap;
	}

	if (inno->plat_data->ops->init)
		inno->plat_data->ops->init(inno);

	ret = inno_hdmi_phy_clk_register(inno);
	if (ret)
		goto err_regsmap;

	inno->irq = platform_get_irq(pdev, 0);
	if (inno->irq > 0) {
		ret = devm_request_threaded_irq(inno->dev, inno->irq,
						inno_hdmi_phy_hardirq,
						inno_hdmi_phy_irq, IRQF_SHARED,
						dev_name(inno->dev), inno);
		if (ret)
			goto err_irq;
	}
	platform_set_drvdata(pdev, inno);
	return 0;

err_irq:
	of_clk_del_provider(pdev->dev.of_node);
err_regsmap:
	clk_disable_unprepare(inno->sysclk);
	return ret;
}

static int inno_hdmi_phy_remove(struct platform_device *pdev)
{
	struct inno_hdmi_phy *inno = platform_get_drvdata(pdev);

	of_clk_del_provider(pdev->dev.of_node);
	clk_disable_unprepare(inno->sysclk);
	return 0;
}

static struct platform_driver inno_hdmi_phy_driver = {
	.probe  = inno_hdmi_phy_probe,
	.remove = inno_hdmi_phy_remove,
	.driver = {
		.name = "inno-hdmi-phy",
		.of_match_table = of_match_ptr(inno_hdmi_phy_of_match),
	},
};

module_platform_driver(inno_hdmi_phy_driver);

MODULE_DESCRIPTION("Innosilion HDMI 2.0 Transmitter PHY Driver");
MODULE_LICENSE("GPL v2");
