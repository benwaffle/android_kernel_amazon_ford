 /* MediaTek Inc. (C) 2010. All rights reserved. */ 

#ifndef BUILD_LK
#include <linux/string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <platform/upmu_common.h>
#include <platform/mt_i2c.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/upmu_common.h>
#endif
#include "lcm_drv.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (1024)
#define FRAME_HEIGHT (600)



//#define GPIO_LCD_PWR_EN      GPIO3
#define GPIO_LCD_3V3_EN                     GPIO83
#define GPIO_LCD_PWR_EN                     GPIO44
#define GPIO_LCD_RST_EN                     GPIO89
#define GPIO_LCD_STB_EN                  	GPIO88

#define HSYNC_PULSE_WIDTH 10 
#define HSYNC_BACK_PORCH  160
#define HSYNC_FRONT_PORCH 160
#define VSYNC_PULSE_WIDTH 10
#define VSYNC_BACK_PORCH  23
#define VSYNC_FRONT_PORCH 12

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    


static void lcd_power_en(unsigned char enabled)
{
#ifndef BUILD_LK
	printk("[IND][K] %s : %s\n", __func__, enabled ? "on" : "off");
#else
	printf("[IND][LK] %s : %s\n", __func__, enabled ? "on" : "off");
#endif

	if (enabled)
	{
		mt_set_gpio_mode(GPIO_LCD_3V3_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_3V3_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_3V3_EN, GPIO_OUT_ONE);
		MDELAY(5);
		mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	}
	else
	{
		mt_set_gpio_mode(GPIO_LCD_3V3_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_3V3_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_3V3_EN, GPIO_OUT_ZERO);
		MDELAY(5);
		mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
	}
}

#if 1
static void lcd_reset(unsigned char enabled)
{
#ifndef BUILD_LK
    printk("[IND][K] %s : %s\n", __func__, enabled ? "on" : "off");
#else
    printf("[IND][LK] %s : %s\n", __func__, enabled ? "on" : "off");
#endif

    if (enabled)
    {
        mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
		MDELAY(1);
		mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
    }
    else
    {	
        mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
		MDELAY(1);
        mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ZERO);   
    }
}
#endif

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DPI;
	params->ctrl   = LCM_CTRL_SERIAL_DBI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->io_select_mode = 0; 

	params->physical_width = 152;
	params->physical_height = 86;
// div0_real = div0==0 ? 1:
//			div0==1 ? 2: 4;
// div1_real = div1==0 ? 1:
//			div1==1 ? 2: 4;
// freq = 26*mipi_pll_clk_ref/2^24/div0_real/div1_real/8
#if 0
	/* RGB interface configurations */
	params->dpi.mipi_pll_clk_ref  = 536870912;
	params->dpi.mipi_pll_clk_div1 = 0;	// div0=0,1,2,3;div0_real=1,2,4,4
	params->dpi.mipi_pll_clk_div2 = 0;	// div1=0,1,2,3;div1_real=1,2,4,4
	params->dpi.dpi_clk_div 	  = 4;			 //{4,2}, pll/4=51.025M
	params->dpi.dpi_clk_duty	  = 2;
#endif
	params->dpi.PLL_CLOCK = 52;  //67MHz

    params->dpi.clk_pol           = LCM_POLARITY_FALLING;
    params->dpi.de_pol            = LCM_POLARITY_RISING;
    params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
    params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

	params->dpi.hsync_pulse_width = HSYNC_PULSE_WIDTH;
	params->dpi.hsync_back_porch  = HSYNC_BACK_PORCH;
	params->dpi.hsync_front_porch = HSYNC_FRONT_PORCH;
	params->dpi.vsync_pulse_width = VSYNC_PULSE_WIDTH;
	params->dpi.vsync_back_porch  = VSYNC_BACK_PORCH;
	params->dpi.vsync_front_porch = VSYNC_FRONT_PORCH;
	
    params->dpi.lvds_tx_en = 1;
    params->dpi.ssc_disable = 1;
    params->dpi.format            = LCM_DPI_FORMAT_RGB888;   // format is 24 bit
    params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
    params->dpi.is_serial_output  = 0;

    params->dpi.intermediat_buffer_num = 0;

    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_2MA;
}

extern void DSI_clk_HS_mode(unsigned char enter);

//extern void DSI_Continuous_HS(void);

static void init_lcm_registers(void)
{
	  
#ifndef BUILD_LK
    printk("[IND][K] %s\n", __func__);
#else
    printf("[IND][LK] %s\n", __func__);
#endif
	  
}

static void lcm_init(void)
{
#ifndef BUILD_LK
	printk("[IND][K] %s\n", __func__);
#else
	printf("[IND][LK] %s\n", __func__);
#endif

	lcd_reset(0);
	MDELAY(50);
	lcd_reset(1);

	upmu_set_rg_vgp1_vosel(0x5);
	upmu_set_rg_vgp1_en(0x1);

	MDELAY(20);

	lcd_power_en(1);
	MDELAY(50);

}

static void lcm_suspend(void)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:suspend=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif

	mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ZERO);
	MDELAY(100);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(10);
}

static void lcm_suspend_power(void)
{
#ifndef BUILD_LK
    printk("[IND][K] %s\n", __func__);
#else
    printf("[IND][LK] %s\n", __func__);
#endif

	upmu_set_rg_vgp1_en(0x0);
#ifndef BUILD_LK
		hwPowerDown(MT6323_POWER_LDO_VGP1,"lcm");
#endif

    MDELAY(5);
    lcd_power_en(0);
}

static void lcm_resume(void)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
	snprintf(buf, sizeof(buf), "%s:lcd:resume=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
#endif

	lcd_reset(0);
	MDELAY(50);
	lcd_reset(1);

#ifndef BUILD_LK
	printk("[IND][K] %s\n", __func__);
	hwPowerOn(MT6323_POWER_LDO_VGP1,VOL_2800,"lcm");
#else
	printf("[IND][LK] %s\n", __func__);
#endif
	MDELAY(20);
	lcd_power_en(1);
	MDELAY(50);
}

static int lcm_compare_id(void)
{
	return 1;
}


LCM_DRIVER hx8282_a01_lvds_dpi_vdo_lcm_drv_txd = 
{
	.name		= "hx8282_a01_lvds_dpi_vdo_txd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params 	= lcm_get_params,
	.init			= lcm_init,
	.suspend		= lcm_suspend,
	.suspend_power  = lcm_suspend_power,
	.resume 		= lcm_resume,
	.compare_id    = lcm_compare_id,

};

