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

#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/err.h>

#include <mach/system.h>
#include "mach/mt_typedefs.h"
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#include "mach/mt_thermal.h"

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#define MTKTSPMIC_TEMP_CRIT 150000      /* 150.000 degree Celsius */
static DEFINE_MUTEX(therm_lock);

struct mtktspmic_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

/* Cali */
static kal_int32 g_o_vts;
static kal_int32 g_degc_cali;
static kal_int32 g_adc_cali_en;
static kal_int32 g_o_slope;
static kal_int32 g_o_slope_sign;
static kal_int32 g_id;
static kal_int32 g_slope1;
static kal_int32 g_slope2;
static kal_int32 g_intercept;

#define y_pmic_repeat_times	1

static u16 pmic_read(u16 addr)
{
	u32 rdata = 0;
	pwrap_read((u32)addr, &rdata);
	return (u16)rdata;
}

static void pmic_cali_prepare(void)
{
	kal_uint32 temp0, temp1;

	temp0 = pmic_read(0x63A);
	temp1 = pmic_read(0x63C);

	pr_info("Power/PMIC_Thermal: Reg(0x63a)=0x%x, Reg(0x63c)=0x%x\n", temp0, temp1);

	g_o_vts = ((temp1&0x001F) << 8) + ((temp0>>8) & 0x00FF);
	g_degc_cali = (temp0>>2) & 0x003f;
	g_adc_cali_en = (temp0>>1) & 0x0001;
	g_o_slope_sign = (temp1>>5) & 0x0001;

	/*
	  CHIP ID
	  E1 : 16'h1023
	  E2 : 16'h2023
	  E3 : 16'h3023
	*/
	if (upmu_get_cid() == 0x1023) {
		g_id = (temp1>>12) & 0x0001;
		g_o_slope = (temp1>>6) & 0x003f;
	} else {
		g_id = (temp1>>14) & 0x0001;
		g_o_slope = (((temp1>>11) & 0x0007) << 3) + ((temp1>>6) & 0x007);
	}

	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en == 0) {
		g_o_vts = 3698;
		g_degc_cali = 50;
		g_o_slope = 0;
		g_o_slope_sign = 0;
	}
	pr_info("Power/PMIC_Thermal: g_ver= 0x%x, g_o_vts = 0x%x, g_degc_cali = 0x%x, g_adc_cali_en = 0x%x, g_o_slope = 0x%x, g_o_slope_sign = 0x%x, g_id = 0x%x\n",
		upmu_get_cid(), g_o_vts, g_degc_cali, g_adc_cali_en, g_o_slope, g_o_slope_sign, g_id);
	pr_info("Power/PMIC_Thermal: chip ver       = 0x%x\n", upmu_get_cid());
	pr_info("Power/PMIC_Thermal: g_o_vts        = 0x%x\n", g_o_vts);
	pr_info("Power/PMIC_Thermal: g_degc_cali    = 0x%x\n", g_degc_cali);
	pr_info("Power/PMIC_Thermal: g_adc_cali_en  = 0x%x\n", g_adc_cali_en);
	pr_info("Power/PMIC_Thermal: g_o_slope      = 0x%x\n", g_o_slope);
	pr_info("Power/PMIC_Thermal: g_o_slope_sign = 0x%x\n", g_o_slope_sign);
	pr_info("Power/PMIC_Thermal: g_id           = 0x%x\n", g_id);
}


static void pmic_cali_prepare2(void)
{
	kal_int32 vbe_t;
	g_slope1 = (100 * 1000);	/*1000 is for 0.001 degree*/

	if (g_o_slope_sign == 0)
		g_slope2 = -(171+g_o_slope);
	else
		g_slope2 = -(171-g_o_slope);

	vbe_t = (-1) * (((g_o_vts + 9102)*1800)/32768) * 1000;
	if (g_o_slope_sign == 0)
		g_intercept = (vbe_t * 100) / (-(171+g_o_slope));       /*0.001 degree*/
	else
		g_intercept = (vbe_t * 100) / (-(171-g_o_slope));  /*0.001 degree*/
	g_intercept = g_intercept + (g_degc_cali*(1000/2)); /*1000 is for 0.1 degree*/
	pr_info("[Power/PMIC_Thermal] [Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
	       g_slope1, g_slope2, g_intercept, vbe_t);

}

static kal_int32 pmic_raw_to_temp(kal_uint32 ret)
{
	kal_int32 y_curr = ret;
	kal_int32 t_current;
	t_current = g_intercept + ((g_slope1 * y_curr) / (g_slope2));
	return t_current;
}

static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1, PMIC_counter;
static int mtktspmic_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;
	mutex_lock(&TSPMIC_lock);

	temp = PMIC_IMM_GetOneChannelValue(3, y_pmic_repeat_times , 2);
	temp1 = pmic_raw_to_temp(temp);

	pr_debug("[mtktspmic_get_hw_temp] PMIC_IMM_GetOneChannel 3=%d, T=%d\n", temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		pr_info("[Power/PMIC_Thermal] raw=%d, PMIC T=%d", temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		pr_info("[Power/PMIC_Thermal] drop this data\n");
		temp1 = pre_temp1;
	} else if ((PMIC_counter != 0) && (((pre_temp1-temp1) > 30000) || ((temp1-pre_temp1) > 30000))) {
		pr_info("[Power/PMIC_Thermal] drop this data 2\n");
		temp1 = pre_temp1;
	} else {
		pre_temp1 = temp1;
		pr_debug("[Power/PMIC_Thermal] pre_temp1=%d\n", pre_temp1);

		if (PMIC_counter == 0)
			PMIC_counter++;
	}

	mutex_unlock(&TSPMIC_lock);
	return temp1;
}

static int mtktspmic_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktspmic_get_hw_temp();
	return 0;
}

static int mtktspmic_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktspmic_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktspmic_get_trip_type(struct thermal_zone_device *thermal,
				   int trip,
				   enum thermal_trip_type *type)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktspmic_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long *t)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktspmic_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long t)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktspmic_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
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

static struct thermal_zone_device_ops mtktspmic_dev_ops = {
	.get_temp = mtktspmic_get_temp,
	.get_mode = mtktspmic_get_mode,
	.set_mode = mtktspmic_set_mode,
	.get_trip_type = mtktspmic_get_trip_type,
	.get_trip_temp = mtktspmic_get_trip_temp,
	.set_trip_temp = mtktspmic_set_trip_temp,
	.get_crit_temp = mtktspmic_get_crit_temp,
};

static void mtktspmic_work(struct work_struct *work)
{
	struct mtktspmic_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktspmic_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int mtktspmic_read_temp(struct thermal_dev *tdev)
{
	return mtktspmic_get_hw_temp();
}
static struct thermal_dev_ops mtktspmic_fops = {
	.get_temp = mtktspmic_read_temp,
};

#ifdef CONFIG_AUSTIN_PROJECT
struct thermal_dev_params mtktspmic_tdp = {
	.offset = 14500,
	.alpha = 4,
	.weight = 250
};
#else
struct thermal_dev_params mtktspmic_tdp = {
	.offset = 18000,
	.alpha = 4,
	.weight = 450
};
#endif

static int mtktspmic_show_params(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tzone->therm_fw->tdp->offset,
		       tzone->therm_fw->tdp->alpha,
		       tzone->therm_fw->tdp->weight);
}

static ssize_t mtktspmic_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;

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

static DEVICE_ATTR(params, S_IRUGO | S_IWUSR, mtktspmic_show_params, mtktspmic_store_params);

static int mtktspmic_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktspmic_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata)
		return -EINVAL;

	pmic_cali_prepare();
	pmic_cali_prepare2();

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktspmic",
							pdata->num_trips,
							1,
							tzone,
							&mtktspmic_dev_ops,
							NULL,
							0,
							pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktspmic thermal zone device\n", __func__);
		return -EINVAL;
	}
	tzone->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (!tzone->therm_fw)
		return -ENOMEM;
	tzone->therm_fw->name = "mtktspmic";
	tzone->therm_fw->dev = &(pdev->dev);
	tzone->therm_fw->dev_ops = &mtktspmic_fops;
	tzone->therm_fw->tdp = &mtktspmic_tdp;

	ret = thermal_dev_register(tzone->therm_fw);
	if (ret) {
		pr_err("Error registering therml mtktspmic device\n");
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtktspmic_work);
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktspmic_remove(struct platform_device *pdev)
{
	struct mtktspmic_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktspmic_driver = {
	.probe = mtktspmic_probe,
	.remove = mtktspmic_remove,
	.driver     = {
		.name  = "mtktspmic",
		.owner = THIS_MODULE,
	},
};

static struct mtk_thermal_platform_data mtktspmic_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {.temp = MTKTSPMIC_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static struct platform_device mtktspmic_device = {
	.name = "mtktspmic",
	.id = -1,
	.dev = {
		.platform_data = &mtktspmic_thermal_data,
	},
};

static int __init mtktspmic_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktspmic_driver);
	if (ret) {
		pr_err("Unable to register mtktspmic thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktspmic_device);
	if (ret) {
		pr_err("Unable to register mtktspmic device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktspmic_exit(void)
{
	platform_driver_unregister(&mtktspmic_driver);
	platform_device_unregister(&mtktspmic_device);
}

module_init(mtktspmic_init);
module_exit(mtktspmic_exit);
