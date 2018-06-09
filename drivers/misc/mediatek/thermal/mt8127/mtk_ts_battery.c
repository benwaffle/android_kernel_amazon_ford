/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/err.h>

#include <mach/system.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#define MTKTSBATTERY_TEMP_CRIT 60000
static DEFINE_MUTEX(therm_lock);

struct mtktsbattery_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

static int get_hw_battery_temp(void)
{
	int ret=0;
#if defined(CONFIG_POWER_EXT)
	ret = -1270;
#else
	ret = read_tbat_value();
	ret = ret * 10;
#endif
	return ret;
}

static DEFINE_MUTEX(Battery_lock);
int ts_battery_at_boot_time = 0;

static int mtktsbattery_get_hw_temp(void)
{
	int t_ret = 0;
	static int battery[60];
	static int counter, first_time;


	if (ts_battery_at_boot_time == 0) {
		ts_battery_at_boot_time = 1;
		battery[counter] = 25000;
		counter++;
		return 25000;
	}

	mutex_lock(&Battery_lock);
	t_ret = get_hw_battery_temp();
	t_ret = t_ret * 100;

	mutex_unlock(&Battery_lock);

	if (t_ret) {
		pr_debug("%s counter=%d, first_time =%d\n", __func__, counter, first_time);
		pr_debug("%s T_Battery, %d\n", __func__, t_ret);
	}

	return t_ret;
}

static int mtktsbattery_get_temp(struct thermal_zone_device *thermal,
			       unsigned long *t)
{
	*t = mtktsbattery_get_hw_temp();
	return 0;
}

static int mtktsbattery_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktsbattery_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktsbattery_get_trip_type(struct thermal_zone_device *thermal,
				   int trip,
				   enum thermal_trip_type *type)
{
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktsbattery_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long *t)
{
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktsbattery_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	for (i = 0; i < pdata->num_trips; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*t = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static int mtktsbattery_set_trip_temp(struct thermal_zone_device *thermal,
				int trip,
				unsigned long t)
{
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static struct thermal_zone_device_ops mtktsbattery_dev_ops = {
	.get_temp = mtktsbattery_get_temp,
	.get_mode = mtktsbattery_get_mode,
	.set_mode = mtktsbattery_set_mode,
	.get_trip_type = mtktsbattery_get_trip_type,
	.get_trip_temp = mtktsbattery_get_trip_temp,
	.get_crit_temp = mtktsbattery_get_crit_temp,
	.set_trip_temp = mtktsbattery_set_trip_temp,
};

static void mtktsbattery_work(struct work_struct *work)
{
	struct mtktsbattery_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktsbattery_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int mtktsbattery_read_temp(struct thermal_dev *tdev)
{
	return mtktsbattery_get_hw_temp();
}

static struct thermal_dev_ops mtktsbattery_fops = {
	.get_temp = mtktsbattery_read_temp,
};

#ifdef CONFIG_AUSTIN_PROJECT
struct thermal_dev_params mtktsbattery_tdp = {
	.offset = -13000,
	.alpha = 7,
	.weight = 500
};
#else
struct thermal_dev_params mtktsbattery_tdp = {
	.offset = -9500,
	.alpha = 7,
	.weight = 437
};
#endif

static int mtktsbattery_show_params(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tzone->therm_fw->tdp->offset,
		       tzone->therm_fw->tdp->alpha,
		       tzone->therm_fw->tdp->weight);
}

static ssize_t mtktsbattery_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktsbattery_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	if (sscanf(buf, "%s %d", param, &value) == 2) {
		if (!strcmp(param, "offset"))
			tzone->therm_fw->tdp->offset = value;
		if (!strcmp(param, "alpha"))
			tzone->therm_fw->tdp->alpha = value;
		if (!strcmp(param, "weight"))
			tzone->therm_fw->tdp->weight = value;
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(params, S_IRUGO | S_IWUSR, mtktsbattery_show_params, mtktsbattery_store_params);

static int mtktsbattery_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktsbattery_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata)
		return -EINVAL;

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktsbattery",
						 pdata->num_trips,
						 1,
						 tzone,
						 &mtktsbattery_dev_ops,
						 NULL,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktsbattery thermal zone device\n", __func__);
		return -EINVAL;
	}
	tzone->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (!tzone->therm_fw)
		return -ENOMEM;
	tzone->therm_fw->name = "mtktsbattery";
	tzone->therm_fw->dev = &(pdev->dev);
	tzone->therm_fw->dev_ops = &mtktsbattery_fops;
	tzone->therm_fw->tdp = &mtktsbattery_tdp;

	ret = thermal_dev_register(tzone->therm_fw);
	if (ret) {
		pr_err("Error registering therml mtktsbattery device\n");
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtktsbattery_work);
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktsbattery_remove(struct platform_device *pdev)
{
	struct mtktsbattery_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktsbattery_driver = {
	.probe = mtktsbattery_probe,
	.remove = mtktsbattery_remove,
	.driver     = {
		.name  = "mtktsbattery",
		.owner = THIS_MODULE,
	},
};

static struct mtk_thermal_platform_data mtktsbattery_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {.temp = MTKTSBATTERY_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static struct platform_device mtktsbattery_device = {
	.name = "mtktsbattery",
	.id = -1,
	.dev = {
		.platform_data = &mtktsbattery_thermal_data,
	},
};

static int __init mtktsbattery_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktsbattery_driver);
	if (ret) {
		pr_err("Unable to register mtktsbattery thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktsbattery_device);
	if (ret) {
		pr_err("Unable to register mtktsbattery device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktsbattery_exit(void)
{
	platform_driver_unregister(&mtktsbattery_driver);
	platform_device_unregister(&mtktsbattery_device);
}

module_init(mtktsbattery_init);
module_exit(mtktsbattery_exit);
