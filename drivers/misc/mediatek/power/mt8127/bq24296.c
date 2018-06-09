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

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "bq24296.h"

#ifndef MTK_BATTERY_NO_HAL
#include "cust_charging.h"
#include <mach/charging.h>
#endif

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define bq24296_SLAVE_ADDR_WRITE   0xD6
#define bq24296_SLAVE_ADDR_READ    0xD7

static struct i2c_client *new_client;
static const struct i2c_device_id bq24296_i2c_id[] = {{"bq24296", 0}, {} };
kal_bool chargin_hw_init_done = KAL_FALSE;
static int bq24296_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static void bq24296_shutdown(struct i2c_client *client)
{

	pr_info("[bq24296_shutdown] driver shutdown\n");
	bq24296_set_chg_config(0x0);
}
static struct i2c_driver bq24296_driver = {
	.driver = {
		.name	= "bq24296",
	},
	.probe	   = bq24296_driver_probe,
	.id_table	= bq24296_i2c_id,
	.shutdown	= bq24296_shutdown,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
#define bq24296_REG_NUM 11
kal_uint8 bq24296_reg[bq24296_REG_NUM] = {0};

static DEFINE_MUTEX(bq24296_i2c_access);
/**********************************************************
  *
  *   [I2C Function For Read/Write bq24296]
  *
  *********************************************************/
int bq24296_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
	char	 cmd_buf[1] = {0x00};
	char	 readData = 0;
	int	  ret = 0;

	mutex_lock(&bq24296_i2c_access);

	/* new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG; */
	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
	if (ret < 0) {
		/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
		new_client->ext_flag = 0;

		mutex_unlock(&bq24296_i2c_access);
		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
	new_client->ext_flag = 0;

	mutex_unlock(&bq24296_i2c_access);
	return 1;
}

int bq24296_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
	char	write_data[2] = {0};
	int	 ret = 0;

	mutex_lock(&bq24296_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_DIRECTION_FLAG;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		new_client->ext_flag = 0;
		mutex_unlock(&bq24296_i2c_access);
		return 0;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&bq24296_i2c_access);
	return 1;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
kal_uint32 bq24296_read_interface(kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
	kal_uint8 bq24296_reg = 0;
	int ret = 0;

	ret = bq24296_read_byte(RegNum, &bq24296_reg);

	bq24296_reg &= (MASK << SHIFT);
	*val = (bq24296_reg >> SHIFT);

	return ret;
}

kal_uint32 bq24296_config_interface(kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
	kal_uint8 bq24296_reg = 0;
	int ret = 0;

	ret = bq24296_read_byte(RegNum, &bq24296_reg);

	bq24296_reg &= ~(MASK << SHIFT);
	bq24296_reg |= (val << SHIFT);

	ret = bq24296_write_byte(RegNum, bq24296_reg);

	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */

void bq24296_set_en_hiz(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON0),
									(kal_uint8)(val),
									(kal_uint8)(CON0_EN_HIZ_MASK),
									(kal_uint8)(CON0_EN_HIZ_SHIFT)
									);
}

void bq24296_set_vindpm(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON0),
									(kal_uint8)(val),
									(kal_uint8)(CON0_VINDPM_MASK),
									(kal_uint8)(CON0_VINDPM_SHIFT)
									);
}

void bq24296_set_iinlim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON0),
									(kal_uint8)(val),
									(kal_uint8)(CON0_IINLIM_MASK),
									(kal_uint8)(CON0_IINLIM_SHIFT)
									);
}

/* CON1---------------------------------------------------- */

void bq24296_set_reg_rst(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON1),
									(kal_uint8)(val),
									(kal_uint8)(CON1_REG_RST_MASK),
									(kal_uint8)(CON1_REG_RST_SHIFT)
									);
}

void bq24296_set_wdt_rst(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON1),
									(kal_uint8)(val),
									(kal_uint8)(CON1_WDT_RST_MASK),
									(kal_uint8)(CON1_WDT_RST_SHIFT)
									);
}

void bq24296_set_otg_config(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON1),
									(kal_uint8)(val),
									(kal_uint8)(CON1_OTG_CONFIG_MASK),
									(kal_uint8)(CON1_OTG_CONFIG_SHIFT)
									);
}

void bq24296_set_chg_config(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON1),
									(kal_uint8)(val),
									(kal_uint8)(CON1_CHG_CONFIG_MASK),
									(kal_uint8)(CON1_CHG_CONFIG_SHIFT)
									);
}

void bq24296_set_sys_min(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON1),
									(kal_uint8)(val),
									(kal_uint8)(CON1_SYS_MIN_MASK),
									(kal_uint8)(CON1_SYS_MIN_SHIFT)
									);
}

void bq24296_set_boost_lim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON1),
									(kal_uint8)(val),
									(kal_uint8)(CON1_BOOST_LIM_MASK),
									(kal_uint8)(CON1_BOOST_LIM_SHIFT)
									);
}

/* CON2---------------------------------------------------- */

void bq24296_set_ichg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON2),
									(kal_uint8)(val),
									(kal_uint8)(CON2_ICHG_MASK),
									(kal_uint8)(CON2_ICHG_SHIFT)
									);
}

void bq24296_set_bcold(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON2),
									(kal_uint8)(val),
									(kal_uint8)(CON2_BCOLD_MASK),
									(kal_uint8)(CON2_BCOLD_SHIFT)
									);
}
void bq24296_set_force_20pct(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON2),
									(kal_uint8)(val),
									(kal_uint8)(CON2_FORCE_20PCT_MASK),
									(kal_uint8)(CON2_FORCE_20PCT_SHIFT)
									);
}

/* CON3---------------------------------------------------- */

void bq24296_set_iprechg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON3),
									(kal_uint8)(val),
									(kal_uint8)(CON3_IPRECHG_MASK),
									(kal_uint8)(CON3_IPRECHG_SHIFT)
									);
}

void bq24296_set_iterm(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON3),
									(kal_uint8)(val),
									(kal_uint8)(CON3_ITERM_MASK),
									(kal_uint8)(CON3_ITERM_SHIFT)
									);
}

/* CON4---------------------------------------------------- */

void bq24296_set_vreg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON4),
									(kal_uint8)(val),
									(kal_uint8)(CON4_VREG_MASK),
									(kal_uint8)(CON4_VREG_SHIFT)
									);
}

void bq24296_set_batlowv(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON4),
									(kal_uint8)(val),
									(kal_uint8)(CON4_BATLOWV_MASK),
									(kal_uint8)(CON4_BATLOWV_SHIFT)
									);
}

void bq24296_set_vrechg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON4),
									(kal_uint8)(val),
									(kal_uint8)(CON4_VRECHG_MASK),
									(kal_uint8)(CON4_VRECHG_SHIFT)
									);
}

/* CON5---------------------------------------------------- */

void bq24296_set_en_term(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON5),
									(kal_uint8)(val),
									(kal_uint8)(CON5_EN_TERM_MASK),
									(kal_uint8)(CON5_EN_TERM_SHIFT)
									);
}

void bq24296_set_watchdog(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON5),
									(kal_uint8)(val),
									(kal_uint8)(CON5_WATCHDOG_MASK),
									(kal_uint8)(CON5_WATCHDOG_SHIFT)
									);
}

void bq24296_set_en_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON5),
									(kal_uint8)(val),
									(kal_uint8)(CON5_EN_TIMER_MASK),
									(kal_uint8)(CON5_EN_TIMER_SHIFT)
									);
}

void bq24296_set_chg_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON5),
									(kal_uint8)(val),
									(kal_uint8)(CON5_CHG_TIMER_MASK),
									(kal_uint8)(CON5_CHG_TIMER_SHIFT)
									);
}

/* CON6---------------------------------------------------- */

void bq24296_set_treg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON6),
									(kal_uint8)(val),
									(kal_uint8)(CON6_TREG_MASK),
									(kal_uint8)(CON6_TREG_SHIFT)
									);
}

void bq24296_set_boostv(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON6),
									(kal_uint8)(val),
									(kal_uint8)(CON6_BOOSTV_MASK),
									(kal_uint8)(CON6_BOOSTV_SHIFT)
									);
}

void bq24296_set_bhot(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON6),
									(kal_uint8)(val),
									(kal_uint8)(CON6_BHOT_MASK),
									(kal_uint8)(CON6_BHOT_SHIFT)
									);
}

/* CON7---------------------------------------------------- */

void bq24296_set_tmr2x_en(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON7),
									(kal_uint8)(val),
									(kal_uint8)(CON7_TMR2X_EN_MASK),
									(kal_uint8)(CON7_TMR2X_EN_SHIFT)
									);
}

void bq24296_set_batfet_disable(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON7),
									(kal_uint8)(val),
									(kal_uint8)(CON7_BATFET_Disable_MASK),
									(kal_uint8)(CON7_BATFET_Disable_SHIFT)
									);
}

void bq24296_set_int_mask(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24296_config_interface((kal_uint8)(bq24296_CON7),
									(kal_uint8)(val),
									(kal_uint8)(CON7_INT_MASK_MASK),
									(kal_uint8)(CON7_INT_MASK_SHIFT)
									);
}

/* CON8---------------------------------------------------- */

kal_uint32 bq24296_get_system_status(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24296_read_interface((kal_uint8)(bq24296_CON8),
									(&val),
									(kal_uint8)(0xFF),
									(kal_uint8)(0x0)
									);
	return val;
}

kal_uint32 bq24296_get_vbus_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24296_read_interface((kal_uint8)(bq24296_CON8),
									(&val),
									(kal_uint8)(CON8_VBUS_STAT_MASK),
									(kal_uint8)(CON8_VBUS_STAT_SHIFT)
									);
	return val;
}

kal_uint32 bq24296_get_chrg_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24296_read_interface((kal_uint8)(bq24296_CON8),
									(&val),
									(kal_uint8)(CON8_CHRG_STAT_MASK),
									(kal_uint8)(CON8_CHRG_STAT_SHIFT)
									);
	return val;
}

kal_uint32 bq24296_get_vsys_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24296_read_interface((kal_uint8)(bq24296_CON8),
									(&val),
									(kal_uint8)(CON8_VSYS_STAT_MASK),
									(kal_uint8)(CON8_VSYS_STAT_SHIFT)
									);
	return val;
}

kal_uint32 bq24296_get_pn(void)
{
	kal_uint8 val = 0;
	kal_uint32 ret = 0;
	ret = bq24296_read_interface((kal_uint8) (bq24296_CON10),
				     (&val),
				     (kal_uint8) (CON10_PN_MASK),
				     (kal_uint8) (CON10_PN_SHIFT)
	    );

	if (ret)
		return val;
	else
		return 0;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
static unsigned char bq24296_get_reg9_fault_type(unsigned char reg9_fault)
{
	unsigned char ret = 0;

	if ((reg9_fault & (CON9_OTG_FAULT_MASK << CON9_OTG_FAULT_SHIFT)) != 0) {
		ret = BQ_OTG_FAULT;
	} else if ((reg9_fault & (CON9_CHRG_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_CHRG_INPUT_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_INPUT_FAULT;
		else if ((reg9_fault &
			  (CON9_CHRG_THERMAL_SHUTDOWN_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_THERMAL_FAULT;
		else if ((reg9_fault &
			  (CON9_CHRG_TIMER_EXPIRATION_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_TIMER_EXPIRATION_FAULT;
	} else if ((reg9_fault & (CON9_BAT_FAULT_MASK << CON9_BAT_FAULT_SHIFT)) != 0)
		ret = BQ_BAT_FAULT;
	else if ((reg9_fault & (CON9_NTC_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_NTC_COLD_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_COLD_FAULT;
		else if ((reg9_fault & (CON9_NTC_HOT_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_HOT_FAULT;
	}
	return ret;
}

void bq24296_get_fault_type(unsigned char *type)
{
	*type = bq24296_get_reg9_fault_type(bq24296_reg[bq24296_CON9]);
}
EXPORT_SYMBOL(bq24296_get_fault_type);

void bq24296_dump_register(void)
{
	int i = 0;
	for (i = 0; i < bq24296_REG_NUM; i++) {
		bq24296_read_byte(i, &bq24296_reg[i]);
	}
	battery_xlog_printk(BAT_LOG_CRTI, "[bq24296_dump_register]:0x%X;0x%X;0x%X;0x%X;0x%X;0x%X;0x%X;0x%X;0x%X;0x%X;0x%X;\n", bq24296_reg[0], bq24296_reg[1],
		bq24296_reg[2], bq24296_reg[3], bq24296_reg[4], bq24296_reg[5], bq24296_reg[6], bq24296_reg[7], bq24296_reg[8], bq24296_reg[9], bq24296_reg[10]);
}

kal_bool bq24296_is_found = KAL_FALSE;
static int bq24296_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	pr_info("[bq24296_driver_probe]\n");

	new_client = client;

	if (bq24296_get_pn() == 0x1) {
		battery_xlog_printk(BAT_LOG_CRTI, "find BQ24296 device and register charger control.\n");
		bq24296_dump_register();
		bq24296_is_found = KAL_TRUE;
		chargin_hw_init_done = KAL_TRUE;
		return 0;
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "no BQ24296 device is found.\n");
		return -1;
	}

	/* --------------------- */
	//bq24296_dump_register();
	//chargin_hw_init_done = KAL_TRUE;

	//return 0;
}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
kal_uint8 g_reg_value_bq24296 = 0;
static ssize_t show_bq24296_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[show_bq24296_access] 0x%x\n", g_reg_value_bq24296);
	return sprintf(buf, "%u\n", g_reg_value_bq24296);
}
static ssize_t store_bq24296_access(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[store_bq24296_access]\n");

	if (buf != NULL && size != 0) {
		pr_info("[store_bq24296_access] buf is %s and size is %d\n", buf, size);
		reg_address = simple_strtoul(buf, &pvalue, 16);

		if (size > 3) {
			reg_value = simple_strtoul((pvalue+1), NULL, 16);
			pr_notice("[store_bq24296_access] write bq24296 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = bq24296_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = bq24296_read_interface(reg_address, &g_reg_value_bq24296, 0xFF, 0x0);
			pr_notice("[store_bq24296_access] read bq24296 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_bq24296);
			pr_notice("[store_bq24296_access] Please use \"cat bq24296_access\" to get value\n");
		}
	}
	return size;
}
static DEVICE_ATTR(bq24296_access, 0664, show_bq24296_access, store_bq24296_access); /* 664 */

static int bq24296_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("******** bq24296_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq24296_access);

	return 0;
}

struct platform_device bq24296_user_space_device = {
	.name   = "bq24296-user",
	.id	 = -1,
};

static struct platform_driver bq24296_user_space_driver = {
	.probe	  = bq24296_user_space_probe,
	.driver	 = {
		.name = "bq24296-user",
	},
};

#ifndef BQ24296_BUSNUM
#define BQ24296_BUSNUM 1
#endif

static struct i2c_board_info i2c_bq24296 __initdata = { I2C_BOARD_INFO("bq24296", (0xd6>>1))};

static int __init bq24296_init(void)
{
	int ret = 0;

	pr_info("[bq24296_init] init start\n");

	i2c_register_board_info(BQ24296_BUSNUM, &i2c_bq24296, 1);

	if (i2c_add_driver(&bq24296_driver) != 0)
		pr_err("[bq24296_init] failed to register bq24296 i2c driver.\n");
	else
		pr_notice("[bq24296_init] Success to register bq24296 i2c driver.\n");

	/* bq24296 user space access interface */
	ret = platform_device_register(&bq24296_user_space_device);
	if (ret) {
		pr_err("****[bq24296_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&bq24296_user_space_driver);
	if (ret) {
		pr_err("****[bq24296_init] Unable to register driver (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void __exit bq24296_exit(void)
{
	i2c_del_driver(&bq24296_driver);
}
module_init(bq24296_init);
module_exit(bq24296_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24296 Driver");
MODULE_AUTHOR("YT Lee<yt.lee@mediatek.com>");
