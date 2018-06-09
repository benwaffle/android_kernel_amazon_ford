/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt6582_auxadc.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt6582 AUXADC
 *
 * Author:
 * -------
 * Zhong Wang
 *
 ****************************************************************************/

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/mt_gpt.h>
#include <mach/mt_clkmgr.h>
#include <mach/sync_write.h>
#include <cust_adc.h>		/*generate by DCT Tool */

#include "mt_auxadc_sw.h"
#include "mt_auxadc_hw.h"

#include <linux/miscdevice.h>

#define DRV_ClearBits(addr, data)     {\
   kal_uint16 temp;\
   temp = DRV_Reg(addr);\
   temp &=~(data);\
   mt65xx_reg_sync_writew(temp, addr);\
}

#define DRV_SetBits(addr, data)     {\
   kal_uint16 temp;\
   temp = DRV_Reg(addr);\
   temp |= (data);\
   mt65xx_reg_sync_writew(temp, addr);\
}

#define DRV_SetData(addr, bitmask, value)     {\
   kal_uint16 temp;\
   temp = (~(bitmask)) & DRV_Reg(addr);\
   temp |= (value);\
   mt65xx_reg_sync_writew(temp, addr);\
}

#define AUXADC_DRV_ClearBits16(addr, data)           DRV_ClearBits(addr,data)
#define AUXADC_DRV_SetBits16(addr, data)             DRV_SetBits(addr,data)
#define AUXADC_DRV_WriteReg16(addr, data)            mt65xx_reg_sync_writew(data, addr)
#define AUXADC_DRV_ReadReg16(addr)                   DRV_Reg(addr)
#define AUXADC_DRV_SetData16(addr, bitmask, value)   DRV_SetData(addr, bitmask, value)

#define AUXADC_DVT_DELAYMACRO(u4Num)                                     \
{                                                                        \
    unsigned int u4Count = 0 ;                                           \
    for (u4Count = 0; u4Count < u4Num; u4Count++ );                      \
}

#define AUXADC_CLR_BITS(BS,REG)     {\
   kal_uint32 temp;\
   temp = DRV_Reg32(REG);\
   temp &=~(BS);\
   mt65xx_reg_sync_writel(temp, REG);\
}

#define AUXADC_SET_BITS(BS,REG)     {\
   kal_uint32 temp;\
   temp = DRV_Reg32(REG);\
   temp |= (BS);\
   mt65xx_reg_sync_writel(temp, REG);\
}

#define VOLTAGE_FULL_RANGE  1500	/* VA voltage */
#define AUXADC_PRECISE      4096	/* 12 bits */
#define ADC_NAME    "dvt-adc"

#define ReadREG(reg)         (*((volatile const u16 *)(reg)))
#define WriteREG(reg, val)   ((*((volatile u16 *)(reg))) = ((u16)(val)))
#define MskWriteREG(reg, val, msk)   WriteREG((reg), ((ReadREG((reg)) & (~((u16)(msk)))) | (((u16)(val)) & ((u16)(msk)))))

struct udvt_cmd {
	int cmd;
	int value;
};
static int adc_using_set_clr_mode = 1;
static unsigned int adc_auto_set_need_trigger_1st_sample;
static int adc_run;
static int adc_log_en;
static int adc_bg_detect_en;
static int adc_wakeup_src_en;

unsigned int auxadc_sample_data[4] = { 0 };
unsigned int auxadc_select_channel[4] = { 0 };


struct timer_list adc_udvt_timer;
struct tasklet_struct adc_udvt_tasklet;

/*****************************************************************************
 * Integrate with NVRAM 
****************************************************************************/
static DEFINE_MUTEX(mutex_get_cali_value);
static int adc_auto_set = 0;

static u16 mt_tpd_read_adc(u16 pos)
{
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_ADDR, pos);
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_CON0, 0x01);
	while (0x01 & AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_TP_CON0)) {;
	}			//wait for write finish
	return AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_TP_DATA0);
}

static void mt_auxadc_disable_penirq(void)
{
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_CMD, 1);
	mt_tpd_read_adc(TP_CMD_ADDR_X);
}


/*
step1 check con2 if auxadc is busy
step2 clear bit
step3  read channle and make sure old ready bit ==0
step4 set bit  to trigger sample
step5  read channle and make sure  ready bit ==1
step6 read data 
*/

int IMM_auxadc_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
{
	unsigned int channel[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int idle_count = 0;
	int data_ready_count = 0;

	mutex_lock(&mutex_get_cali_value);
#if 0
	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		printk("hwEnableClock AUXADC failed.");
	}
#endif
	if (dwChannel == PAD_AUX_XP)
		mt_auxadc_disable_penirq();
	/*step1 check con2 if auxadc is busy */
	while ((*(volatile u16 *)AUXADC_CON2) & 0x01) {
		msleep(100);
		idle_count++;
		if (idle_count > 30) {
			/*wait for idle time out */
			printk(KERN_ERR "[adc_api]: wait for auxadc idle time out\n");
			mutex_unlock(&mutex_get_cali_value);
			return -1;
		}
	}
	/* step2 clear bit */
	if (0 == adc_auto_set)
		AUXADC_DRV_ClearBits16((volatile u16 *)AUXADC_CON1, (1 << dwChannel));

	/*step3  read channle and make sure old ready bit ==0 */
	while ((*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & (1 << 12)) {
		msleep(10);
		data_ready_count++;
		if (data_ready_count > 30) {
			printk(KERN_ERR "[adc_api]: wait for channel[%d] ready bit clear time out\n",
			       dwChannel);
			mutex_unlock(&mutex_get_cali_value);
			return -2;
		}
	}

	/*step4 set bit  to trigger sample */
	if (0 == adc_auto_set) {
		AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON1, (1 << dwChannel));
	}
	/*step5  read channle and make sure  ready bit ==1 */
	udelay(25);		/*we must dealay here for hw sample cahnnel data */
	while (0 == ((*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & (1 << 12))) {
		msleep(10);
		data_ready_count++;

		if (data_ready_count > 30) {
			printk(KERN_ERR "[adc_api]: wait for channel[%d] data ready time out\n", dwChannel);
			mutex_unlock(&mutex_get_cali_value);
			return -3;
		}
	}

	channel[dwChannel] = (*(volatile u16 *)(AUXADC_DAT0 + dwChannel * 0x04)) & 0x0FFF;
	if (NULL != rawdata) {
		*rawdata = channel[dwChannel];
	}

	data[0] = (channel[dwChannel] * 150 / AUXADC_PRECISE / 100);
	data[1] = ((channel[dwChannel] * 150 / AUXADC_PRECISE) % 100);

#if 0
	if (disable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		printk("hwEnableClock AUXADC failed.");
	}
#endif
	mutex_unlock(&mutex_get_cali_value);

	return 0;

}

/* this function voltage Unit is uv */
int IMM_auxadc_GetOneChannelValue_Cali(int Channel, int *voltage)
{
	int ret = 0, data[4];
	int rawvalue[4];
	ret = IMM_auxadc_GetOneChannelValue(Channel, data, rawvalue);
	if (ret) {
		printk(KERN_ERR "[adc_api]:IMM_auxadc_GetOneChannelValue_Cali  get raw value error %d \n",
		       ret);
		return -1;
	}
	*voltage = (rawvalue[0]) * 1500 / AUXADC_PRECISE;
	*voltage = *voltage*1000;

	return 0;

}

#if 0
static int IMM_auxadc_get_evrage_data(int times, int Channel)
{
	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0;

	i = times;
	while (i--) {
		ret_value = IMM_auxadc_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		printk("[auxadc_get_data(channel%d)]: ret_temp=%d\n", Channel, ret_temp);
	}

	ret = ret / times;
	return ret;
}
#endif

static long adc_udvt_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* void __user *uarg = (void __user *)arg; */
	struct udvt_cmd *pcmd = (struct udvt_cmd *)arg;

	int voltage[4] = { 0 };

/*      printk(KERN_EMERG"cmd:%d, value:0x%x\n", pcmd->cmd, pcmd->value);   */

	switch (pcmd->cmd) {
	case ENABLE_ADC_CLOCK:
#if 0
		if (enable_clock(MT_CG_PERI_AUXADC_PDN, "AUXADC") == 0) {
			printk(KERN_EMERG "[adc_udvt]: ENABLE_ADC_CLOCK\n");
		} else {
			printk(KERN_EMERG "[adc_udvt]: enable_clock failed.\n");
		}
#endif
		break;

	case DISABLE_ADC_CLOCK:
#if 0
		if (disable_clock(MT_CG_PERI_AUXADC_PDN, "AUXADC") == 0) {
			printk(KERN_EMERG "[adc_udvt]: DISABLE_ADC_CLOCK\n");
		} else {
			printk(KERN_EMERG "[adc_udvt]: disable_clock failed.\n");
		}
#endif
		break;
#if 0
	case ENABLE_ADC_DCM:
		printk(KERN_EMERG "[adc_udvt]: dcm_enable(ALL_DCM)\n");
		dcm_enable(ALL_DCM);
		break;
#endif
	case ENABLE_ADC_AUTO_SET:
		adc_auto_set = 1;
		adc_auto_set_need_trigger_1st_sample = 0xffff;
		WriteREG(AUXADC_CON0, 0xffff);
		break;

	case DISABLE_ADC_AUTO_SET:
		adc_auto_set = 0;
		adc_auto_set_need_trigger_1st_sample = 0;
		WriteREG(AUXADC_CON0, 0x0000);
		break;

	case SET_AUXADC_CON0:
		WriteREG(AUXADC_CON0, pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_CON1:
		WriteREG(AUXADC_CON1, pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_CON1_SET:
		adc_using_set_clr_mode = 1;
		break;

	case SET_AUXADC_CON1_CLR:
		adc_using_set_clr_mode = 0;
		break;

	case SET_AUXADC_CON2:
		*(volatile u16 *)AUXADC_CON2 = (u16) (pcmd->value & 0xFFFF);
		break;
#if 0

	case SET_AUXADC_CON3:
		*(volatile u16 *)AUXADC_CON3 = (u16) (pcmd->value & 0xFFFF);
		break;
#endif

	case SET_AUXADC_DAT0:
		*(volatile u16 *)AUXADC_DAT0 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT1:
		*(volatile u16 *)AUXADC_DAT1 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT2:
		*(volatile u16 *)AUXADC_DAT2 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT3:
		*(volatile u16 *)AUXADC_DAT3 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT4:
		*(volatile u16 *)AUXADC_DAT4 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT5:
		*(volatile u16 *)AUXADC_DAT5 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT6:
		*(volatile u16 *)AUXADC_DAT6 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT7:
		*(volatile u16 *)AUXADC_DAT7 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT8:
		*(volatile u16 *)AUXADC_DAT8 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT9:
		*(volatile u16 *)AUXADC_DAT9 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT10:
		*(volatile u16 *)AUXADC_DAT10 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT11:
		*(volatile u16 *)AUXADC_DAT11 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT12:
		*(volatile u16 *)AUXADC_DAT12 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT13:
		*(volatile u16 *)AUXADC_DAT13 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT14:
		*(volatile u16 *)AUXADC_DAT13 = (u16) (pcmd->value & 0xFFFF);
		break;

	case SET_AUXADC_DAT15:
		*(volatile u16 *)AUXADC_DAT13 = (u16) (pcmd->value & 0xFFFF);
		break;

	case GET_AUXADC_CON0:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON0) & 0xFFFF);
		break;

	case GET_AUXADC_CON1:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON1) & 0xFFFF);
		break;

	case GET_AUXADC_CON2:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON2) & 0xFFFF);
		break;
#if 0

	case GET_AUXADC_CON3:
		pcmd->value = ((*(volatile u16 *)AUXADC_CON3) & 0xFFFF);
		break;
#endif

	case GET_AUXADC_DAT0:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT0) & 0xFFFF);
		break;

	case GET_AUXADC_DAT1:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT1) & 0xFFFF);
		break;

	case GET_AUXADC_DAT2:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT2) & 0xFFFF);
		break;

	case GET_AUXADC_DAT3:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT3) & 0xFFFF);
		break;

	case GET_AUXADC_DAT4:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT4) & 0xFFFF);
		break;

	case GET_AUXADC_DAT5:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT5) & 0xFFFF);
		break;

	case GET_AUXADC_DAT6:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT6) & 0xFFFF);
		break;

	case GET_AUXADC_DAT7:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT7) & 0xFFFF);
		break;

	case GET_AUXADC_DAT8:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT8) & 0xFFFF);
		break;

	case GET_AUXADC_DAT9:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT9) & 0xFFFF);
		break;

	case GET_AUXADC_DAT10:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT10) & 0xFFFF);
		break;

	case GET_AUXADC_DAT11:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT11) & 0xFFFF);
		break;

	case GET_AUXADC_DAT12:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT12) & 0xFFFF);
		break;

	case GET_AUXADC_DAT13:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT13) & 0xFFFF);
		break;

	case GET_AUXADC_DAT14:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT13) & 0xFFFF);
		break;

	case GET_AUXADC_DAT15:
		pcmd->value = ((*(volatile u16 *)AUXADC_DAT13) & 0xFFFF);
		break;
#if 0

	case ENABLE_SYN_MODE:
		adc_mode = 1;
		break;

	case DISABLE_SYN_MODE:
		adc_mode = 0;
		break;
#endif

	case ENABLE_ADC_RUN:
		adc_run = 1;

#if 0
		printk(KERN_EMERG "hwDisableClock AUXADC.");

		if (disable_clock(MT65XX_PDN_PERI_AUXADC, "AUXADC")) {
			printk(KERN_EMERG "hwEnableClock AUXADC failed.");
		}
#endif
		break;

	case DISABLE_ADC_RUN:
		adc_run = 0;
		break;

	case ENABLE_ADC_LOG:
		adc_log_en = 1;
		break;

	case DISABLE_ADC_LOG:
		adc_log_en = 0;
		break;

	case ENABLE_BG_DETECT:
		adc_bg_detect_en = 1;
		break;

	case DISABLE_BG_DETECT:
		adc_bg_detect_en = 0;
		break;
#if 0

	case ENABLE_3G_TX:
		adc_3g_tx_en = 1;
		break;

	case DISABLE_3G_TX:
		adc_3g_tx_en = 0;
		break;

	case ENABLE_2G_TX:
		adc_2g_tx_en = 1;
		break;

	case DISABLE_2G_TX:
		adc_2g_tx_en = 0;
		break;
#endif

	case SET_ADC_WAKE_SRC:
		/* slp_set_wakesrc(WAKE_SRC_CFG_KEY|WAKE_SRC_LOW_BAT, TRUE, FALSE); */
		adc_wakeup_src_en = 1;
		break;

	case SET_DET_VOLT:
		(*(volatile u16 *)AUXADC_DET_VOLT) = pcmd->value;
		printk(KERN_DEBUG "AUXADC_DET_VOLT: 0x%x\n", (*(volatile u16 *)AUXADC_DET_VOLT));
		break;

	case SET_DET_PERIOD:
		(*(volatile u16 *)AUXADC_DET_PERIOD) = pcmd->value;
		printk(KERN_DEBUG "AUXADC_DET_PERIOD: 0x%x\n",
		       (*(volatile u16 *)AUXADC_DET_PERIOD));
		break;

	case SET_DET_DEBT:
		(*(volatile u16 *)AUXADC_DET_DEBT) = pcmd->value;
		printk(KERN_DEBUG "AUXADC_DET_DEBT: 0x%x\n", (*(volatile u16 *)AUXADC_DET_DEBT));
		break;

	case Set_Adc_Channel0:
		IMM_auxadc_GetOneChannelValue_Cali(0, auxadc_sample_data);
		printk(KERN_DEBUG "Get channel0 voltage value: 0x%x!\n", auxadc_sample_data[0]);

		break;

	case Set_Adc_Channel1:
		IMM_auxadc_GetOneChannelValue_Cali(1, auxadc_sample_data);
		printk(KERN_DEBUG "Get channel0 voltage value: 0x%x!\n", auxadc_sample_data[0]);

		break;

	case Set_Adc_Channel2:
		IMM_auxadc_GetOneChannelValue_Cali(2, auxadc_sample_data);
		printk(KERN_DEBUG "Get channel0 voltage value: 0x%x!\n", auxadc_sample_data[0]);

		break;

	case Set_Adc_Channel3:
		IMM_auxadc_GetOneChannelValue_Cali(3, auxadc_sample_data);
		printk(KERN_DEBUG "Get channel0 voltage value: 0x%x!\n", auxadc_sample_data[0]);

		break;

	case Set_Adc_Channel4:
		IMM_auxadc_GetOneChannelValue_Cali(4, auxadc_sample_data);
		printk(KERN_DEBUG "Get channel0 voltage value: 0x%x!\n", auxadc_sample_data[0]);

		break;

	case Set_Adc_Channel5:
		IMM_auxadc_GetOneChannelValue_Cali(5, auxadc_sample_data);
		printk(KERN_DEBUG "Get channel0 voltage value: 0x%x!\n", auxadc_sample_data[0]);

		break;

	default:
		return -EINVAL;
	}

	return 0;
}


static int adc_udvt_dev_open(struct inode *inode, struct file *file)
{
	/* if(hwEnableClock(MT65XX_PDN_PERI_TP,"Touch")==FALSE) */
	/* printk(KERN_EMERG"hwEnableClock TP failed.\n"); */

	return 0;
}

static int adc_udvt_dev_read(struct file *file, int __user * buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;

	int i = 0, data[4] = { 0 };
	int rawvalue[4] = { 0 };
	i = auxadc_select_channel[0];

	ret = IMM_auxadc_GetOneChannelValue(i, data, rawvalue);

	if (copy_to_user(buf, data, count))
	{
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;

		rawvalue[0] = rawvalue[0] * 1500000 / AUXADC_PRECISE;

		printk(KERN_DEBUG "[adc_api]: 2!imm mode raw data => channel[%d] = %d,%d\n", i, rawvalue[0],
		       *rawvalue);

	}


	return ret;
}

static int adc_udvt_dev_write(struct file *file, int __user * buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int i = 0, data[4] = { 0 };
	int ret = 0;

	if (copy_from_user(data, buf, count)) {
		printk(KERN_ERR "copy_from_user fail!\n");
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
	}
	auxadc_select_channel[0] = data[0];	/*get select channel from user space*/
	return ret;

}


static struct file_operations adc_udvt_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_udvt_dev_ioctl,
	.open = adc_udvt_dev_open,
	.read = adc_udvt_dev_read,
	.write = adc_udvt_dev_write,
};

static struct miscdevice adc_udvt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = ADC_NAME,
	.fops = &adc_udvt_dev_fops,
};

void adc_udvt_tasklet_fn()
{

	unsigned int i = 0, data[4] = { 0 };
	int res = 0;

	res = IMM_auxadc_GetOneChannelValue_Cali(i, data);

	if (res < 0)
		printk(KERN_ERR "[adc_udvt]: get data error\n");
	else
		printk(KERN_DEBUG "[adc_udvt]: channel[%2u]=%4u mV\n", i, data[0]);
	return;
}

static void mt_auxadc_cal_prepare(void)
{
	//mt6582 no voltage calibration
	int ret;

	ret = misc_register(&adc_udvt_dev);
	if (ret) {
		printk("[adc_udvt]: register driver failed (%d)\n", ret); 
      /* return ret; */
	}

	tasklet_init(&adc_udvt_tasklet, adc_udvt_tasklet_fn, 0);
}

void mt_auxadc_hal_init(void)
{
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_MISC, 1 << 14);

	mt_auxadc_cal_prepare();

	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);
}

void mt_auxadc_hal_suspend(void)
{
	if (disable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		printk(KERN_ERR "hwEnableClock AUXADC failed.");
	}

}

void mt_auxadc_hal_resume(void)
{

	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		printk(KERN_ERR "hwEnableClock AUXADC again!!!.");
		if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
			printk(KERN_ERR "hwEnableClock AUXADC failed.");
		}

	}

	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_MISC, 1 << 14);	/*power on ADC */    
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);	/*disable RTP */
}

int mt_auxadc_dump_register(char *buf)
{
	if (buf == NULL) {
		pr_debug("[%s] Invalid input!!\n", __func__);
		return 0;
	}

	if (strlen(buf) < 64) {
		pr_debug("[%s] Invalid input!!\n", __func__);
		return 0;
	}

	printk(KERN_DEBUG "[auxadc]: AUXADC_CON0=%x\n", *(volatile u16 *)AUXADC_CON0);
	printk(KERN_DEBUG "[auxadc]: AUXADC_CON1=%x\n", *(volatile u16 *)AUXADC_CON1);
	printk(KERN_DEBUG "[auxadc]: AUXADC_CON2=%x\n", *(volatile u16 *)AUXADC_CON2);

	return sprintf(buf, "AUXADC_CON0:%x\n AUXADC_CON1:%x\n AUXADC_CON2:%x\n",
		       *(volatile u16 *)AUXADC_CON0, *(volatile u16 *)AUXADC_CON1,
		       *(volatile u16 *)AUXADC_CON2);
}
