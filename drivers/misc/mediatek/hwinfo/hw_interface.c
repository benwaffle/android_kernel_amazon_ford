/*
* Copyright (C) 2011-2014 Huaqin Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>

#include <mach/mt_gpio.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot_common.h>

/******************************************************************************
 **** Global Marco
******************************************************************************/
#define ADC_BOARD_ID	(15)	/* ADC_BORRD_ID is AUX_IN5 and in ADC the channel is 15 */
#define BUILD_VARIANT_GPIO_NUM	(50)

/******************************************************************************
 **** Some struct defenition
******************************************************************************/
struct board_id_information {
	int initialized;
	int adc_channel;
	int voltage;
	char *built_name;
	char *id;
};

static struct board_id_information board_id;
extern struct msdc_host *mtk_msdc_host[3];

/******************************************************************************
****  build variant attribute group
*******************************************************************************/

static int get_build_variant_gpio_level(void)
{
	mt_set_gpio_mode(BUILD_VARIANT_GPIO_NUM, GPIO_MODE_GPIO);
	mt_set_gpio_dir(BUILD_VARIANT_GPIO_NUM, GPIO_DIR_IN);
	return mt_get_gpio_in(BUILD_VARIANT_GPIO_NUM);
}

static ssize_t show_gpio_num(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int ret_value = 1;
	ret_value = sprintf(buf, "%d\n", BUILD_VARIANT_GPIO_NUM);
	return ret_value;
}

static ssize_t show_gpio_level(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
	ret_value = sprintf(buf, "%d\n", get_build_variant_gpio_level());
	return ret_value;
}

static ssize_t show_build_variant(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
	int gpio_level = get_build_variant_gpio_level();
	ret_value = sprintf(buf, "%s\n", gpio_level ? "SHIP" : "ENG");
	return ret_value;
}

static DEVICE_ATTR(gpio_num, 0644, show_gpio_num, NULL);
static DEVICE_ATTR(gpio_level, 0644, show_gpio_level, NULL);
static DEVICE_ATTR(build_variant, 0644, show_build_variant, NULL);

static struct attribute *build_variant_sysfs_entries[] = {
	&dev_attr_gpio_num.attr,
	&dev_attr_gpio_level.attr,
	&dev_attr_build_variant.attr,
	NULL
};

static struct attribute_group build_variant_attribute_group = {
	.name = "build_variant",
	.attrs = build_variant_sysfs_entries,
};

/******************************************************************************
****  board_id attribute group
*******************************************************************************/

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);

static int init_board_id(void)
{
	if (1 == board_id.initialized)
		return 1;
	else {
		int data[4];
		int voltage = 0;
		int ret = 0;
		board_id.adc_channel = ADC_BOARD_ID;	/* ADC_BORRD_ID is AUX_IN5 and in ADC the channel is 15 */

		ret =
		    IMM_GetOneChannelValue(board_id.adc_channel, data,
					   &voltage);
		if (0 != ret) {
			printk(KERN_WARNING
			       "init_board_id get ADC error: %d\n", ret);
			return 0;
		}

		board_id.voltage = (voltage * 1500) / 4096;
		/* MTK's suggestion we convert to a valid voltage */

#ifdef CONFIG_AUSTIN_PROJECT
		if (board_id.voltage < 150) {	/* HVT 0V */
			board_id.built_name = "HVT";
			board_id.id = "0037001010000017";
		} else if (board_id.voltage < 322) { /* EVT1.0 0.224V */
			board_id.built_name = "EVT1.0";
			board_id.id = "0037001020000017";
		} else if (board_id.voltage < 570) { /* EVT1.1 0.42V */
			board_id.built_name = "EVT1.1";
			board_id.id = "0037001030000017";
		} else if (board_id.voltage < 810) { /* EVT1.2 0.72V */
			board_id.built_name = "EVT1.2";
			board_id.id = "0037001040000017";
		} else if (board_id.voltage < 1005) {	/* DVT1.0 0.9V */
			board_id.built_name = "DVT1.0";
			board_id.id = "0037001050000017";
		} else if (board_id.voltage < 1230) {	/* DVT1.1 1.11V */
			board_id.built_name = "DVT1.1";
			board_id.id = "0037001060000017";
		} else if (board_id.voltage < 1575) {	/* PVT 1.35V */
			board_id.built_name = "PVT";
			board_id.id = "0037001070000017";
		} else {
			board_id.built_name = "unknow stage";
			board_id.id = "0037001000000017";
		}
#else
		if (board_id.voltage < 300) {	/* HVT 0V */
			board_id.built_name = "HVT";
			board_id.id = "0025001010000015";
		} else if (board_id.voltage < 570) {	/* EVT1 0.42V */
			board_id.built_name = "EVT1";
			board_id.id = "0025001020000015";
		} else if (board_id.voltage < 810) {	/* EVT2 0.72V */
			board_id.built_name = "EVT2";
			board_id.id = "0025001020000015";
		} else if (board_id.voltage < 1005) {	/* DVT1 0.9V */
			board_id.built_name = "DVT1";
			board_id.id = "0025001030000015";
		} else if (board_id.voltage < 1230) {	/* DVT2 1.11V */
			board_id.built_name = "DVT2";
			board_id.id = "0025001030000015";
		} else if (board_id.voltage < 1500) {	/* EVT 1.35V */
			board_id.built_name = "PVT";
			board_id.id = "0025001040000015";
		} else {
			board_id.built_name = "unknow stage";
			board_id.id = "0025001000000015";
		}
#endif
		board_id.initialized = 1;
		return 1;
	}
}

static ssize_t show_adc_channel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
	ret_value = sprintf(buf, "%d\n", ADC_BOARD_ID);
	return ret_value;
}

static ssize_t show_voltage(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	int ret_value = 1;
	init_board_id();
	ret_value = sprintf(buf, "%d\n", board_id.voltage);
	return ret_value;
}

static ssize_t show_built_name(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
	init_board_id();
	ret_value = sprintf(buf, "%s\n", board_id.built_name);
	return ret_value;
}

static ssize_t show_id(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret_value = 1;
	init_board_id();
	ret_value = sprintf(buf, "%s\n", board_id.id);
	return ret_value;
}

static DEVICE_ATTR(adc_channel, 0644, show_adc_channel, NULL);
static DEVICE_ATTR(voltage, 0644, show_voltage, NULL);
static DEVICE_ATTR(built_name, 0644, show_built_name, NULL);
static DEVICE_ATTR(id, 0644, show_id, NULL);

static struct attribute *board_id_sysfs_entries[] = {
	&dev_attr_adc_channel.attr,
	&dev_attr_voltage.attr,
	&dev_attr_built_name.attr,
	&dev_attr_id.attr,
	NULL
};

static struct attribute_group board_id_attribute_group = {
	.name = "board_id",
	.attrs = board_id_sysfs_entries,
};


/******************************************************************************
****  device informations
*******************************************************************************/

extern unsigned int msdc_get_capacity(int get_emmc_total);
unsigned int round_kbytes_to_readable_mbytes(unsigned int k)
{
	unsigned int r_size_m = 0;
	if (k > 64 * 1024 * 1024)
		r_size_m = 128 * 1024;
	else if (k > 32 * 1024 * 1024)
		r_size_m = 64 * 1024;
	else if (k > 16 * 1024 * 1024)
		r_size_m = 32 * 1024;
	else if (k > 8 * 1024 * 1024)
		r_size_m = 16 * 1024;
	else if (k > 4 * 1024 * 1024)
		r_size_m = 8 * 1024;
	else if (k > 2 * 1024 * 1024)
		r_size_m = 4 * 1024;
	else if (k > 1 * 1024 * 1024)
		r_size_m = 2 * 1024;
	else if (k > 512 * 1024)
		r_size_m = 1024;
	else if (k > 256 * 1024)
		r_size_m = 512;
	else if (k > 128 * 1024)
		r_size_m = 256;
	else
		k = 0;
	return r_size_m;
}

static ssize_t show_ram_size(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int ret_value = 1;
#define K(x) ((x) << (PAGE_SHIFT - 10))
	struct sysinfo i;
	si_meminfo(&i);

	if (round_kbytes_to_readable_mbytes(K(i.totalram)) >= 1024) {
		ret_value =
		    sprintf(buf, "%dGB",
			    round_kbytes_to_readable_mbytes(K(i.totalram)) /
			    1024);
	} else {
		ret_value =
		    sprintf(buf, "%dMB",
			    round_kbytes_to_readable_mbytes(K(i.totalram)));
	}

	return ret_value;
}

static ssize_t show_emmc_size(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value =
	    sprintf(buf, "%dGB",
		    round_kbytes_to_readable_mbytes(msdc_get_capacity(1) / 2) /
		    1024);
	return ret_value;

}

static DEVICE_ATTR(ram_size, 0644, show_ram_size, NULL);
static DEVICE_ATTR(emmc_size, 0644, show_emmc_size, NULL);

static struct attribute *device_information_sysfs_entries[] = {
	&dev_attr_ram_size.attr,
	&dev_attr_emmc_size.attr,
	NULL
};

static struct attribute_group device_information_attribute_group = {
	.name = "device_information",
	.attrs = device_information_sysfs_entries,
};

/******************************************************************************************
*******************************************************************************************
*******************************************************************************************/
static int hw_interface_driver_probe(struct platform_device *dev)
{
	int error = 0;

	error = sysfs_create_group(&dev->dev.kobj, &board_id_attribute_group);
	if (error) {
		printk
		    (KERN_WARNING
		     "failed to create sysfs device attributes board_id_attribute_group\n");
		goto exit_error;
	}

	error =
	    sysfs_create_group(&dev->dev.kobj, &build_variant_attribute_group);
	if (error) {
		printk
		    (KERN_WARNING
		     "failed to create sysfs device attributes build_variant_attribute_group\n");
		goto error_2;
	}

	error =
	    sysfs_create_group(&dev->dev.kobj,
			       &device_information_attribute_group);
	if (error) {
		printk
		    (KERN_WARNING
		     "failed to create sysfs device attributes device_information_attribute_group\n");
		goto error_1;
	}
	goto exit_error;

error_1:
	sysfs_remove_group(&dev->dev.kobj, &build_variant_attribute_group);
error_2:
	sysfs_remove_group(&dev->dev.kobj, &board_id_attribute_group);
exit_error:
	return error;
}

static int hw_interface_driver_remove(struct platform_device *dev)
{
	printk(KERN_WARNING "** hw_interface_drvier_remove!! **");

	sysfs_remove_group(&dev->dev.kobj, &board_id_attribute_group);
	sysfs_remove_group(&dev->dev.kobj, &build_variant_attribute_group);
	sysfs_remove_group(&dev->dev.kobj, &device_information_attribute_group);

	return 0;
}

static struct platform_driver hw_interface_driver = {
	.probe = hw_interface_driver_probe,
	.remove = hw_interface_driver_remove,
	.driver = {
		   .name = "hw_interface",
		   },
};

static struct platform_device hw_interface_device = {
	.name = "hw_interface",
	.id = -1,
};

static int __init hw_interface_mod_init(void)
{
	int ret = 0;
	ret = platform_device_register(&hw_interface_device);
	if (ret) {
		printk
		    (KERN_WARNING
		     "**hw_interface_mod_init  Unable to driver register(%d)\n",
		     ret);
		goto fail_2;
	}

	ret = platform_driver_register(&hw_interface_driver);
	if (ret) {
		printk
		    (KERN_WARNING
		     "**hw_interface_mod_init  Unable to driver register(%d)\n",
		     ret);
		goto fail_1;
	}

	goto ok_result;

fail_1:
	platform_driver_unregister(&hw_interface_driver);
fail_2:
	platform_device_unregister(&hw_interface_device);
ok_result:
	return ret;
}

/*****************************************************************************/
static void __exit hw_interface_mod_exit(void)
{
	platform_driver_unregister(&hw_interface_driver);
	platform_device_unregister(&hw_interface_device);
}

/*****************************************************************************/
module_init(hw_interface_mod_init);
module_exit(hw_interface_mod_exit);
/*****************************************************************************/
