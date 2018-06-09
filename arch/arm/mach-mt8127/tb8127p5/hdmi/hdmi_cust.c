/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <cust_gpio_usage.h>
#include <hdmi_cust.h>
#include <mach/mt_gpio.h>

/******************************************************************
** Basic define
******************************************************************/
#ifndef s32
	#define s32 signed int
#endif
#ifndef s64
	#define s64 signed long long
#endif

static bool cust_power_on = false;
int cust_hdmi_power_on(int on)
{
/*
	if(on > 0) 
	{
	    printk("MHL_Power power %x, rst %x \n" ,GPIO_MHL_POWER_CTRL_PIN, GPIO_MHL_RST_B_PIN);
	    mt_set_gpio_mode(GPIO_MHL_POWER_CTRL_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_MHL_POWER_CTRL_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
#ifdef PMIC_APP_MHL_POWER_LDO1
		if(cust_power_on == false)
		{
			hwPowerOn(PMIC_APP_MHL_POWER_LDO1, VOL_1200,"MHL");
			cust_power_on = true;
		}
#else
		printk("Error: PMIC_APP_MHL_POWER_LDO1 not defined -\n" );
#endif
	}
	else
	{
#ifdef PMIC_APP_MHL_POWER_LDO1
		if(cust_power_on == true)
		{
			hwPowerDown(PMIC_APP_MHL_POWER_LDO1, "MHL");
			cust_power_on = false;
		}
#endif
	}
*/
	return 0;
}


int cust_hdmi_dpi_gpio_on(int on)
{
/*
    unsigned int dpi_pin_start = 0;
    if(on > 0)
    {        	  
#ifdef GPIO_EXT_DISP_DPI0_PIN
		for(dpi_pin_start = GPIO_EXT_DISP_DPI0_PIN; dpi_pin_start < GPIO_EXT_DISP_DPI0_PIN + 16; dpi_pin_start++)
		{
			mt_set_gpio_mode(dpi_pin_start, GPIO_MODE_01);
		}
		printk("%s, %d GPIO_EXT_DISP_DPI0_PIN is defined+ %x\n", __func__, __LINE__, GPIO_EXT_DISP_DPI0_PIN);
#else
		printk("%s,%d Error: GPIO_EXT_DISP_DPI0_PIN is not defined\n", __func__, __LINE__);
#endif
	
    }
    else
    {
#ifdef GPIO_EXT_DISP_DPI0_PIN
		for(dpi_pin_start = GPIO_EXT_DISP_DPI0_PIN; dpi_pin_start < GPIO_EXT_DISP_DPI0_PIN + 16; dpi_pin_start++)
		{
			mt_set_gpio_mode(dpi_pin_start, GPIO_MODE_00);
			mt_set_gpio_dir(dpi_pin_start, GPIO_DIR_IN);
			mt_set_gpio_pull_enable(dpi_pin_start, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(dpi_pin_start, GPIO_PULL_DOWN);
		}
		printk("%s, %d GPIO_EXT_DISP_DPI0_PIN is defined- %x\n", __func__, __LINE__, GPIO_EXT_DISP_DPI0_PIN);
#endif
	}
*/
	return 0;
}

int cust_hdmi_i2s_gpio_on(int on)
{
/*
    if(on > 0)
    {
#ifdef GPIO_MHL_I2S_OUT_WS_PIN
        mt_set_gpio_mode(GPIO_MHL_I2S_OUT_WS_PIN, GPIO_MHL_I2S_OUT_WS_PIN_M_I2S3_WS);
        mt_set_gpio_mode(GPIO_MHL_I2S_OUT_CK_PIN, GPIO_MHL_I2S_OUT_CK_PIN_M_I2S3_BCK);
        mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MHL_I2S_OUT_DAT_PIN_M_I2S3_DO);
#else
        printk("%s,%d Error. GPIO_MHL_I2S_OUT_WS_PIN is not defined\n", __func__, __LINE__);
#endif
    }
    else
    {
#ifdef GPIO_MHL_I2S_OUT_WS_PIN
        mt_set_gpio_pull_enable(GPIO_MHL_I2S_OUT_WS_PIN, GPIO_PULL_DISABLE);
        mt_set_gpio_pull_enable(GPIO_MHL_I2S_OUT_CK_PIN, GPIO_PULL_DISABLE);
        mt_set_gpio_pull_enable(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_PULL_DISABLE);
#endif
    }
*/
    return 0;
}

int get_hdmi_i2c_addr(void)
{
//    return (SII_I2C_ADDR);
	return 0;
}

int get_hdmi_i2c_channel(void)
{
//    return (HDMI_I2C_CHANNEL);
	return 0;
}