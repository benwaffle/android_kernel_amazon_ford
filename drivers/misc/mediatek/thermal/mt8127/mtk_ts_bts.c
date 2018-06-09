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
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

static DEFINE_MUTEX(therm_lock);

struct mtkts_bts_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

#define MTKTS_BTS_TEMP_CRIT 100000 /* 100.000 degree Celsius */

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_IsAdcInitReady(void);
typedef struct{
    INT32 BTS_Temp;
    INT32 TemperatureR;
}BTS_TEMPERATURE;

extern struct proc_dir_entry * mtk_thermal_get_proc_drv_therm_dir_entry(void);

#define AUX_IN0_NTC (0) //NTC6301

static int g_RAP_pull_up_R = 39000;//39K,pull up resister
static int g_TAP_over_critical_low = 195652 ;//base on 100K NTC temp default value -40 deg
static int g_RAP_pull_up_voltage = 1800;//1.8V ,pull up voltage
static int g_RAP_ntc_table = 7;  //default is //NTCG104EF104F(100K)
static int g_RAP_ADC_channel = AUX_IN0_NTC;  //default is 0

static int g_AP_TemperatureR = 0;

static BTS_TEMPERATURE BTS_Temperature_Table[] = {
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}
};

/*AP_NTC_BL197 */
BTS_TEMPERATURE BTS_Temperature_Table1[] = {
	{-40,74354}, {-35,74354}, {-30,74354}, {-25,74354}, {-20,74354},
	{-15,57626}, {-10,45068}, { -5,35548}, {  0,28267}, {  5,22650},
	{ 10,18280}, { 15,14855}, { 20,12151}, { 25,10000},/*10K*/ { 30,8279},
	{ 35,6892},  { 40,5768},  { 45,4852},  { 50,4101},  { 55,3483},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970}
};

/*AP_NTC_TSM_1*/
BTS_TEMPERATURE BTS_Temperature_Table2[] = {
	{-40,70603}, {-35,70603}, {-30,70603}, {-25,70603}, {-20,70603},
	{-15,55183}, {-10,43499}, { -5,34569}, {  0,27680}, {  5,22316},
	{ 10,18104}, { 15,14773}, { 20,12122}, { 25,10000},/*10K*/ { 30,8294},
	{ 35,6915},  { 40,5795},  { 45,4882},  { 50,4133},  { 55,3516},
	{ 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},
	{ 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},  { 60,3004},
	{ 60,3004},  { 60,3004},  { 60,3004},  { 60,3004}
};

/*AP_NTC_10_SEN_1*/
BTS_TEMPERATURE BTS_Temperature_Table3[] = {
	{-40,74354}, {-35,74354}, {-30,74354}, {-25,74354}, {-20,74354},
	{-15,57626}, {-10,45068}, { -5,35548}, {  0,28267}, {  5,22650},
	{ 10,18280}, { 15,14855}, { 20,12151}, { 25,10000},/*10K*/ { 30,8279},
	{ 35,6892},  { 40,5768},  { 45,4852},  { 50,4101},  { 55,3483},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},  { 60,2970},
	{ 60,2970},  { 60,2970},  { 60,2970},  { 60,2970}
};

/*AP_NTC_10(TSM0A103F34D1RZ)*/
BTS_TEMPERATURE BTS_Temperature_Table4[] = {
	{-40,188500}, {-35,144290}, {-30,111330}, {-25,86560}, {-20,67790},
	{-15,53460},  {-10,42450},  { -5,33930},  {  0,27280}, {  5,22070},
	{ 10,17960},  { 15,14700},  { 20,12090},  { 25,10000},/*10K*/ { 30,8310},
	{ 35,6940},   { 40,5830},   { 45,4910},   { 50,4160},  { 55,3540},
	{ 60,3020},   { 65,2590},   { 70,2230},   { 75,1920},  { 80,1670},
	{ 85,1450},   { 90,1270},   { 95,1110},   { 100,975},  { 105,860},
	{ 110,760},   { 115,674},   { 120,599},   { 125,534}
};

/*AP_NTC_47*/
BTS_TEMPERATURE BTS_Temperature_Table5[] = {
	{-40,483954}, {-35,483954}, {-30,483954}, {-25,483954}, {-20,483954},
	{-15,360850}, {-10,271697}, { -5,206463}, {  0,158214}, {  5,122259},
	{ 10,95227},  { 15,74730},  { 20,59065},  { 25,47000},/*47K*/ { 30,37643},
	{ 35,30334},  { 40,24591},  { 45,20048},  { 50,16433},  { 55,13539},
	{ 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},
	{ 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},  { 60,11210},
	{ 60,11210},  { 60,11210},  { 60,11210},  { 60,11210}
};


/*NTCG104EF104F(100K)*/
BTS_TEMPERATURE BTS_Temperature_Table6[] = {
	{-40,4251000}, {-35,3005000}, {-30,2149000}, {-25,1554000}, {-20,1135000},
	{-15,837800},  {-10,624100},  { -5,469100},  {  0,355600},  {  5,271800},
	{ 10,209400},  { 15,162500},  { 20,127000},  { 25,100000},/*100K*/ { 30,79230},
	{ 35,63180},   { 40,50680},   { 45,40900},   { 50,33190},   { 55,27090},
	{ 60,22220},   { 65,18320},   { 70,15180},   { 75,12640},   { 80,10580},
	{ 85, 8887},   { 90, 7500},   { 95, 6357},   { 100,5410},   { 105,4623},
	{ 110,3965},   { 115,3415},   { 120,2951},   { 125,2560}
};

BTS_TEMPERATURE BTS_Temperature_Table7[] = {
	{-40,195652}, {-35,148171}, {-30,113347}, {-25,87558}, {-20,68236},
	{-15,53649},  {-10,42506},  { -5,33892},  {  0,27218}, {  5,22021},
	{ 10,17925},  { 15,14673},  { 20,12080},  { 25,10000},/*100K*/ { 30,8314},
	{ 35,6947},   { 40,5833},   { 45,4916},   { 50,4160},  { 55,3535},
	{ 60,3014},   { 65,2586},   { 70,2227},   { 75,1924},  { 80,1668},
	{ 85,1452},   { 90,1268},   { 95,1109},   { 100,973},  { 105,858},
	{ 110,758},   { 115,671},   { 120,596},   { 125,531}
};


/* =========== bts temp read ========== */

/* convert register to temperature  */
static INT16 mtkts_bts_thermistor_conver_temp(INT32 Res)
{
	int i=0;
	int asize=0;
	INT32 RES1=0,RES2=0;
	INT32 TAP_Value=-200,TMP1=0,TMP2=0;

	asize = (sizeof(BTS_Temperature_Table)/sizeof(BTS_TEMPERATURE));
	if(Res >= BTS_Temperature_Table[0].TemperatureR)
	{
		TAP_Value = -40; /* min */
	}
	else if(Res <= BTS_Temperature_Table[asize-1].TemperatureR)
	{
		TAP_Value = 125; /* max */
	}
	else
	{
		RES1 = BTS_Temperature_Table[0].TemperatureR;
		TMP1 = BTS_Temperature_Table[0].BTS_Temp;

		for(i=0; i < asize; i++)
		{
			if(Res >= BTS_Temperature_Table[i].TemperatureR)
			{
				RES2 = BTS_Temperature_Table[i].TemperatureR;
				TMP2 = BTS_Temperature_Table[i].BTS_Temp;
				break;
			}
			else
			{
				RES1 = BTS_Temperature_Table[i].TemperatureR;
				TMP1 = BTS_Temperature_Table[i].BTS_Temp;
			}
		}
		TAP_Value = (((Res-RES2)*TMP1)+((RES1-Res)*TMP2))/(RES1-RES2);
	}

	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/* Volt to Temp formula same with 6589  */
static INT16 mtk_ts_bts_volt_to_temp(UINT32 dwVolt)
{
    INT32 TRes;
    INT32 dwVCriAP = 0;
    INT32 BTS_TMP = -100;

    /* SW workaround-----------------------------------------------------
      dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) / (TAP_OVER_CRITICAL_LOW + 39000);
      dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) / (TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
    */
    dwVCriAP = (g_TAP_over_critical_low * g_RAP_pull_up_voltage) / (g_TAP_over_critical_low + g_RAP_pull_up_R);

    if(dwVolt > dwVCriAP)
    {
        TRes = g_TAP_over_critical_low;
    }
    else
    {
        TRes = (g_RAP_pull_up_R*dwVolt) / (g_RAP_pull_up_voltage-dwVolt);
    }

    g_AP_TemperatureR = TRes;

    /* convert register to temperature */
    BTS_TMP = mtkts_bts_thermistor_conver_temp(TRes);
    pr_debug("Thermal %s: TRes = %d, BTS_TMP=%d\n", __func__, TRes, BTS_TMP);

    return BTS_TMP;
}

static int get_hw_bts_temp(void)
{

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times=1, Channel=g_RAP_ADC_channel;
	static int valid_temp;

	if( IMM_IsAdcInitReady() == 0 )
	{
        	pr_err("[thermal_auxadc_get_data]: AUXADC is not ready\n");
		return 0;
	}

	i = times;
	while (i--)
	{
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		if (ret_value == -1) /* AUXADC is busy */
		{
			ret_temp = valid_temp;
		}
		else
		{
			valid_temp = ret_temp;
		}
		ret += ret_temp;
		pr_debug("[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n",ret_temp);
	}

	ret = ret*1500/4096; /* 82's ADC power */
	pr_debug("APtery output mV = %d\n",ret);
	output = mtk_ts_bts_volt_to_temp(ret);
	pr_debug("BTS output temperature = %d\n",output);

	return output;
}

static DEFINE_MUTEX(BTS_lock);
int mtkts_bts_get_hw_temp(void)
{
	int t_ret=0;

	mutex_lock(&BTS_lock);

	/* get HW AP temp (TSAP)
	   cat /sys/class/power_supply/AP/AP_temp
	*/
	t_ret = get_hw_bts_temp();
	t_ret = t_ret * 1000;

	mutex_unlock(&BTS_lock);

	if (t_ret > 60000) /* abnormal high temp */
		pr_info("[Power/BTS_Thermal] T_AP=%d\n", t_ret);

	pr_debug("[mtkts_bts_get_hw_temp] T_AP, %d\n", t_ret);
	return t_ret;
}

/* =========== bts thermal zone callbacks ========== */

static int mtkts_bts_get_temp(struct thermal_zone_device *thermal,
			                 unsigned long *t)
{
	*t = mtkts_bts_get_hw_temp();
	return 0;
}


static int mtkts_bts_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtkts_bts_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtkts_bts_get_trip_type(struct thermal_zone_device *thermal,
				   int trip,
				   enum thermal_trip_type *type)
{
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtkts_bts_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long *t)
{
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtkts_bts_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long t)
{
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtkts_bts_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;
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

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_bts_dev_ops = {
	.get_temp = mtkts_bts_get_temp,
	.get_mode = mtkts_bts_get_mode,
	.set_mode = mtkts_bts_set_mode,
	.get_trip_type = mtkts_bts_get_trip_type,
	.get_trip_temp = mtkts_bts_get_trip_temp,
	.set_trip_temp = mtkts_bts_set_trip_temp,
	.get_crit_temp = mtkts_bts_get_crit_temp,
};

/* =========== bts thermal param handling ========== */

#ifdef CONFIG_AUSTIN_PROJECT
struct thermal_dev_params mtkts_bts_tdp = {
	.offset = -2500,
	.alpha = 15,
	.weight = 250
};
#else
struct thermal_dev_params mtkts_bts_tdp = {
	.offset = -1000,
	.alpha = 15,
	.weight = 113
};
#endif

static int mtkts_bts_show_params(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tzone->therm_fw->tdp->offset,
		       tzone->therm_fw->tdp->alpha,
		       tzone->therm_fw->tdp->weight);
}

static ssize_t mtkts_bts_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtkts_bts_thermal_zone *tzone = thermal->devdata;

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

static DEVICE_ATTR(params, S_IRUGO | S_IWUSR, mtkts_bts_show_params, mtkts_bts_store_params);

/* ========= bts table/tzbts_param handling =========== */

void mtkts_bts_copy_table(BTS_TEMPERATURE *des,BTS_TEMPERATURE *src)
{
	int i=0;
	int j=0;

	j = (sizeof(BTS_Temperature_Table)/sizeof(BTS_TEMPERATURE));
	for(i=0; i<j; i++)
	{
		des[i] = src[i];
	}
}

void mtkts_bts_prepare_table(int table_num)
{
	pr_info("Thermal %s with %d\n", __func__, table_num);

	switch(table_num)
	{
		case 1://AP_NTC_BL197
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table1);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table1));
			break;
		case 2://AP_NTC_TSM_1
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table2);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table2));
			break;
		case 3://AP_NTC_10_SEN_1
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table3);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table3));
			break;
		case 4://AP_NTC_10
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table4);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table4));
			break;
		case 5://AP_NTC_47
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table5);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table5));
			break;
		case 6://NTCG104EF104F
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table6);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table6));
			break;
                case 7://NCP15XH103F03RC
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table7);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table7));
                        break;

		default://AP_NTC_10
			mtkts_bts_copy_table(BTS_Temperature_Table,BTS_Temperature_Table4);
			BUG_ON(sizeof(BTS_Temperature_Table)!=sizeof(BTS_Temperature_Table4));
			break;
	}
}

static int mtkts_bts_param_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n",g_RAP_pull_up_R);
	seq_printf(m, "%d\n",g_RAP_pull_up_voltage);
	seq_printf(m, "%d\n",g_TAP_over_critical_low);
	seq_printf(m, "%d\n",g_RAP_ntc_table);
	seq_printf(m, "%d\n",g_RAP_ADC_channel);

	return 0;
}


static ssize_t mtkts_bts_param_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len=0;
	char desc[512];
	char pull_R[10],pull_V[10];
	char overcrilow[16];
	char NTC_TABLE[10];
	unsigned int valR,valV,over_cri_low,ntc_table;
	unsigned int adc_channel;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
	{
		return 0;
	}
	desc[len] = '\0';


	if (sscanf(desc, "%10s %d %10s %d %16s %d %10s %d %d", \
		pull_R, &valR, pull_V, &valV, overcrilow, \
		&over_cri_low, NTC_TABLE, &ntc_table, &adc_channel) < 8)
	{
		printk("[mtkts_bts_write] bad argument\n");
		return -EINVAL;
	}


	if (!strcmp(pull_R, "PUP_R"))
		g_RAP_pull_up_R = valR;
	else{
		pr_err("%s bad PUP_R argument\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(pull_V, "PUP_VOLT"))
		g_RAP_pull_up_voltage = valV;
	else{
		pr_err("%s bad PUP_VOLT argument\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(overcrilow, "OVER_CRITICAL_L"))
		g_TAP_over_critical_low = over_cri_low;
	else{
		pr_err("%s bad OVERCRIT_L argument\n", __func__);
		return -EINVAL;
	}

        if (!strcmp(NTC_TABLE, "NTC_TABLE"))
            g_RAP_ntc_table = ntc_table;
        else{
		pr_err("%s bad NTC_TABLE argument\n", __func__);
		return -EINVAL;
        }

	/* external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,
	   choose "adc_channel=11" to check if there is any param input
	*/
        if((adc_channel >= 2) && (adc_channel <= 11)){
		/* check unsupport pin value, 
		   if unsupport, set channel = 1 as default setting.
		*/
		g_RAP_ADC_channel = AUX_IN0_NTC;
	}else{
		if(adc_channel!=11){
			/* check if there is any param input, 
			   if not using default g_RAP_ADC_channel:1
			*/
			g_RAP_ADC_channel = adc_channel;
		}
		else{
			g_RAP_ADC_channel = AUX_IN0_NTC;
		}
	}

	mtkts_bts_prepare_table(g_RAP_ntc_table);

	return count;
}

static int mtkts_bts_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtkts_bts_param_read, NULL);
}

static const struct file_operations mtkts_AP_param_fops = {
    .owner = THIS_MODULE,
    .open = mtkts_bts_param_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mtkts_bts_param_write,
    .release = single_release,
};

/* ========= bts device/driver handling =========== */

static void mtkts_bts_work(struct work_struct *work)
{
	struct mtkts_bts_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtkts_bts_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int mtkts_bts_read_temp(struct thermal_dev *tdev)
{
	return mtkts_bts_get_hw_temp();
}
static struct thermal_dev_ops mtkts_bts_fops = {
	.get_temp = mtkts_bts_read_temp,
};

static int mtkts_bts_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_AP_dir = NULL;
	struct mtkts_bts_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata) {
		pr_err("%s: Error getting platform data\n", __func__);
		return -EINVAL;
	}
	pr_info("%s thermal bts\n", __func__);

	/* setup default table */
	mtkts_bts_prepare_table(g_RAP_ntc_table);

	mtkts_AP_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_AP_dir)
		pr_err("%s: mkdir /proc/driver/thermal failed\n", __func__);
	else {
		entry = proc_create("tzbts_param", S_IRUGO | S_IWUSR | S_IWGRP, 
				    mtkts_AP_dir, &mtkts_AP_param_fops);
		if (entry)
			proc_set_user(entry, 0, 1000);
	}

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtkts_bts",
						pdata->num_trips,
						(1 << pdata->num_trips) - 1,
						tzone,
						&mtkts_bts_dev_ops,
						NULL,
						0,
						pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register thermal zone device\n", __func__);
		return -EINVAL;
	}
	tzone->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (!tzone->therm_fw)
		return -ENOMEM;
	tzone->therm_fw->name = "mtkts_bts";
	tzone->therm_fw->dev = &(pdev->dev);
	tzone->therm_fw->dev_ops = &mtkts_bts_fops;
	tzone->therm_fw->tdp = &mtkts_bts_tdp;

	ret = thermal_dev_register(tzone->therm_fw);
	if (ret) {
		pr_err("%s: Error registering thermal device\n", __func__);
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtkts_bts_work);
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtkts_bts_remove(struct platform_device *pdev)
{
	struct mtkts_bts_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtkts_bts_driver = {
	.probe = mtkts_bts_probe,
	.remove = mtkts_bts_remove,
	.driver     = {
		.name  = "mtkts_bts",
		.owner = THIS_MODULE,
	},
};

static struct mtk_thermal_platform_data mtkts_bts_thermal_data = {
	.num_trips = 3,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	/* original trip temps:
	   {120000,110000,100000,90000,80000,70000,65000,60000,55000,50000};
	   Only use the ones below critical temp
	*/
	.trips[0] = {.temp = 85000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[1] = {.temp = 90000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0},
	.trips[2] = {.temp = MTKTS_BTS_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static struct platform_device mtkts_bts_device = {
	.name = "mtkts_bts",
	.id = -1,
	.dev = {
		.platform_data = &mtkts_bts_thermal_data,
	},
};

static int __init mtkts_bts_init(void)
{
	int ret;
	ret = platform_driver_register(&mtkts_bts_driver);
	if (ret) {
		pr_err("Unable to register mtkts_bts thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtkts_bts_device);
	if (ret) {
		pr_err("Unable to register mtkts_bts device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtkts_bts_exit(void)
{
	platform_driver_unregister(&mtkts_bts_driver);
	platform_device_unregister(&mtkts_bts_device);
}

module_init(mtkts_bts_init);
module_exit(mtkts_bts_exit);

