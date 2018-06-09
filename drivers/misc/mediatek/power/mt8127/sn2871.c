#if 0
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>

#include <mt-plat/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "sn2871.h"
#include <mach/mt_charging.h>
#include <mt-plat/charging.h>

#if defined(CONFIG_MTK_FPGA)
#else
#ifdef CONFIG_OF
#else
#include <cust_i2c.h>
#endif
#endif
#endif
#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <mach/charging.h>
//#include "charge.h"
#include "sn2871.h"
/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/


#ifdef CONFIG_OF
#else

#define sn2871_SLAVE_ADDR_WRITE   0xD6
#define sn2871_SLAVE_ADDR_Read    0xD7

#define sn2871_BUSNUM 2

#endif
static struct i2c_client *new_client;
static const struct i2c_device_id sn2871_i2c_id[] = { {"sn2871", 0}, {} };

extern kal_bool chargin_hw_init_done;
static int sn2871_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);


/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
unsigned char sn2871_reg[sn2871_REG_NUM] = { 0 };

static DEFINE_MUTEX(sn2871_i2c_access);

int g_sn2871_hw_exist = 0;

/**********************************************************
  *
  *   [I2C Function For Read/Write sn2871]
  *
  *********************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int sn2871_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&sn2871_i2c_access);

	/* new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG; */
	new_client->ext_flag =
	    ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
		new_client->ext_flag = 0;
		mutex_unlock(&sn2871_i2c_access);

		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
	new_client->ext_flag = 0;
	mutex_unlock(&sn2871_i2c_access);

	return 1;
}

unsigned int sn2871_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&sn2871_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		new_client->ext_flag = 0;
		mutex_unlock(&sn2871_i2c_access);
		return 0;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&sn2871_i2c_access);
	return 1;
}
#else
unsigned int sn2871_read_byte(unsigned char cmd, unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&sn2871_i2c_access);

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = new_client->addr,
				.flags = 0,
				.len = 1,
				.buf = &cmd,
			},
			{

				.addr = new_client->addr,
				.flags = I2C_M_RD,
				.len = 1,
				.buf = returnData,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			battery_xlog_printk(BAT_LOG_CRTI, "skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&sn2871_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int sn2871_write_byte(unsigned char cmd, unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];

	mutex_lock(&sn2871_i2c_access);

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
				.addr = new_client->addr,
				.flags = 0,
				.len = 1 + 1,
				.buf = buf,
			},
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			battery_xlog_printk(BAT_LOG_CRTI, "skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&sn2871_i2c_access);

	return ret == xfers ? 1 : -1;
}
#endif
/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
unsigned int sn2871_read_interface(unsigned char RegNum, unsigned char *val, unsigned char MASK,
				  unsigned char SHIFT)
{
	unsigned char sn2871_reg = 0;
	unsigned int ret = 0;

	ret = sn2871_read_byte(RegNum, &sn2871_reg);


	sn2871_reg &= (MASK << SHIFT);
	*val = (sn2871_reg >> SHIFT);

	battery_xlog_printk(BAT_LOG_FULL, "[sn2871_read_interface] val=0x%x\n", *val);

	return ret;
}

unsigned int sn2871_config_interface(unsigned char RegNum, unsigned char val, unsigned char MASK,
				    unsigned char SHIFT)
{
	unsigned char sn2871_reg = 0;
	unsigned int ret = 0;

	ret = sn2871_read_byte(RegNum, &sn2871_reg);
	battery_xlog_printk(BAT_LOG_FULL, "[sn2871_config_interface] Reg[%x]=0x%x\n", RegNum, sn2871_reg);

	sn2871_reg &= ~(MASK << SHIFT);
	sn2871_reg |= (val << SHIFT);

	ret = sn2871_write_byte(RegNum, sn2871_reg);
	battery_xlog_printk(BAT_LOG_FULL, "[sn2871_config_interface] write Reg[%x]=0x%x\n", RegNum,
		    sn2871_reg);

	/* Check */
	/* sn2871_read_byte(RegNum, &sn2871_reg); */
	/* printk("[sn2871_config_interface] Check Reg[%x]=0x%x\n", RegNum, sn2871_reg); */

	return ret;
}

/* write one register directly */
unsigned int sn2871_reg_config_interface(unsigned char RegNum, unsigned char val)
{
	unsigned int ret = 0;

	ret = sn2871_write_byte(RegNum, val);

	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */

void sn2871_set_en_hiz(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_HIZ_MASK),
				       (unsigned char) (CON0_EN_HIZ_SHIFT)
	    );
}

void sn2871_set_en_ilim(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_ILIM_MASK),
				       (unsigned char) (CON0_EN_ILIM_SHIFT)
	    );
}

void sn2871_set_iinlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON0),
				       (val),
				       (unsigned char) (CON0_IINLIM_MASK),
				       (unsigned char) (CON0_IINLIM_SHIFT)
	    );
}

unsigned int sn2871_get_iinlim(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON0),
				     (&val),
				     (unsigned char) (CON0_IINLIM_MASK), (unsigned char) (CON0_IINLIM_SHIFT)
	    );
	return val;
}



/* CON1---------------------------------------------------- */

void sn2871_ADC_start(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_CONV_START_MASK),
				       (unsigned char) (CON2_CONV_START_SHIFT)
	    );
}

void sn2871_set_ADC_rate(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_CONV_RATE_MASK),
				       (unsigned char) (CON2_CONV_RATE_SHIFT)
	    );
}

void sn2871_set_vindpm_os(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_VINDPM_OS_MASK),
				       (unsigned char) (CON1_VINDPM_OS_SHIFT)
	    );
}

/* CON2---------------------------------------------------- */

void sn2871_set_ico_en_start(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_ICO_EN_MASK),
				       (unsigned char) (CON2_ICO_EN_RATE_SHIFT)
	    );
}



/* CON3---------------------------------------------------- */

void sn2871_wd_reset(unsigned int val)
{
	unsigned int ret = 0;


	ret = sn2871_config_interface((unsigned char) (sn2871_CON3),
				       (val),
				       (unsigned char) (CON3_WD_MASK), (unsigned char) (CON3_WD_SHIFT)
	    );

}

void sn2871_otg_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = sn2871_config_interface((unsigned char) (sn2871_CON3),
				       (val),
				       (unsigned char) (CON3_OTG_CONFIG_MASK),
				       (unsigned char) (CON3_OTG_CONFIG_SHIFT)
	    );

}

void sn2871_chg_en(unsigned int val)
{
	unsigned int ret = 0;


	ret = sn2871_config_interface((unsigned char) (sn2871_CON3),
				       (val),
				       (unsigned char) (CON3_CHG_CONFIG_MASK),
				       (unsigned char) (CON3_CHG_CONFIG_SHIFT)
	    );

}

unsigned int sn2871_get_chg_en(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON3),
				     (&val),
				     (unsigned char) (CON3_CHG_CONFIG_MASK),
				     (unsigned char) (CON3_CHG_CONFIG_SHIFT)
	    );
	return val;
}


void sn2871_set_sys_min(unsigned int val)
{
	unsigned int ret = 0;


	ret = sn2871_config_interface((unsigned char) (sn2871_CON3),
				       (val),
				       (unsigned char) (CON3_SYS_V_LIMIT_MASK),
				       (unsigned char) (CON3_SYS_V_LIMIT_SHIFT)
	    );

}

/* CON4---------------------------------------------------- */

void sn2871_en_pumpx(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_EN_PUMPX_MASK),
				       (unsigned char) (CON4_EN_PUMPX_SHIFT)
	    );
}


void sn2871_set_ichg(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_ICHG_MASK), (unsigned char) (CON4_ICHG_SHIFT)
	    );
}

unsigned int sn2871_get_reg_ichg(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON4),
				     (&val),
				     (unsigned char) (CON4_ICHG_MASK), (unsigned char) (CON4_ICHG_SHIFT)
	    );
	return val;
}

/* CON5---------------------------------------------------- */

void sn2871_set_iprechg(unsigned int val)
{
	unsigned int ret = 0;


	ret = sn2871_config_interface((unsigned char) (sn2871_CON5),
				       (val),
				       (unsigned char) (CON5_IPRECHG_MASK),
				       (unsigned char) (CON5_IPRECHG_SHIFT)
	    );

}

void sn2871_set_iterml(unsigned int val)
{
	unsigned int ret = 0;


	ret = sn2871_config_interface((unsigned char) (sn2871_CON5),
				       (val),
				       (unsigned char) (CON5_ITERM_MASK), (unsigned char) (CON5_ITERM_SHIFT)
	    );

}



/* CON6---------------------------------------------------- */

void sn2871_set_vreg(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_2XTMR_EN_MASK),
				       (unsigned char) (CON6_2XTMR_EN_SHIFT)
	    );
}

void sn2871_set_batlowv(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BATLOWV_MASK),
				       (unsigned char) (CON6_BATLOWV_SHIFT)
	    );
}

void sn2871_set_vrechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_VRECHG_MASK),
				       (unsigned char) (CON6_VRECHG_SHIFT)
	    );
}

/* CON7---------------------------------------------------- */


void sn2871_en_term_chg(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_EN_TERM_CHG_MASK),
				       (unsigned char) (CON7_EN_TERM_CHG_SHIFT)
	    );
}

void sn2871_en_state_dis(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_STAT_DIS_MASK),
				       (unsigned char) (CON7_STAT_DIS_SHIFT)
	    );
}

void sn2871_set_wd_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_WTG_TIM_SET_MASK),
				       (unsigned char) (CON7_WTG_TIM_SET_SHIFT)
	    );
}

void sn2871_en_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_EN_TIMER_MASK),
				       (unsigned char) (CON7_EN_TIMER_SHIFT)
	    );
}

void sn2871_set_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_SET_CHG_TIM_MASK),
				       (unsigned char) (CON7_SET_CHG_TIM_SHIFT)
	    );
}

/* CON8--------------------------------------------------- */
void sn2871_set_thermal_regulation(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON8),
				       (unsigned char) (val),
				       (unsigned char) (CON8_TREG_MASK), (unsigned char) (CON8_TREG_SHIFT)
	    );
}

void sn2871_set_VBAT_clamp(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON8),
				       (unsigned char) (val),
				       (unsigned char) (CON8_VCLAMP_MASK),
				       (unsigned char) (CON8_VCLAMP_SHIFT)
	    );
}

void sn2871_set_VBAT_IR_compensation(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CON8),
				       (unsigned char) (val),
				       (unsigned char) (CON8_BAT_COMP_MASK),
				       (unsigned char) (CON8_BAT_COMP_SHIFT)
	    );
}

/* CON9---------------------------------------------------- */
void sn2871_pumpx_up(unsigned int val)
{
	unsigned int ret = 0;

	sn2871_en_pumpx(1);
	if (val == 1) {
		ret = sn2871_config_interface((unsigned char) (sn2871_CON9),
					       (unsigned char) (1),
					       (unsigned char) (CON9_PUMPX_UP),
					       (unsigned char) (CON9_PUMPX_UP_SHIFT)
		    );
	} else {
		ret = sn2871_config_interface((unsigned char) (sn2871_CON9),
					       (unsigned char) (1),
					       (unsigned char) (CON9_PUMPX_DN),
					       (unsigned char) (CON9_PUMPX_DN_SHIFT)
		    );
	}
/* Input current limit = 800 mA, changes after port detection*/
	sn2871_set_iinlim(0x14);
/* CC mode current = 2048 mA*/
	sn2871_set_ichg(0x20);
	msleep(3000);
}

/* CONA---------------------------------------------------- */
void sn2871_set_boost_ilim(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CONA),
				       (unsigned char) (val),
				       (unsigned char) (CONA_BOOST_ILIM_MASK),
				       (unsigned char) (CONA_BOOST_ILIM_SHIFT)
	    );
}

void sn2871_set_boost_vlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_CONA),
				       (unsigned char) (val),
				       (unsigned char) (CONA_BOOST_VLIM_MASK),
				       (unsigned char) (CONA_BOOST_VLIM_SHIFT)
	    );
}

/* CONB---------------------------------------------------- */


unsigned int sn2871_get_vbus_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONB),
				     (&val),
				     (unsigned char) (CONB_VBUS_STAT_MASK),
				     (unsigned char) (CONB_VBUS_STAT_SHIFT)
	    );
	return val;
}


unsigned int sn2871_get_chrg_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONB),
				     (&val),
				     (unsigned char) (CONB_CHRG_STAT_MASK),
				     (unsigned char) (CONB_CHRG_STAT_SHIFT)
	    );
	return val;
}

unsigned int sn2871_get_pg_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONB),
				     (&val),
				     (unsigned char) (CONB_PG_STAT_MASK),
				     (unsigned char) (CONB_PG_STAT_SHIFT)
	    );
	return val;
}



unsigned int sn2871_get_sdp_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONB),
				     (&val),
				     (unsigned char) (CONB_SDP_STAT_MASK),
				     (unsigned char) (CONB_SDP_STAT_SHIFT)
	    );
	return val;
}

unsigned int sn2871_get_vsys_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONB),
				     (&val),
				     (unsigned char) (CONB_VSYS_STAT_MASK),
				     (unsigned char) (CONB_VSYS_STAT_SHIFT)
	    );
	return val;
}

/* CON0C---------------------------------------------------- */
unsigned int sn2871_get_wdt_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONC),
				     (&val),
				     (unsigned char) (CONB_WATG_STAT_MASK),
				     (unsigned char) (CONB_WATG_STAT_SHIFT)
	    );
	return val;
}

unsigned int sn2871_get_boost_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONC),
				     (&val),
				     (unsigned char) (CONB_BOOST_STAT_MASK),
				     (unsigned char) (CONB_BOOST_STAT_SHIFT)
	    );
	return val;
}

unsigned int sn2871_get_chrg_fault_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONC),
				     (&val),
				     (unsigned char) (CONC_CHRG_FAULT_MASK),
				     (unsigned char) (CONC_CHRG_FAULT_SHIFT)
	    );
	return val;
}

unsigned int sn2871_get_bat_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONC),
				     (&val),
				     (unsigned char) (CONB_BAT_STAT_MASK),
				     (unsigned char) (CONB_BAT_STAT_SHIFT)
	    );
	return val;
}


/* COND */
void sn2871_set_FORCE_VINDPM(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_COND),
				       (unsigned char) (val),
				       (unsigned char) (COND_FORCE_VINDPM_MASK),
				       (unsigned char) (COND_FORCE_VINDPM_SHIFT)
	    );
}

void sn2871_set_VINDPM(unsigned int val)
{
	unsigned int ret = 0;

	ret = sn2871_config_interface((unsigned char) (sn2871_COND),
				       (unsigned char) (val),
				       (unsigned char) (COND_VINDPM_MASK),
				       (unsigned char) (COND_VINDPM_SHIFT)
	    );
}

/* CONDE */
unsigned int sn2871_get_vbat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CONE),
				     (&val),
				     (unsigned char) (CONE_VBAT_MASK), (unsigned char) (CONE_VBAT_SHIFT)
	    );
	return val;
}

/* CON11 */
unsigned int sn2871_get_vbus(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON11),
				     (&val),
				     (unsigned char) (CON11_VBUS_MASK), (unsigned char) (CON11_VBUS_SHIFT)
	    );
	return val;
}

/* CON12 */
unsigned int sn2871_get_ichg(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON12),
				     (&val),
				     (unsigned char) (CONB_ICHG_STAT_MASK),
				     (unsigned char) (CONB_ICHG_STAT_SHIFT)
	    );
	return val;
}



/* CON13 /// */


unsigned int sn2871_get_idpm_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON13),
				     (&val),
				     (unsigned char) (CON13_IDPM_STAT_MASK),
				     (unsigned char) (CON13_IDPM_STAT_SHIFT)
	    );
	return val;
}

unsigned int sn2871_get_vdpm_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON13),
				     (&val),
				     (unsigned char) (CON13_VDPM_STAT_MASK),
				     (unsigned char) (CON13_VDPM_STAT_SHIFT)
	    );
	return val;
}


unsigned int sn2871_get_device_revision(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface((unsigned char) (sn2871_CON14),
				     (&val),
				     (unsigned char) (CON14_DEV_REV_MASK),
				     (unsigned char) (CON14_DEV_REV_SHIFT)
	    );
	battery_xlog_printk(BAT_LOG_CRTI, "%s 0x%x", __func__, val);
	return val;
}


/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void sn2871_hw_component_detect(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sn2871_read_interface(0x03, &val, 0xFF, 0x0);

	if (val == 0)
		g_sn2871_hw_exist = 0;
	else
		g_sn2871_hw_exist = 1;

	pr_debug("[sn2871_hw_component_detect] exist=%d, Reg[0x03]=0x%x\n",
		 g_sn2871_hw_exist, val);
}

int is_sn2871_exist(void)
{
	pr_debug("[is_sn2871_exist] g_sn2871_hw_exist=%d\n", g_sn2871_hw_exist);

	return g_sn2871_hw_exist;
}

void sn2871_dump_register(void)
{
	unsigned char i = 0;
	unsigned char ichg = 0;
	unsigned char ichg_reg = 0;
	unsigned char iinlim = 0;
	unsigned char vbat = 0;
	unsigned char chrg_state = 0;
	unsigned char chr_en = 0;
	unsigned char vbus = 0;
	unsigned char vdpm = 0;
	unsigned char fault = 0;

	/*sn2871_ADC_start(1);*/
	for (i = 0; i < sn2871_REG_NUM; i++) {
		sn2871_read_byte(i, &sn2871_reg[i]);
		battery_xlog_printk(BAT_LOG_CRTI, "[sn2871 reg@][0x%x]=0x%x ", i, sn2871_reg[i]);
	}
	sn2871_ADC_start(1);
	iinlim = sn2871_get_iinlim();
	chrg_state = sn2871_get_chrg_state();
	chr_en = sn2871_get_chg_en();
	ichg_reg = sn2871_get_reg_ichg();
	ichg = sn2871_get_ichg();
	vbat = sn2871_get_vbat();
	vbus = sn2871_get_vbus();
	vdpm = sn2871_get_vdpm_state();
	fault = sn2871_get_chrg_fault_state();
	battery_xlog_printk(BAT_LOG_CRTI,
	"[PE+]Ibat=%d, Ilim=%d, Vbus=%d, err=%d, Ichg=%d, Vbat=%d, ChrStat=%d, CHGEN=%d, VDPM=%d\n",
	ichg_reg * 64, iinlim * 50 + 100, vbus * 100 + 2600, fault,
		    ichg * 50, vbat * 20 + 2304, chrg_state, chr_en, vdpm);

}

void sn2871_hw_init(void)
{
	/*battery_log(BAT_LOG_CRTI, "[sn2871_hw_init] After HW init\n");*/
	sn2871_dump_register();
}

kal_bool sn2871_is_found = KAL_FALSE;
static int sn2871_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	battery_xlog_printk(BAT_LOG_CRTI, "[sn2871_driver_probe]\n");

	new_client = client;
	new_client->addr = 0x6B;

	if (sn2871_get_device_revision() == 0x2) {
		battery_xlog_printk(BAT_LOG_CRTI, "find SN2871 device and register charger control. \n");
		sn2871_dump_register();
		chargin_hw_init_done = KAL_TRUE;
		sn2871_is_found = KAL_TRUE;
		return 0;
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "no SN2871 device is found.\n");
		return -1;
	}
	/* --------------------- */
	//sn2871_hw_component_detect();
	//sn2871_dump_register();
	/* sn2871_hw_init(); //move to charging_hw_xxx.c */
	//return 0;
}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
unsigned char g_reg_value_sn2871 = 0;
static ssize_t show_sn2871_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	battery_xlog_printk(BAT_LOG_CRTI, "[show_sn2871_access] 0x%x\n", g_reg_value_sn2871);
	return sprintf(buf, "%u\n", g_reg_value_sn2871);
}

static ssize_t store_sn2871_access(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	/*char *pvalue = NULL;*/
	unsigned int reg_value = 0;
	unsigned long int reg_address = 0;
	int rv;

	battery_xlog_printk(BAT_LOG_CRTI, "[store_sn2871_access]\n");

	if (buf != NULL && size != 0) {
		battery_xlog_printk(BAT_LOG_CRTI, "[store_sn2871_access] buf is %s and size is %zu\n", buf,
			    size);
		/*reg_address = simple_strtoul(buf, &pvalue, 16);*/
		rv = kstrtoul(buf, 0, &reg_address);
			if (rv != 0)
				return -EINVAL;
		/*ret = kstrtoul(buf, 16, reg_address); *//* This must be a null terminated string */
		if (size > 3) {
			/*NEED to check kstr*/
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16);*/
			/*ret = kstrtoul(buf + 3, 16, reg_value); */
			battery_xlog_printk(BAT_LOG_CRTI,
				    "[store_sn2871_access] write sn2871 reg 0x%x with value 0x%x !\n",
				    (unsigned int) reg_address, reg_value);
			ret = sn2871_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = sn2871_read_interface(reg_address, &g_reg_value_sn2871, 0xFF, 0x0);
			battery_xlog_printk(BAT_LOG_CRTI,
				    "[store_sn2871_access] read sn2871 reg 0x%x with value 0x%x !\n",
				    (unsigned int) reg_address, g_reg_value_sn2871);
			battery_xlog_printk(BAT_LOG_CRTI,
				    "[store_sn2871_access] Please use \"cat sn2871_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(sn2871_access, 0664, show_sn2871_access, store_sn2871_access);	/* 664 */

static int sn2871_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	battery_xlog_printk(BAT_LOG_CRTI, "******** sn2871_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_sn2871_access);

	return 0;
}

struct platform_device sn2871_user_space_device = {
	.name = "sn2871-user",
	.id = -1,
};

static struct platform_driver sn2871_user_space_driver = {
	.probe = sn2871_user_space_probe,
	.driver = {
		   .name = "sn2871-user",
		   },
};

#ifdef CONFIG_OF
static const struct of_device_id sn2871_of_match[] = {
	{.compatible = "mediatek,sw_charger"},
	{},
};
#else
static struct i2c_board_info i2c_sn2871 __initdata = {
	I2C_BOARD_INFO("sn2871", (0x5b))
};
#endif

static struct i2c_driver sn2871_driver = {
	.driver = {
		   .name = "sn2871",
#ifdef CONFIG_OF
		   .of_match_table = sn2871_of_match,
#endif
		   },
	.probe = sn2871_driver_probe,
	.id_table = sn2871_i2c_id,
};

static int __init sn2871_init(void)
{
	int ret = 0;

	/* i2c registeration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	battery_xlog_printk(BAT_LOG_CRTI, "[sn2871_init] init start with i2c DTS");
#else
	battery_xlog_printk(BAT_LOG_CRTI, "[sn2871_init] init start. ch=%d\n", sn2871_BUSNUM);
	i2c_register_board_info(sn2871_BUSNUM, &i2c_sn2871, 1);
#endif
	if (i2c_add_driver(&sn2871_driver) != 0) {
		battery_xlog_printk(BAT_LOG_CRTI,
			    "[sn2871_init] failed to register sn2871 i2c driver.\n");
	} else {
		battery_xlog_printk(BAT_LOG_CRTI,
			    "[sn2871_init] Success to register sn2871 i2c driver.\n");
	}

	/* sn2871 user space access interface */
	ret = platform_device_register(&sn2871_user_space_device);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI, "****[sn2871_init] Unable to device register(%d)\n",
			    ret);
		return ret;
	}
	ret = platform_driver_register(&sn2871_user_space_driver);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI, "****[sn2871_init] Unable to register driver (%d)\n",
			    ret);
		return ret;
	}

	return 0;
}

static void __exit sn2871_exit(void)
{
	i2c_del_driver(&sn2871_driver);
}
module_init(sn2871_init);
module_exit(sn2871_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C sn2871 Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
