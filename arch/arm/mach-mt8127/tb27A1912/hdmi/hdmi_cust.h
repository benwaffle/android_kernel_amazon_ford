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

#ifndef HDMI_CUSTOMIZATION_H
#define HDMI_CUSTOMIZATION_H

/******************************************************************
** scale adjustment
******************************************************************/
/*
#define USING_SCALE_ADJUSTMENT

///if mhl chip CI2CA pin is pull up, the folloing should be defined. otherwise, please mask it.
#define SII_I2C_ADDR  (0x72)
#define HDMI_I2C_CHANNEL 3
*/



/******************************************************************
** MHL GPIO Customization
******************************************************************/
//#define MHL_PHONE_GPIO_REUSAGE
void ChangeGPIOToI2S();
void ChangeI2SToGPIO();

int cust_hdmi_power_on(int on);

int cust_hdmi_dpi_gpio_on(int on);

int cust_hdmi_i2s_gpio_on(int on);

int get_hdmi_i2c_addr(void);

int get_hdmi_i2c_channel(void);


#endif
