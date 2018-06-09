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
#include <linux/spinlock.h>
#include "mach/dma.h"
#include <mach/sync_write.h>
#include <mach/mt_irq.h>
#include "mach/mtk_thermal_monitor.h"
#include <mach/system.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include "mach/mt_gpufreq.h"
#include <mach/mt_clkmgr.h>

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

/* 1: turn on supports to MET logging; 0: turn off */
#define CONFIG_SUPPORT_MET_MTKTSCPU (1)

/* Thermal controller HW filtering function.
   Only 1, 2, 4, 8, 16 are valid values,
   they means one reading is a avg of X samples
*/
#define THERMAL_CONTROLLER_HW_FILTER (1)

/* 1: turn on thermal controller HW thermal protection; 0: turn off */
#define THERMAL_CONTROLLER_HW_TP     (1)

#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))

int MA_len_temp = 0;
int mtktscpu_limited_dmips = 0;

static DEFINE_MUTEX(TS_lock);

#define NEW_6582_NON_DVFS_GPU

#define MTKTSCPU_TEMP_CRIT 120000 /* 120.000 degree Celsius */
static DEFINE_MUTEX(therm_lock);

struct mtktscpu_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

static int mtktscpu_debug_log;

#define mtktscpu_dprintk(fmt, args...)  \
do {                                    \
	if (mtktscpu_debug_log) {                \
		xlog_printk(ANDROID_LOG_INFO, "Power/CPU_Thermal", fmt, ##args); \
	}                                   \
} while (0)

static kal_int32 temperature_to_raw_abb(kal_uint32 ret);
int last_abb_t = 0;
int last_CPU1_t = 0;
int last_CPU2_t = 0;

static kal_int32 g_adc_ge_t;
static kal_int32 g_adc_oe_t;
static kal_int32 g_o_vtsmcu1;
static kal_int32 g_o_vtsmcu2;
static kal_int32 g_o_vtsabb;
static kal_int32 g_degc_cali;
static kal_int32 g_adc_cali_en_t;
static kal_int32 g_o_slope;
static kal_int32 g_o_slope_sign;
static kal_int32 g_id;

static kal_int32 g_ge;
static kal_int32 g_oe;
static kal_int32 g_gain;

static kal_int32 g_x_roomt1;
static kal_int32 g_x_roomt2;
static kal_int32 g_x_roomtabb;

#define y_curr_repeat_times 1
#define THERMAL_NAME    "mtk-thermal"

#if CONFIG_SUPPORT_MET_MTKTSCPU
/* MET */
#include <linux/export.h>
#include <linux/met_drv.h>

static char header[] =
	"met-info [000] 0.0: ms_ud_sys_header: CPU_Temp,"
	"TS1_temp,TS2_temp,d,d\n"
	"met-info [000] 0.0: ms_ud_sys_header: P_static,"
	"CPU_power,GPU_power,d,d\n";
static const char help[] = "  --mtktscpu                              monitor mtktscpu\n";
static int sample_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int sample_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, header);
}

unsigned int met_mtktscpu_dbg = 0;
static void sample_start(void)
{
	met_mtktscpu_dbg = 1;
	return;
}

static void sample_stop(void)
{
	met_mtktscpu_dbg = 0;
	return;
}

struct metdevice met_mtktscpu = {
	.name = "mtktscpu",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.start = sample_start,
	.stop = sample_stop,
	.print_help = sample_print_help,
	.print_header = sample_print_header,
};
EXPORT_SYMBOL(met_mtktscpu);
#endif

static bool talking_flag;
void set_taklking_flag(bool flag)
{
	talking_flag = flag;
	mtktscpu_dprintk("Power/CPU_Thermal: talking_flag=%d", talking_flag);
	return;
}

int mtktscpu_thermal_clock_on(void)
{
	enable_clock(MT_CG_PERI_THERM, "THERMAL");
	return 0;
}

int mtktscpu_thermal_clock_off(void)
{
	disable_clock(MT_CG_PERI_THERM, "THERMAL");
	return 0;
}

void get_thermal_all_register(void)
{
	mtktscpu_dprintk("get_thermal_all_register\n");

	mtktscpu_dprintk("TEMPMSR1                = 0x%8x\n", DRV_Reg32(TEMPMSR1));
	mtktscpu_dprintk("TEMPMSR2            = 0x%8x\n", DRV_Reg32(TEMPMSR2));

	mtktscpu_dprintk("TEMPMONCTL0             = 0x%8x\n", DRV_Reg32(TEMPMONCTL0));
	mtktscpu_dprintk("TEMPMONCTL1             = 0x%8x\n", DRV_Reg32(TEMPMONCTL1));
	mtktscpu_dprintk("TEMPMONCTL2             = 0x%8x\n", DRV_Reg32(TEMPMONCTL2));
	mtktscpu_dprintk("TEMPMONINT              = 0x%8x\n", DRV_Reg32(TEMPMONINT));
	mtktscpu_dprintk("TEMPMONINTSTS           = 0x%8x\n", DRV_Reg32(TEMPMONINTSTS));
	mtktscpu_dprintk("TEMPMONIDET0            = 0x%8x\n", DRV_Reg32(TEMPMONIDET0));

	mtktscpu_dprintk("TEMPMONIDET1            = 0x%8x\n", DRV_Reg32(TEMPMONIDET1));
	mtktscpu_dprintk("TEMPMONIDET2            = 0x%8x\n", DRV_Reg32(TEMPMONIDET2));
	mtktscpu_dprintk("TEMPH2NTHRE             = 0x%8x\n", DRV_Reg32(TEMPH2NTHRE));
	mtktscpu_dprintk("TEMPHTHRE               = 0x%8x\n", DRV_Reg32(TEMPHTHRE));
	mtktscpu_dprintk("TEMPCTHRE               = 0x%8x\n", DRV_Reg32(TEMPCTHRE));
	mtktscpu_dprintk("TEMPOFFSETH             = 0x%8x\n", DRV_Reg32(TEMPOFFSETH));

	mtktscpu_dprintk("TEMPOFFSETL             = 0x%8x\n", DRV_Reg32(TEMPOFFSETL));
	mtktscpu_dprintk("TEMPMSRCTL0             = 0x%8x\n", DRV_Reg32(TEMPMSRCTL0));
	mtktscpu_dprintk("TEMPMSRCTL1             = 0x%8x\n", DRV_Reg32(TEMPMSRCTL1));
	mtktscpu_dprintk("TEMPAHBPOLL             = 0x%8x\n", DRV_Reg32(TEMPAHBPOLL));
	mtktscpu_dprintk("TEMPAHBTO               = 0x%8x\n", DRV_Reg32(TEMPAHBTO));
	mtktscpu_dprintk("TEMPADCPNP0             = 0x%8x\n", DRV_Reg32(TEMPADCPNP0));

	mtktscpu_dprintk("TEMPADCPNP1             = 0x%8x\n", DRV_Reg32(TEMPADCPNP1));
	mtktscpu_dprintk("TEMPADCPNP2             = 0x%8x\n", DRV_Reg32(TEMPADCPNP2));
	mtktscpu_dprintk("TEMPADCMUX              = 0x%8x\n", DRV_Reg32(TEMPADCMUX));
	mtktscpu_dprintk("TEMPADCEXT              = 0x%8x\n", DRV_Reg32(TEMPADCEXT));
	mtktscpu_dprintk("TEMPADCEXT1             = 0x%8x\n", DRV_Reg32(TEMPADCEXT1));
	mtktscpu_dprintk("TEMPADCEN               = 0x%8x\n", DRV_Reg32(TEMPADCEN));


	mtktscpu_dprintk("TEMPPNPMUXADDR      = 0x%8x\n", DRV_Reg32(TEMPPNPMUXADDR));
	mtktscpu_dprintk("TEMPADCMUXADDR      = 0x%8x\n", DRV_Reg32(TEMPADCMUXADDR));
	mtktscpu_dprintk("TEMPADCEXTADDR      = 0x%8x\n", DRV_Reg32(TEMPADCEXTADDR));
	mtktscpu_dprintk("TEMPADCEXT1ADDR     = 0x%8x\n", DRV_Reg32(TEMPADCEXT1ADDR));
	mtktscpu_dprintk("TEMPADCENADDR       = 0x%8x\n", DRV_Reg32(TEMPADCENADDR));
	mtktscpu_dprintk("TEMPADCVALIDADDR    = 0x%8x\n", DRV_Reg32(TEMPADCVALIDADDR));

	mtktscpu_dprintk("TEMPADCVOLTADDR     = 0x%8x\n", DRV_Reg32(TEMPADCVOLTADDR));
	mtktscpu_dprintk("TEMPRDCTRL          = 0x%8x\n", DRV_Reg32(TEMPRDCTRL));
	mtktscpu_dprintk("TEMPADCVALIDMASK    = 0x%8x\n", DRV_Reg32(TEMPADCVALIDMASK));
	mtktscpu_dprintk("TEMPADCVOLTAGESHIFT = 0x%8x\n", DRV_Reg32(TEMPADCVOLTAGESHIFT));
	mtktscpu_dprintk("TEMPADCWRITECTRL    = 0x%8x\n", DRV_Reg32(TEMPADCWRITECTRL));
	mtktscpu_dprintk("TEMPMSR0            = 0x%8x\n", DRV_Reg32(TEMPMSR0));


	mtktscpu_dprintk("TEMPIMMD0           = 0x%8x\n", DRV_Reg32(TEMPIMMD0));
	mtktscpu_dprintk("TEMPIMMD1           = 0x%8x\n", DRV_Reg32(TEMPIMMD1));
	mtktscpu_dprintk("TEMPIMMD2           = 0x%8x\n", DRV_Reg32(TEMPIMMD2));
	mtktscpu_dprintk("TEMPPROTCTL         = 0x%8x\n", DRV_Reg32(TEMPPROTCTL));

	mtktscpu_dprintk("TEMPPROTTA          = 0x%8x\n", DRV_Reg32(TEMPPROTTA));
	mtktscpu_dprintk("TEMPPROTTB              = 0x%8x\n", DRV_Reg32(TEMPPROTTB));
	mtktscpu_dprintk("TEMPPROTTC              = 0x%8x\n", DRV_Reg32(TEMPPROTTC));
	mtktscpu_dprintk("TEMPSPARE0              = 0x%8x\n", DRV_Reg32(TEMPSPARE0));
	mtktscpu_dprintk("TEMPSPARE1              = 0x%8x\n", DRV_Reg32(TEMPSPARE1));
	mtktscpu_dprintk("TEMPSPARE2              = 0x%8x\n", DRV_Reg32(TEMPSPARE2));
	mtktscpu_dprintk("TEMPSPARE3              = 0x%8x\n", DRV_Reg32(TEMPSPARE3));
	mtktscpu_dprintk("0x11001040              = 0x%8x\n", DRV_Reg32(0xF1001040));
}

void get_thermal_slope_intercept(struct TS_PTPOD *ts_info)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	temp0 = (10000*100000/g_gain)*15/18;
	if (g_o_slope_sign == 0)
		temp1 = temp0/(165+g_o_slope);
	else
		temp1 = temp0/(165-g_o_slope);
	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali*10 / 2);
	temp1 = ((10000*100000/4096/g_gain)*g_oe + g_x_roomtabb*10)*15/18;
	if (g_o_slope_sign == 0)
		temp2 = temp1*10/(165+g_o_slope);
	else
		temp2 = temp1*10/(165-g_o_slope);
	ts_ptpod.ts_BTS = (temp0+temp2-250)*4/10;
	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	mtktscpu_dprintk("ts_MTS=%d, ts_BTS=%d\n", ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);

	return;
}
EXPORT_SYMBOL(get_thermal_slope_intercept);

static irqreturn_t thermal_interrupt_handler(int irq, void *dev_id)
{
	kal_uint32 ret = 0;
	ret = DRV_Reg32(TEMPMONINTSTS);

	mtktscpu_dprintk("thermal_isr: [Interrupt trigger]: status = 0x%x\n", ret);
	if (ret & THERMAL_MON_CINTSTS0)
		mtktscpu_dprintk("thermal_isr: thermal sensor point 0 - cold interrupt trigger\n");
	if (ret & THERMAL_MON_HINTSTS0)
		mtktscpu_dprintk("thermal_isr: thermal sensor point 0 - hot interrupt trigger\n");
	if (ret & THERMAL_tri_SPM_State0)
		mtktscpu_dprintk("thermal_isr: Thermal state0 to trigger SPM state0\n");
	if (ret & THERMAL_tri_SPM_State1)
		mtktscpu_dprintk("thermal_isr: Thermal state1 to trigger SPM state1\n");
	if (ret & THERMAL_tri_SPM_State2)
		mtktscpu_dprintk("thermal_isr: Thermal state2 to trigger SPM state2\n");

	return IRQ_HANDLED;
}

int thermal_fast_init(void)
{
	UINT32 temp = 0;
	UINT32 cunt = 0;
	mtktscpu_dprintk("thermal_fast_init\n");


	temp = 0xDA1;
	DRV_WriteReg32(TEMPSPARE2, (0x00001000 + temp));
	DRV_WriteReg32(TEMPMONCTL1, 1);
	DRV_WriteReg32(TEMPMONCTL2, 1);
	DRV_WriteReg32(TEMPAHBPOLL, 1);

	DRV_WriteReg32(TEMPAHBTO,    0x000000FF);
	DRV_WriteReg32(TEMPMONIDET0, 0x00000000);
	DRV_WriteReg32(TEMPMONIDET1, 0x00000000);

	DRV_WriteReg32(TEMPMSRCTL0, 0x0000000);

	DRV_WriteReg32(TEMPADCPNP0, 0x1);
	DRV_WriteReg32(TEMPADCPNP1, 0x2);
	DRV_WriteReg32(TEMPADCPNP2, 0x3);


	DRV_WriteReg32(TEMPPNPMUXADDR, (UINT32) (TEMPSPARE0-0xE0000000));
	DRV_WriteReg32(TEMPADCMUXADDR, (UINT32) (TEMPSPARE0-0xE0000000));
	DRV_WriteReg32(TEMPADCENADDR,  (UINT32) (TEMPSPARE1-0xE0000000));
	DRV_WriteReg32(TEMPADCVALIDADDR, (UINT32) (TEMPSPARE2-0xE0000000));
	DRV_WriteReg32(TEMPADCVOLTADDR, (UINT32) (TEMPSPARE2-0xE0000000));

	DRV_WriteReg32(TEMPRDCTRL, 0x0);
	DRV_WriteReg32(TEMPADCVALIDMASK, 0x0000002C);
	DRV_WriteReg32(TEMPADCVOLTAGESHIFT, 0x0);
	DRV_WriteReg32(TEMPADCWRITECTRL, 0x3);

	DRV_WriteReg32(TEMPMONINT, 0x00000000);

	DRV_WriteReg32(TEMPMONCTL0, 0x0000000F);

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR0)&0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		mtktscpu_dprintk("[Power/CPU_Thermal]0 temp=%d,cunt=%d\n", temp, cunt);
		temp = DRV_Reg32(TEMPMSR0)&0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR1)&0x0fff;
	while (temp != 0xDA1 &&  cunt < 20) {
		cunt++;
		mtktscpu_dprintk("[Power/CPU_Thermal]1 temp=%d,cunt=%d\n", temp, cunt);
		temp = DRV_Reg32(TEMPMSR1)&0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR2)&0x0fff;
	while (temp != 0xDA1 &&  cunt < 20) {
		cunt++;
		mtktscpu_dprintk("[Power/CPU_Thermal]2 temp=%d,cunt=%d\n", temp, cunt);
		temp = DRV_Reg32(TEMPMSR2)&0x0fff;
	}

	return 0;
}

static void thermal_reset_and_initial(void)
{
	UINT32 temp = 0;
	int cnt = 0, temp2 = 0;

	mtktscpu_dprintk("[Reset and init thermal controller]\n");

	/*
	  fix ALPS00848017
	  can't turn off thermal, this will cause PTPOD  issue abnormal interrupt
	  and let system crash.(because PTPOD can't get thermal's temperature)
	*/

	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp |= 0x00010000;
	THERMAL_WRAP_WR32(temp, PERI_GLOBALCON_RST0);

	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp &= 0xFFFEFFFF;
	THERMAL_WRAP_WR32(temp, PERI_GLOBALCON_RST0);

	thermal_fast_init();
	while (cnt < 30) {
		temp2 = DRV_Reg32(TEMPMSRCTL1);
		mtktscpu_dprintk("TEMPMSRCTL1 = 0x%x\n", temp2);
		/*
		  TEMPMSRCTL1[7]:Temperature measurement bus status[1]
		  TEMPMSRCTL1[0]:Temperature measurement bus status[0]

		  00: IDLE                           <=can pause    ,TEMPMSRCTL1[7][0]=0x00
		  01: Write transaction              <=can not pause,TEMPMSRCTL1[7][0]=0x01
		  10: Read transaction               <=can not pause,TEMPMSRCTL1[7][0]=0x10
		  11: Waiting for read after Write   <=can pause    ,TEMPMSRCTL1[7][0]=0x11
		*/
		if (((temp2 & 0x81) == 0x00) || ((temp2 & 0x81) == 0x81)) {
			/*
			  Pause periodic temperature measurement for
			  sensing point 0,sensing point 1,sensing point 2
			*/

			DRV_WriteReg32(TEMPMSRCTL1, (temp2 | 0x10E));
			break;
		}
		mtktscpu_dprintk("temp=0x%x, cnt=%d\n", temp2, cnt);
		udelay(10);
		cnt++;
	}
	THERMAL_WRAP_WR32(0x00000000, TEMPMONCTL0);

	temp = DRV_Reg32(AUXADC_CON0_V);
	temp &= 0xFFFFF7FF;
	THERMAL_WRAP_WR32(temp, AUXADC_CON0_V);

	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_CLR_V);


	THERMAL_WRAP_WR32(0x000003FF, TEMPMONCTL1);

#if THERMAL_CONTROLLER_HW_FILTER == 2
	THERMAL_WRAP_WR32(0x07E007E0, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x001F7972, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x00000049, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	THERMAL_WRAP_WR32(0x050A050A, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x001424C4, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x000000DB, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	THERMAL_WRAP_WR32(0x03390339, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x000C96FA, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x00000124, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	THERMAL_WRAP_WR32(0x01C001C0, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x0006FE8B, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x0000016D, TEMPMSRCTL0);
#else
	THERMAL_WRAP_WR32(0x03FF0000, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x00FFFFFF, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x00000000, TEMPMSRCTL0);
#endif

	THERMAL_WRAP_WR32(0xFFFFFFFF, TEMPAHBTO);

	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET0);
	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET1);

	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_SET_V);

	THERMAL_WRAP_WR32(0x800, TEMPADCMUX);
	THERMAL_WRAP_WR32((UINT32) AUXADC_CON1_CLR_P, TEMPADCMUXADDR);

	THERMAL_WRAP_WR32(0x800, TEMPADCEN);
	THERMAL_WRAP_WR32((UINT32) AUXADC_CON1_SET_P, TEMPADCENADDR);

	THERMAL_WRAP_WR32((UINT32) AUXADC_DAT11_P, TEMPADCVALIDADDR);
	THERMAL_WRAP_WR32((UINT32) AUXADC_DAT11_P, TEMPADCVOLTADDR);
	THERMAL_WRAP_WR32(0x0, TEMPRDCTRL);
	THERMAL_WRAP_WR32(0x0000002C, TEMPADCVALIDMASK);
	THERMAL_WRAP_WR32(0x0, TEMPADCVOLTAGESHIFT);
	THERMAL_WRAP_WR32(0x2, TEMPADCWRITECTRL);

	temp = DRV_Reg32(TS_CON0);
	temp &= ~(0x000000C0);
	THERMAL_WRAP_WR32(temp, TS_CON0);

	/* Add delay time before sensor polling. */
	udelay(200);

	THERMAL_WRAP_WR32(0x0, TEMPADCPNP0);
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP1);
	THERMAL_WRAP_WR32((UINT32) TS_CON1_P, TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);

	temp = DRV_Reg32(TEMPMSRCTL1);
	DRV_WriteReg32(TEMPMSRCTL1, ((temp & (~0x10E))));

	THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);

}

static void set_thermal_ctrl_trigger_SPM(int temperature)
{
#if THERMAL_CONTROLLER_HW_TP
	int temp = 0;
	int raw_high, raw_middle, raw_low;

	pr_crit("[Set_thermal_ctrl_trigger_SPM]: temperature=%d\n", temperature);

	raw_high   = temperature_to_raw_abb(temperature);
	raw_middle = temperature_to_raw_abb(20000);
	raw_low    = temperature_to_raw_abb(5000);

	temp = DRV_Reg32(TEMPMONINT);

	THERMAL_WRAP_WR32(temp & 0x1FFFFFFF, TEMPMONINT);

	THERMAL_WRAP_WR32(0x20000, TEMPPROTCTL);
	THERMAL_WRAP_WR32(raw_low, TEMPPROTTA);
	THERMAL_WRAP_WR32(raw_middle, TEMPPROTTB);
	THERMAL_WRAP_WR32(raw_high, TEMPPROTTC);

	/*trigger cold ,normal and hot interrupt*/
	/*Only trigger hot interrupt*/
	THERMAL_WRAP_WR32(temp | 0x80000000, TEMPMONINT);
#endif
}

static void thermal_cal_prepare(void)
{
	kal_uint32 temp0, temp1 = 0;

	temp0 = get_devinfo_with_index(7);
	temp1 = get_devinfo_with_index(8);

	pr_crit("[Power/CPU_Thermal] [Thermal calibration] temp0=0x%x, temp1=0x%x\n", temp0, temp1);

	g_adc_ge_t     = ((temp1 & 0xFFC00000)>>22);
	g_adc_oe_t     = ((temp1 & 0x003FF000)>>12);


	g_o_vtsmcu1    = (temp0 & 0x03FE0000)>>17;
	g_o_vtsmcu2    = (temp0 & 0x0001FF00)>>8;

	g_o_vtsabb     = (temp1 & 0x000001FF);

	g_degc_cali    = (temp0 & 0x0000007E)>>1;
	g_adc_cali_en_t = (temp0 & 0x00000001);
	g_o_slope      = (temp0 & 0xFC000000)>>26;
	g_o_slope_sign = (temp0 & 0x00000080)>>7;

	g_id           = (temp1 & 0x00000200)>>9;

	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en_t == 1)
		pr_info("thermal_enable = true\n");
	else {
		pr_crit("This sample is not Thermal calibrated\n");
		g_adc_ge_t = 512;
		g_adc_oe_t = 512;
		g_degc_cali = 40;
		g_o_slope = 0;
		g_o_slope_sign = 0;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		g_o_vtsabb = 260;
	}

	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_adc_ge_t      = 0x%x\n", g_adc_ge_t);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_adc_oe_t      = 0x%x\n", g_adc_oe_t);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_degc_cali     = 0x%x\n", g_degc_cali);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_adc_cali_en_t = 0x%x\n", g_adc_cali_en_t);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_o_slope       = 0x%x\n", g_o_slope);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_o_slope_sign  = 0x%x\n", g_o_slope_sign);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_id            = 0x%x\n", g_id);


	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsmcu1     = 0x%x\n", g_o_vtsmcu1);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsmcu2     = 0x%x\n", g_o_vtsmcu2);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsabb      = 0x%x\n", g_o_vtsabb);
}

static void thermal_cal_prepare_2(kal_uint32 ret)
{
	kal_int32 format_1, format_2, format_abb = 0;

	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;
	g_oe =  (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format_1   = (g_o_vtsmcu1 + 3350 - g_oe);
	format_2   = (g_o_vtsmcu2 + 3350 - g_oe);
	format_abb = (g_o_vtsabb  + 3350 - g_oe);

	g_x_roomt1   = (((format_1   * 10000) / 4096) * 10000) / g_gain;
	g_x_roomt2   = (((format_2   * 10000) / 4096) * 10000) / g_gain;
	g_x_roomtabb = (((format_abb * 10000) / 4096) * 10000) / g_gain;

	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_ge       = 0x%x\n", g_ge);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_gain     = 0x%x\n", g_gain);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_gain     = 0x%x\n", g_gain);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_x_roomt1 = 0x%x\n", g_x_roomt1);
	pr_crit("[Power/CPU_Thermal] [Thermal calibration] g_x_roomt2 = 0x%x\n", g_x_roomt2);
}

static kal_int32 temperature_to_raw_abb(kal_uint32 ret)
{
	kal_int32 t_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	if (g_o_slope_sign == 0) {
		format_1 = t_curr-(g_degc_cali*1000/2);
		format_2 = format_1 * (165+g_o_slope) * 18/15;
		format_2 = format_2 - 2*format_2;
		format_3 = format_2/1000 + g_x_roomt1*10;
		format_4 = (format_3*4096/10000*g_gain)/100000 + g_oe;
	} else {
		format_1 = t_curr-(g_degc_cali*1000/2);
		format_2 = format_1 * (165-g_o_slope) * 18/15;
		format_2 = format_2 - 2*format_2;
		format_3 = format_2/1000 + g_x_roomt1*10;
		format_4 = (format_3*4096/10000*g_gain)/100000 + g_oe;
	}
	return format_4;
}

static kal_int32 raw_to_temperature_MCU1(kal_uint32 ret)
{
	kal_int32 t_current = 0;
	kal_int32 y_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	if (ret == 0)
		return 0;

	format_1 = (g_degc_cali*10 / 2);
	format_2 = (y_curr - g_oe);
	format_3 = (((((format_2) * 10000) / 4096) * 10000) / g_gain) - g_x_roomt1;
	format_3 = format_3 * 15/18;


	if (g_o_slope_sign == 0)
		format_4 = ((format_3 * 100) / (165+g_o_slope));
	else
		format_4 = ((format_3 * 100) / (165-g_o_slope));

	format_4 = format_4 - (2 * format_4);
	t_current = format_1 + format_4;

	return t_current;
}

static kal_int32 raw_to_temperature_MCU2(kal_uint32 ret)
{
	kal_int32 t_current = 0;
	kal_int32 y_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	if (ret == 0)
		return 0;


	format_1 = (g_degc_cali*10 / 2);
	format_2 = (y_curr - g_oe);
	format_3 = (((((format_2) * 10000) / 4096) * 10000) / g_gain) - g_x_roomt2;
	format_3 = format_3 * 15/18;


	if (g_o_slope_sign == 0)
		format_4 = ((format_3 * 100) / (165+g_o_slope));
	else
		format_4 = ((format_3 * 100) / (165-g_o_slope));

	format_4 = format_4 - (2 * format_4);
	t_current = format_1 + format_4;

	return t_current;
}

static void thermal_calibration(void)
{
	if (g_adc_cali_en_t == 0)
		pr_crit("Not Calibration\n");
	thermal_cal_prepare_2(0);
}

static int get_immediate_temp1(void)
{
	int curr_raw1, curr_temp1;

	mutex_lock(&TS_lock);

	curr_raw1 = DRV_Reg32(TEMPMSR0);
	curr_raw1 = curr_raw1 & 0x0fff;
	curr_temp1 = raw_to_temperature_MCU1(curr_raw1);
	curr_temp1 = curr_temp1*100;

	mutex_unlock(&TS_lock);
	return curr_temp1;
}

static int get_immediate_temp2(void)
{
	int curr_raw2, curr_temp2;
	mutex_lock(&TS_lock);

	curr_raw2 = DRV_Reg32(TEMPMSR1);
	curr_raw2 = curr_raw2 & 0x0fff;
	curr_temp2 = raw_to_temperature_MCU2(curr_raw2);
	curr_temp2 = curr_temp2*100;

	mutex_unlock(&TS_lock);
	return curr_temp2;
}

int get_immediate_temp2_wrap(void)
{
	int curr_raw;
	curr_raw = get_immediate_temp2();
	return curr_raw;
}

int CPU_Temp(void)
{
	int curr_temp, curr_temp2;
	curr_temp = get_immediate_temp1();
	curr_temp2 = get_immediate_temp2();

#if CONFIG_SUPPORT_MET_MTKTSCPU
	if (met_mtktscpu_dbg)
		trace_printk("%d,%d\n", curr_temp, curr_temp2);
#endif
	return curr_temp;
}

int mtktscpu_get_cpu_temp(void)
{
	int curr_temp;

	curr_temp = get_immediate_temp1();
	if ((curr_temp > MTKTSCPU_TEMP_CRIT) || (curr_temp < -30000))
		mtktscpu_dprintk("[Power/CPU_Thermal] CPU T=%d\n", curr_temp);

	return (unsigned long) curr_temp;
}

static int mtktscpu_match_cdev(struct thermal_cooling_device *cdev,
			       struct trip_t *trip,
			       int *index)
{
	int i;
	if (!strlen(cdev->type))
		return -EINVAL;

	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		if (!strcmp(cdev->type, trip->cdev[i].type)) {
			*index = i;
			return 0;
		}
	return -ENODEV;
}

static int mtktscpu_cdev_bind(struct thermal_zone_device *thermal,
			      struct thermal_cooling_device *cdev)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	struct trip_t *trip = NULL;
	int index = -1;
	struct cdev_t *cool_dev;
	unsigned long max_state, upper, lower;
	int i, ret = -EINVAL;

	cdev->ops->get_max_state(cdev, &max_state);

	for (i = 0; i < pdata->num_trips; i++) {
		trip = &pdata->trips[i];

		if (mtktscpu_match_cdev(cdev, trip, &index))
			continue;

		if (index == -1)
			return -EINVAL;

		cool_dev = &(trip->cdev[index]);
		lower = cool_dev->lower;
		upper =  cool_dev->upper > max_state ? max_state : cool_dev->upper;
		ret = thermal_zone_bind_cooling_device(thermal,
						       i,
						       cdev,
						       upper,
						       lower);
		dev_info(&cdev->device, "%s bind to %d: idx=%d: u=%ld: l=%ld: %d-%s\n", cdev->type,
			 i, index, upper, lower, ret, ret ? "fail" : "succeed");
	}
	return ret;
}

static int mtktscpu_cdev_unbind(struct thermal_zone_device *thermal,
				struct thermal_cooling_device *cdev)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	struct trip_t *trip;
	int i, ret = -EINVAL;
	int index = -1;

	for (i = 0; i < pdata->num_trips; i++) {
		trip = &pdata->trips[i];
		if (mtktscpu_match_cdev(cdev, trip, &index))
			continue;
		ret = thermal_zone_unbind_cooling_device(thermal, i, cdev);
		dev_info(&cdev->device, "%s unbind from %d: %s\n", cdev->type,
			 i, ret ? "fail" : "succeed");
	}
	return ret;
}

static int mtktscpu_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *t)
{
	int curr_temp = CPU_Temp();

	if ((curr_temp > MTKTSCPU_TEMP_CRIT) || (curr_temp < -30000))
		pr_notice("[Power/CPU_Thermal] CPU T=%d\n", curr_temp);

	*t = (unsigned long)curr_temp;
	return 0;
}

static int mtktscpu_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	*mode = pdata->mode;
	return 0;
}

static int mtktscpu_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktscpu_get_trip_type(struct thermal_zone_device *thermal,
				  int trip,
				  enum thermal_trip_type *type)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktscpu_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *t)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktscpu_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long t)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktscpu_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
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

static int mtktscpu_get_trip_hyst(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *hyst)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*hyst = pdata->trips[trip].hyst;
	return 0;
}

static int mtktscpu_set_trip_hyst(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long hyst)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	pdata->trips[trip].hyst = hyst;
	return 0;
}

static struct thermal_zone_device_ops mtktscpu_dev_ops = {
	.bind = mtktscpu_cdev_bind,
	.unbind = mtktscpu_cdev_unbind,
	.get_temp = mtktscpu_get_temp,
	.get_mode = mtktscpu_get_mode,
	.set_mode = mtktscpu_set_mode,
	.get_trip_type = mtktscpu_get_trip_type,
	.get_trip_temp = mtktscpu_get_trip_temp,
	.set_trip_temp = mtktscpu_set_trip_temp,
	.get_crit_temp = mtktscpu_get_crit_temp,
	.get_trip_hyst = mtktscpu_get_trip_hyst,
	.set_trip_hyst = mtktscpu_set_trip_hyst,
};

static void mtktscpu_work(struct work_struct *work)
{
	struct mtktscpu_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktscpu_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static ssize_t trips_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->trips);
}
static ssize_t trips_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int trips = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	if (sscanf(buf, "%d\n", &trips) != 1)
		return -EINVAL;
	if (trips < 0)
		return -EINVAL;

	pdata->num_trips = trips;
	thermal->trips = pdata->num_trips;
	return count;
}
static ssize_t polling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->polling_delay);
}
static ssize_t polling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int polling_delay = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;

	if (sscanf(buf, "%d\n", &polling_delay) != 1)
		return -EINVAL;
	if (polling_delay < 0)
		return -EINVAL;

	pdata->polling_delay = polling_delay;
	thermal->polling_delay = pdata->polling_delay;
	thermal_zone_device_update(thermal);
	return count;
}

static DEVICE_ATTR(trips, S_IRUGO | S_IWUSR, trips_show, trips_store);
static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR, polling_show, polling_store);

static int mtktscpu_create_sysfs(struct mtktscpu_thermal_zone *tzone)
{
	int ret = 0;
	ret = device_create_file(&tzone->tz->device, &dev_attr_polling);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_trips);
	if (ret)
		pr_err("%s Failed to create trips attr\n", __func__);
	return ret;
}

static int mtktscpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktscpu_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata)
		return -EINVAL;

	thermal_cal_prepare();
	thermal_calibration();
	/* turn off the sensor buffer to save power */
	THERMAL_WRAP_WR32(DRV_Reg32(TS_CON0) | 0x000000C0, TS_CON0);
	thermal_reset_and_initial();
	set_thermal_ctrl_trigger_SPM(MTKTSCPU_TEMP_CRIT);

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktscpu",
						 pdata->num_trips,
						 (1 << pdata->num_trips) - 1,
						 tzone,
						 &mtktscpu_dev_ops,
						 NULL,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktscpu thermal zone device\n", __func__);
		return -EINVAL;
	}

	ret = request_irq(THERM_CTRL_IRQ_ID, thermal_interrupt_handler, IRQF_TRIGGER_LOW, THERMAL_NAME, NULL);
	if (ret)
		pr_err("%s IRQ register fail\n", __func__);

	ret = mtktscpu_create_sysfs(tzone);
	INIT_WORK(&tzone->therm_work, mtktscpu_work);
	platform_set_drvdata(pdev, tzone);
	pdata->mode = THERMAL_DEVICE_ENABLED;

	return 0;
}

static int mtktscpu_remove(struct platform_device *pdev)
{
	struct mtktscpu_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static int mtktscpu_suspend(struct platform_device *dev, pm_message_t state)
{
	int temp = 0;
	int cnt = 0;
	while (cnt < 30) {
		temp = DRV_Reg32(TEMPMSRCTL1);
		mtktscpu_dprintk("TEMPMSRCTL1 = 0x%x\n", temp);

		/*
		  TEMPMSRCTL1[7]:Temperature measurement bus status[1]
		  TEMPMSRCTL1[0]:Temperature measurement bus status[0]

		  00: IDLE                           <=can pause,TEMPMSRCTL1[7][0]=0x00
		  01: Write transaction              <=can not pause,TEMPMSRCTL1[7][0]=0x01
		  10: Read transaction               <=can not pause,TEMPMSRCTL1[7][0]=0x10
		  11: Waiting for read after Write   <=can pause    ,TEMPMSRCTL1[7][0]=0x11
		*/

		if (((temp & 0x81) == 0x00) || ((temp & 0x81) == 0x81)) {

			/*
			  Pause periodic temperature measurement for
			  sensing point 0,sensing point 1,sensing point 2
			*/

			DRV_WriteReg32(TEMPMSRCTL1, (temp | 0x0E));
			break;
		}
		mtktscpu_dprintk("temp=0x%x, cnt=%d\n", temp, cnt);
		udelay(10);
		cnt++;
	}

	THERMAL_WRAP_WR32(0x00000000, TEMPMONCTL0);

	/*
	  fix ALPS00848017
	  can't turn off thermal, this will cause PTPOD  issue abnormal interrupt
	  and let system crash.(because PTPOD can't get thermal's temperature)
	*/

	THERMAL_WRAP_WR32(DRV_Reg32(TS_CON0) | 0x000000C0, TS_CON0);

	return 0;
}

static int mtktscpu_resume(struct platform_device *dev)
{
	thermal_reset_and_initial();
	set_thermal_ctrl_trigger_SPM(MTKTSCPU_TEMP_CRIT);
	return 0;
}

static struct platform_driver mtk_thermal_driver = {
	.probe = mtktscpu_probe,
	.remove = mtktscpu_remove,
	.suspend = mtktscpu_suspend,
	.resume = mtktscpu_resume,
	.driver = {
		.name = THERMAL_NAME,
	},
};

static int __init mtktscpu_init(void)
{
	return platform_driver_register(&mtk_thermal_driver);
}

static void __exit mtktscpu_exit(void)
{
	platform_driver_unregister(&mtk_thermal_driver);
}

module_init(mtktscpu_init);
module_exit(mtktscpu_exit);
