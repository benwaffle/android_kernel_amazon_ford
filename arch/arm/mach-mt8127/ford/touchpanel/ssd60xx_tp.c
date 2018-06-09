/*
 * Copyright 2015 Solomon Systech Ltd. All rights reserved.
 *
 *  Date:  2015.8.28 testing build from Vincent
 */

#include "ssd60xx_tp.h"

static int ssd60xx_probe(struct i2c_client *client, const struct i2c_device_id *id);

static int ssd60xx_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);

static void ssd60xx_resume(struct early_suspend *h);

static void ssd60xx_suspend(struct early_suspend *h);

static int ssd60xx_remove(struct i2c_client *client);

static const struct i2c_device_id ssd60xx_tpd_id[] = { {SSD60XX_I2C_NAME, 0}, {} };
static struct i2c_board_info __initdata ssd60xx_i2c_tpd = { I2C_BOARD_INFO(SSD60XX_I2C_NAME, (SSD60XX_I2C_ADDR)) };
static unsigned short force[] = { 0, SSD60XX_I2C_ADDR << 1, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8] = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		   .name = SSD60XX_I2C_NAME,
		   },
	.probe = ssd60xx_probe,
	.remove = ssd60xx_remove,
	.id_table = ssd60xx_tpd_id,
	.detect = ssd60xx_detect,
	.address_list = (const unsigned short *)forces,
};

extern struct tpd_device *tpd;

static int tpd_flag;
struct task_struct *ssd60xx_thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);

static void ssd60xx_interrupt_handler(void);

static int ssd60xx_event_handler(void *unused);

/* ############################################################################### */
#define DRIVER_VERSION "20150828_1.1"

#define FINGERNO 5

#define ENABLE_INT	1
#define ENABLE_TIMER	2
#define DEFAULT_USED	ENABLE_INT

#define MICRO_TIMER_FREQ	50
#define MICRO_TIME_INTERUPT	(1000000000 / MICRO_TIMER_FREQ)

#define FACE_TIMER_FREQ		20
#define FACE_TIME_INTERUPT	(1000000000 / FACE_TIMER_FREQ)
/* ############################################################################### */

/*
 * ###############################################################################
 * #define CONFIG_TOUCHSCREEN_SSL_TOUCH_KEY //no virtual key
 */

#ifdef CONFIG_TOUCHSCREEN_SSL_TOUCH_KEY
#define TPD_HAVE_BUTTON

#define KEY_W_H 80, 50

#define KEY_0	40, 880
#define KEY_1	240, 880
#define KEY_2	430, 880
#define KEY_3	430, 880

#define TPD_KEY_COUNT	3
#define TPD_KEYS	{ KEY_MENU, KEY_HOMEPAGE, KEY_BACK }
#define TPD_KEYS_DIM	{ { KEY_0, KEY_W_H }, { KEY_1, KEY_W_H }, { KEY_2, KEY_W_H } }

static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
/* ############################################################################### */

/*
 * ###############################################################################
 * #define CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER //no usr jiter fillter
 */
static	int ForceTouch_threshold;//ForceTouch 20150729
static  int ForceTouch_upperbound;
static  int ForceTouch_lowerbound;
static  int ForceTouch_state = 0;
static  int ForceTouch_hysteresis = 0;
static  int Reg_C4,Reg_C5,Reg_C6,Reg_C7,Reg_C8;  // 20150730 
//static  int ForceTouch_alive = 0;

/*
Usage of FB (Edge Filter):
  - Edge X2 Offset (D15 - D08)
  - Edge X1 Offset (D7 - D0)
*/
#define FORCE_EDGE_FILTER
#ifdef FORCE_EDGE_FILTER
#define EDGE_FILTER_REG  0xFB 
static  unsigned short edge_filter_x1_offset, edge_filter_x2_offset, edge_filter_y1_offset, edge_filter_y2_offset, edge_filter_scale, edge_filter_enable;  //20150806, for edge tuning
static  unsigned short edge_filter_switch[FINGERNO] = {0};
static  int edge_filter_x[FINGERNO] = {0xFFF};
static  int edge_filter_y[FINGERNO] = {0xFFF};
#endif



/*
brand new filter aims at removing hard press ghost finger for A+

ghost finger after palm detected is removed by the palm detection hack in index filter
this aims at removing the ghost points just before palm rejection takes place

For now, it reuse the value of 0xFC as a correlation.
The threshold of this hack shall be less than 0xFC because if pressure reaches the value in 0xFC,
the points shall be removed already.

Refer to ForceTouch_threshold, now set to 3/4 of it

*/
//#define CONFIG_HARD_PRESS_HACK_A

#define CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER 

#ifdef CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER

#define PALM_STATUS_REG 0x5B

/*
Usage of E5 (index filter register):
  - enable:1/disable:0 (D15)
  - penalty enable:1/disable:0 (D14)
  - penalty pressure scale (D13 - D12)
  - 1st frame penalty enable:1/disable:0 (D11)
  - no. of fingers to turn on (D10 - D08)
  - min. frames to confirm finger (D7 - D0)
*/
#define INDEX_FILTER_REG  0xE5
static unsigned int jitter_filter_on =0;
static unsigned int jitter_filter_penalty_on =0;
static unsigned int ssd_jitter_count = 0;
static unsigned int jitter_finger_num = 0;
static unsigned int jitter_finger_num_backup = 0;
static unsigned int jitter_max_pressure = 0;
static unsigned int jitter_penalty[FINGERNO] = { 0 };
static unsigned int jitter_filter_count[FINGERNO] = { 0 };
static unsigned int  jitter_filter_penalty_pressure_scale = 0;
static unsigned int  jitter_filter_penalty_1st_frame = 0;

#endif

/*
Usage of E6 (special small finger register):
  - enable:1/disable:0 (D15)
  - M (ratio of average weight) (D14 – D0)
*/
#define SPECIAL_FINGER_SIZE_FILTER
#ifdef SPECIAL_FINGER_SIZE_FILTER
extern kal_bool upmu_is_chr_det(void);
#define FINGER_SIZE_FILTER	0xE6
#define BUTTON_REGION           560
static unsigned int finger_size_filter_on = 0;
static unsigned int finger_size_filter_ratio = 0;
static unsigned int finger_weight[FINGERNO] = {0};
static int confidence[FINGERNO] = {0};
#endif 
/* ############################################################################### */

#define DEVICE_ID_REG		0x02
#define VERSION_ID_REG		0x03
#define EVENT_STATUS		0x79
#define EVENT_MASK		0x7B
#define FINGER01_REG		0x7C
#define KEY_STATUS		0xB0
#define FINGER_COUNT_REG	0x8A

#define PRESS_MAX 100

struct ssl_ts_priv {
	struct i2c_client *client;
	struct input_dev *input;
	struct hrtimer timer;

	struct work_struct ssl_work;
	struct workqueue_struct *ssl_workqueue;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	int device_id;
	int version_id;
	int finger_count;
	int use_irq;
	int irq;

	int FingerX[FINGERNO];
	int FingerY[FINGERNO];
	int FingerP[FINGERNO];

#ifdef CONFIG_TOUCHSCREEN_SSL_TOUCH_KEY
	int keys[TPD_KEY_COUNT];
#endif
};
static int ssl_ts_suspend_flag;
static struct ssl_ts_priv *ssl_priv;

int ssd60xx_read_regs(struct i2c_client *client, uint8_t reg, char *buf, int len);

int ssd60xx_write_regs(struct i2c_client *client, uint8_t reg, char *buf, int len);

int ssd60xx_read_reg(struct i2c_client *client, uint8_t reg, int ByteNo);

int ssd60xx_write_reg(struct i2c_client *client, uint8_t Reg, unsigned char Data1, unsigned char Data2, int ByteNo);

void ssd60xx_device_reset(void);

void ssd60xx_device_patch(struct i2c_client *client);

void ssd60xx_device_init(struct i2c_client *client);

static int ssd60xx_work(void);

/* ############################################################################### */
#define CONFIG_TOUCHSCREEN_SSL_DEBUG

#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
static int debug_show;
#define SSL_DEBUG(fmt, arg ...) do { if (debug_show) { printk("ssd60xx: " fmt "\n", ## arg); } } while (0)
#else
#define SSL_DEBUG(fmt, arg ...)
#endif
/* ############################################################################### */

/* ############################################################################### */
#define MISC_TP_CHR		/* for apk debug don't remark */

#include <linux/miscdevice.h>

#ifdef MISC_TP_CHR
#define TP_CHR "tp_chr"

static ssize_t ssd_misc_read(struct file *file, char __user *buf, size_t count, loff_t *offset);

static ssize_t ssd_misc_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

#endif
/* ############################################################################### */

/* ############################################################################### */
#define TPD_PROC_DEBUG		/* for debug don't remark */

#ifdef TPD_PROC_DEBUG
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

static struct ChipSetting ssd_config_patch[5120] = { {0} };
static struct ChipSetting ssd_config_table[1024] = { {0} };

int ssd_config_patch_count = 0;
int ssd_config_table_count = 0;

static char ssd_return_str[25600] = { 0 };

static ssize_t ssdtouch_initcode_set(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t ssdtouch_initcode_get(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

static struct kobj_attribute ssdtouch_attr = {
	.attr = {
		 .name = "ssdtouch",
		 .mode = S_IRUGO,
		 },
	.show = &ssdtouch_initcode_get,
	.store = &ssdtouch_initcode_set,
};

static struct attribute *ssdtouch_attributes[] = {
	&ssdtouch_attr.attr,
	NULL
};

static struct attribute_group ssdtouch_attr_group = {
	.attrs = ssdtouch_attributes,
};
#endif
/* ############################################################################### */

/* ############################################################################### */
/*#define  SSL_AUTO_UPGRADE	//for auto upgrade don't remark */

#ifdef SSL_AUTO_UPGRADE
#define FILE_READ_TRY		50
#define ssl_initcode_file1	"/sdcard/ssd_tp.h"
#define ssl_initcode_file2	"/data/ssd_tp.h"
#endif
/* ############################################################################### */

/* =============================================================================== */
#ifdef TPD_PROC_DEBUG
void ssd_config_run_patch(void)
{
	int i;

	for (i = 0; i < ssd_config_patch_count; i++) {
		ssd60xx_write_reg(ssl_priv->client, ssd_config_patch[i].Reg, ssd_config_patch[i].Data1, ssd_config_patch[i].Data2, ssd_config_patch[i].No);
	}
	printk("%s ok\n", __func__);
}

void ssd_config_run_init(void)
{
	int i;

	for (i = 0; i < ssd_config_table_count; i++) {
		ssd60xx_write_reg(ssl_priv->client, ssd_config_table[i].Reg, ssd_config_table[i].Data1, ssd_config_table[i].Data2, ssd_config_table[i].No);
	}
	printk("%s ok\n", __func__);

	/* read patch checksum */
	mdelay(50);
	ssd60xx_write_reg(ssl_priv->client, 0x96, 0x00, 0x01, 2);
	mdelay(50);
	i = ssd60xx_read_reg(ssl_priv->client, 0x97, 2);

	printk("patch checksum : 0x%04X\n", i);
}

#define SSDTOUCH_WRITE	"write"
#define SSDTOUCH_READ	"read"
#define SSDTOUCH_READS	"reads"

#define SSDTOUCH_PATCHCLEAR	"patchclear"
#define SSDTOUCH_PATCHADDS	"patchadds"
#define SSDTOUCH_PATCHADD	"patchadd"
#define SSDTOUCH_PATCH		"patch"

#define SSDTOUCH_INITCLEAR	"initclear"
#define SSDTOUCH_INITADDS	"initadds"
#define SSDTOUCH_INITADD	"initadd"
#define SSDTOUCH_INIT		"init"

#define SSDTOUCH_DEBUGSHOW	"debugshow"
#define SSDTOUCH_RESET		"reset"
#define SSDTOUCH_JITTERCOUNT	"jittercount"

static ssize_t ssdtouch_initcode_set(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *value = NULL;
	unsigned int Reg = 0;
	unsigned int Data1 = 0;
	unsigned int Data2 = 0;
	unsigned int Count = 0;
	unsigned int ret = 0;
	int i = 0;
	char tmp_str[256] = { 0 };

	const char *ssd_receive_buf = buf;

	if ((strstr(ssd_receive_buf, SSDTOUCH_WRITE)) != NULL) {	/* write register */
		value = strstr(ssd_receive_buf, SSDTOUCH_WRITE);
		value += strlen(SSDTOUCH_WRITE);
		if (strlen(value) < 14) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_WRITE);
			return -EFAULT;
		}
		value += strlen(" ");
		sscanf(value, "0x%x 0x%x 0x%x", &Reg, &Data1, &Data2);

		ssd60xx_write_reg(ssl_priv->client, Reg, Data1, Data2, 2);

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "write [0x%02X]=0x%02X%02X\n", Reg, Data1, Data2);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_READS)) != NULL) {	/* reads register */
		value = strstr(ssd_receive_buf, SSDTOUCH_READS);
		value += strlen(SSDTOUCH_READS);
		if (strlen(value) < 4) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_READS);
			return -EFAULT;
		}

		value += strlen(" ");
		sscanf(value, "0x%x", &Reg);

		value += 5;
		sscanf(value, "0x%x", &Count);

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		for (i = 0; i < Count; i++) {
			ret = ssd60xx_read_reg(ssl_priv->client, Reg + i, 2);
			memset(tmp_str, 0, sizeof(tmp_str));
			sprintf(tmp_str, " 0x%04X", ret);
			strcat(ssd_return_str, tmp_str);
		}
		strcat(ssd_return_str, "\n");
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_READ)) != NULL) {	/* read register */
		value = strstr(ssd_receive_buf, SSDTOUCH_READ);
		value += strlen(SSDTOUCH_READ);
		if (strlen(value) < 4) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_READ);
			return -EFAULT;
		}

		value += strlen(" ");
		sscanf(value, "0x%x", &Reg);

		ret = ssd60xx_read_reg(ssl_priv->client, Reg, 2);

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "0x%04X\n", ret);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_PATCHCLEAR)) != NULL) {	/* patch clear */
		value = strstr(ssd_receive_buf, SSDTOUCH_PATCHCLEAR);
		ssd_config_patch_count = 0;

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s ok!\n", SSDTOUCH_PATCHCLEAR);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_PATCHADDS)) != NULL) {	/* patch add */
		value = strstr(ssd_receive_buf, SSDTOUCH_PATCHADDS);
		value += strlen(SSDTOUCH_PATCHADDS);
		if (strlen(value) < 14) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_PATCHADDS);
			return -EFAULT;
		}
		value += strlen(" ");
		sscanf(value, "0x%x", &Count);

		value += 5;
		if (strlen(value) != Count * 15) {
			sprintf(ssd_return_str, "%s Count=%d strlen(value)=%d count error!\n", SSDTOUCH_PATCHADDS, Count, strlen(value));
			return -EFAULT;
		}

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		for (i = 0; i < Count; i++) {
			sscanf(value, " 0x%x 0x%x 0x%x", &Reg, &Data1, &Data2);
			value += 15;

			ssd_config_patch[ssd_config_patch_count].No = 2;
			ssd_config_patch[ssd_config_patch_count].Reg = Reg;
			ssd_config_patch[ssd_config_patch_count].Data1 = Data1;
			ssd_config_patch[ssd_config_patch_count].Data2 = Data2;
			ssd_config_patch_count++;

			memset(tmp_str, 0, sizeof(tmp_str));
			sprintf(tmp_str, "%s Patch[%04d]= {%d,0x%02X,0x%02X,0x%02X} ok!\n", SSDTOUCH_PATCHADDS, ssd_config_patch_count - 1, 2, Reg, Data1, Data2);
			strcat(ssd_return_str, tmp_str);
		}
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_PATCHADD)) != NULL) {	/* patch add */
		value = strstr(ssd_receive_buf, SSDTOUCH_PATCHADD);
		value += strlen(SSDTOUCH_PATCHADD);
		if (strlen(value) < 14) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_PATCHADD);
			return -EFAULT;
		}
		value += strlen(" ");

		sscanf(value, "0x%x 0x%x 0x%x", &Reg, &Data1, &Data2);

		ssd_config_patch[ssd_config_patch_count].No = 2;
		ssd_config_patch[ssd_config_patch_count].Reg = Reg;
		ssd_config_patch[ssd_config_patch_count].Data1 = Data1;
		ssd_config_patch[ssd_config_patch_count].Data2 = Data2;
		ssd_config_patch_count++;

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s Patch[%04d]= {%d,0x%02X,0x%02X,0x%02X} ok!\n", SSDTOUCH_PATCHADD, ssd_config_patch_count - 1, 2, Reg, Data1, Data2);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_PATCH)) != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_PATCH);
		ssd_config_run_patch();

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s ok!\n", SSDTOUCH_PATCH);

	} else if ((strstr(ssd_receive_buf, SSDTOUCH_INITCLEAR)) != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_INITCLEAR);
		ssd_config_table_count = 0;

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s ok!\n", SSDTOUCH_INITCLEAR);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_INITADDS)) != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_INITADDS);
		value += strlen(SSDTOUCH_INITADDS);
		if (strlen(value) < 14) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_INITADDS);
			return -EFAULT;
		}
		value += strlen(" ");
		sscanf(value, "0x%x", &Count);

		value += 5;
		if (strlen(value) != Count * 15) {
			sprintf(ssd_return_str, "%s Count=%d strlen(value)=%d count error!\n", SSDTOUCH_PATCHADDS, Count, strlen(value));
			return -EFAULT;
		}

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		for (i = 0; i < Count; i++) {
			sscanf(value, " 0x%x 0x%x 0x%x", &Reg, &Data1, &Data2);
			value += 15;

			ssd_config_table[ssd_config_table_count].No = 2;
			ssd_config_table[ssd_config_table_count].Reg = Reg;
			ssd_config_table[ssd_config_table_count].Data1 = Data1;
			ssd_config_table[ssd_config_table_count].Data2 = Data2;
			ssd_config_table_count++;

			memset(tmp_str, 0, sizeof(tmp_str));
			sprintf(tmp_str, "%s Init[%04d]= {%d,0x%02X,0x%02X,0x%02X} ok!\n", SSDTOUCH_INITADDS, ssd_config_table_count - 1, 2, Reg, Data1, Data2);
			strcat(ssd_return_str, tmp_str);
		}
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_INITADD)) != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_INITADD);
		value += strlen(SSDTOUCH_INITADD);
		if (strlen(value) < 14) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_INITADD);
			return -EFAULT;
		}
		value += strlen(" ");
		sscanf(value, "0x%x 0x%x 0x%x", &Reg, &Data1, &Data2);

		ssd_config_table[ssd_config_table_count].No = 2;
		ssd_config_table[ssd_config_table_count].Reg = Reg;
		ssd_config_table[ssd_config_table_count].Data1 = Data1;
		ssd_config_table[ssd_config_table_count].Data2 = Data2;
		ssd_config_table_count++;

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s Init[%04d]= {%d,0x%02X,0x%02X,0x%02X} ok!\n", SSDTOUCH_INITADD, ssd_config_table_count - 1, 2, Reg, Data1, Data2);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_INIT)) != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_INIT);
		ssd_config_run_init();

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s ok!\n", SSDTOUCH_INIT);
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_DEBUGSHOW)) != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_DEBUGSHOW);
#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
		value += strlen(SSDTOUCH_DEBUGSHOW);
		if (strlen(value) < 4) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_DEBUGSHOW);
			return -EFAULT;
		}
		value += strlen(" ");
		sscanf(value, "0x%x", &Reg);
		debug_show = Reg;

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s debug_show=%d ok!\n", SSDTOUCH_DEBUGSHOW, debug_show);
#endif
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_JITTERCOUNT))
		   != NULL) {
		value = strstr(ssd_receive_buf, SSDTOUCH_JITTERCOUNT);
#ifdef CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER
		value += strlen(SSDTOUCH_JITTERCOUNT);
		if (strlen(value) < 4) {
			sprintf(ssd_return_str, "%s error!\n", SSDTOUCH_JITTERCOUNT);
			return -EFAULT;
		}
		value += strlen(" ");
		sscanf(value, "0x%x", &Reg);
		ssd_jitter_count = Reg;

		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s SSDTOUCH_JITTERCOUNT=%d ok!\n", SSDTOUCH_JITTERCOUNT, ssd_jitter_count);
#endif
	} else if ((strstr(ssd_receive_buf, SSDTOUCH_RESET)) != NULL) {	/* reset */
		value = strstr(ssd_receive_buf, SSDTOUCH_RESET);
		ssd60xx_device_reset();
		msleep(20);
		ssd60xx_device_patch(ssl_priv->client);
		ssd60xx_device_init(ssl_priv->client);
		memset(ssd_return_str, 0, sizeof(ssd_return_str));
		sprintf(ssd_return_str, "%s ok!\n", SSDTOUCH_RESET);
	}
	return 1;
}

static ssize_t ssdtouch_initcode_get(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ssd_return_str);
}

#endif

/* ******************************************************************************* */

/* =============================================================================== */
#ifdef MISC_TP_CHR
static const struct file_operations ssd_misc_fops = {
	/* .owner        = THIS_MODULE, */
	.read = ssd_misc_read,
	.write = ssd_misc_write,
};

static struct miscdevice ssd_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TP_CHR,
	.fops = &ssd_misc_fops,
};

static ssize_t ssd_misc_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	char *kbuf;
	uint8_t reg;
	int ByteNo;
	int readValue;
	int i;

	kbuf = kmalloc(count, GFP_KERNEL);

	if (copy_from_user(kbuf, buf, 1)) {
		printk("no enough memory!\n");
		return -1;
	}

	reg = (uint8_t) kbuf[0];
	ByteNo = count;

	if (ByteNo <= 4) {
		readValue = ssd60xx_read_reg(ssl_priv->client, reg, ByteNo);
		for (i = 0; i < ByteNo; i++) {
			kbuf[i] = (readValue >> (8 * i)) & 0xff;
		}
	} else {
		ssd60xx_read_regs(ssl_priv->client, reg, kbuf, ByteNo);
	}

	if (copy_to_user(buf, kbuf, count)) {
		printk("no enough memory!\n");
		kfree(kbuf);
		return -1;
	}

	kfree(kbuf);

	return count;
}

static ssize_t ssd_misc_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	char *kbuf;

	kbuf = kmalloc(count, GFP_KERNEL);

	if (copy_from_user(kbuf, buf, count)) {
		printk("no enough memory!\n");
		return -1;
	}

	ssd60xx_write_reg(ssl_priv->client, kbuf[1], kbuf[2], kbuf[3], kbuf[0]);

	kfree(kbuf);

	return count;
}

#endif
/* =============================================================================== */

/* ############################################################################### */
#ifdef SSL_AUTO_UPGRADE

int ssd_get_line(char *line, int *reg, int *data1, int *data2)
{
	char *buff = NULL, *tmpbuff = NULL;
	int ret = -1;
	buff = line;

	/* printf("line:%s \n", line); */
	while (buff[0] == ' ') {
		buff++;
	}

	while (buff[0] == '\t') {
		buff++;
	}

	if (buff[0] == '/') {
		return -1;
	}

	if (strlen(buff) > 0) {
		if ((strstr(buff, "0x")) != NULL) {
			tmpbuff = strstr(buff, "0x");
			sscanf(tmpbuff, "0x%x", reg);
			buff = tmpbuff + 3;
			ret = -1;
		}
		if ((strstr(buff, "0x")) != NULL) {
			tmpbuff = strstr(buff, "0x");
			sscanf(tmpbuff, "0x%x", data1);
			buff = tmpbuff + 3;
			ret = -1;
		}
		if ((strstr(buff, "0x")) != NULL) {
			tmpbuff = strstr(buff, "0x");
			sscanf(tmpbuff, "0x%x", data2);
			buff = tmpbuff + 3;
			ret = 0;
		}
	}
	return ret;
}

void ssd_do_fw_upgrade(char *buffer, int size)
{
	char *buf;
	int i, j;
	char line_buffer[128];
	int reg, data1, data2;

	buf = buffer;
	memset(line_buffer, 0, sizeof(line_buffer));
	j = 0;
	/* printf("buffer:%s \n", buffer); */
	for (i = 0; i < size; i++) {
		if (buf[i] == '\r') {
			continue;
		}
		if (j < sizeof(line_buffer)) {
			line_buffer[j] = buf[i];
			j++;
		}
		if (buf[i] == '\n') {
			if (ssd_get_line(line_buffer, &reg, &data1, &data2)
			    == 0) {
				/* printf("0x%02X 0x%02X 0x%02X \n", reg, data1, data2); */
				ssd60xx_write_reg(ssl_priv->client, reg, data1, data2, 2);
			}
			memset(line_buffer, 0, sizeof(line_buffer));
			j = 0;
		}
	}
}

int ssd_update_proc(void *dir)
{
	struct file *fp;
	int i;
	mm_segment_t fs;
	char *buf = NULL;
	int buf_size = 0;

	for (i = 0; i < FILE_READ_TRY; i++) {
		fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(ssl_initcode_file1, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			fp = filp_open(ssl_initcode_file2, O_RDONLY, 0);
			if (IS_ERR(fp)) {
				printk("-------------------read fw file error---------------\n");
				msleep(3000);
			}
		}

		if (!IS_ERR(fp)) {
			printk("---------------open fw file ok-----------------------\n");
			fp->f_op->llseek(fp, 0, SEEK_SET);
			buf_size = fp->f_op->llseek(fp, 0, SEEK_END);
			fp->f_op->llseek(fp, 0, SEEK_SET);

			buf = (char *)kzalloc(buf_size, GFP_KERNEL);
			if (buf == NULL) {
				printk("upgrade buffer kzalloc OK\n");
				filp_close(fp, NULL);
				break;
			}

			fp->f_op->read(fp, buf, buf_size, &fp->f_pos);
			printk("the upgrade file len is %d\n", buf_size);
			filp_close(fp, NULL);

			ssd_do_fw_upgrade(buf, buf_size);

			kfree(buf);
			break;
		}
		set_fs(fs);
	}

	return 0;
}

int ssd_auto_update_init(void)
{
	struct task_struct *thread = NULL;

	thread = kthread_run(ssd_update_proc, (void *)NULL, "ssd_update");
	if (IS_ERR(thread)) {
		printk("Failed to create update thread.\n");
		return -1;
	}
	return 0;
}

#endif
/* ############################################################################### */

int ssd60xx_read_regs(struct i2c_client *client, uint8_t reg, char *buf, int len)
{
	int ret;

	client->addr = client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
	buf[0] = reg;
	ret = i2c_master_send(client, (const char *)buf, len << 8 | 1);
	client->addr = client->addr & I2C_MASK_FLAG;

	/* SSL_DEBUG("%s\n", (ret < 0)? "i2c_transfer Error !":"i2c_transfer OK !"); */

	return ret;
}

int ssd60xx_write_regs(struct i2c_client *client, uint8_t reg, char *buf, int len)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, len, buf);
	/* SSL_DEBUG("%s\n", (ret < 0)? "i2c_master_send Error!":"i2c_master_send OK!"); */

	return ret;
}

int ssd60xx_read_reg(struct i2c_client *client, uint8_t reg, int data_count)
{
	unsigned char buf[4];
	int ret;

	if (data_count > 4) {
		data_count = 4;
	}
	ret = ssd60xx_read_regs(client, reg, buf, data_count);

	if (data_count == 1) {
		return (int)((unsigned int)buf[0] << 0);
	} else if (data_count == 2) {
		return (int)((unsigned int)buf[1] << 0) | ((unsigned int)
							   buf[0]
							   << 8);
	} else if (data_count == 3) {
		return (int)((unsigned int)buf[2] << 0) | ((unsigned int)
							   buf[1]
							   << 8) | ((unsigned int)buf[0]
								    << 16);
	} else if (data_count == 4) {
		return (int)((unsigned int)buf[3] << 0) | ((unsigned int)
							   buf[2]
							   << 8) | ((unsigned int)buf[1]
								    << 16) | (buf[0] << 24);
	}
	return 0;
}

int ssd60xx_write_reg(struct i2c_client *client, uint8_t reg, unsigned char data1, unsigned char data2, int data_count)
{
	unsigned char buf[4];
	int ret;

	if (reg == 0x00) {
		mdelay(data1 * 256 + data2);
		return 0;
	}

	buf[0] = data1;
	buf[1] = data2;
	buf[2] = 0;
	ret = ssd60xx_write_regs(client, reg, buf, data_count);

	return ret;
}

void ssd60xx_device_reset(void)
{
	SSL_DEBUG("enter %s\n", __func__);

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	mdelay(5);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	mdelay(2);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	mdelay(5);
}

void ssd60xx_device_patch(struct i2c_client *client)
{
	int i;

	if (ssl_priv->device_id == SSD60XX_DEVICE_ID) {
		for (i = 0; i < sizeof(ssd60xxcfgPatch) / sizeof(ssd60xxcfgPatch[0]); i++) {
			ssd60xx_write_reg(client, ssd60xxcfgPatch[i].Reg, ssd60xxcfgPatch[i].Data1, ssd60xxcfgPatch[i].Data2, ssd60xxcfgPatch[i].No);
		}
	}
	printk("%s ok\n", __func__);
}

void ssd60xx_device_init(struct i2c_client *client)
{
	int i;

	if (ssl_priv->device_id == SSD60XX_DEVICE_ID) {
		for (i = 0; i < sizeof(ssd60xxcfgTable) / sizeof(ssd60xxcfgTable[0]); i++) {
			ssd60xx_write_reg(client, ssd60xxcfgTable[i].Reg, ssd60xxcfgTable[i].Data1, ssd60xxcfgTable[i].Data2, ssd60xxcfgTable[i].No);
		}
	}
	mdelay(50);

	ssd60xx_write_reg(ssl_priv->client, 0x96, 0x00, 0x01, 2);
	mdelay(50);
	i = ssd60xx_read_reg(ssl_priv->client, 0x97, 2);

	printk("ssd60xx Patch CheckSum : 0x%04X\n", i);
}

static void ssd60xx_touch_down_up(int i, int xpos, int ypos, int press, int isdown)
{
	if (isdown) {
		if (i <= 4) {
			SSL_DEBUG("pos[%d]: X = %d , Y = %d, W = %d\n", i, xpos, ypos, press);
		}
		input_report_key(ssl_priv->input, BTN_TOUCH, 1);
		input_report_abs(ssl_priv->input, ABS_MT_TRACKING_ID, i);
		input_report_abs(ssl_priv->input, ABS_MT_TOUCH_MAJOR, press);
		input_report_abs(ssl_priv->input, ABS_MT_PRESSURE, press);
		input_report_abs(ssl_priv->input, ABS_MT_POSITION_X, xpos);
		input_report_abs(ssl_priv->input, ABS_MT_POSITION_Y, ypos);
		input_mt_sync(ssl_priv->input);

#ifdef TPD_HAVE_BUTTON
		if (FACTORY_BOOT == get_boot_mode()
		    || RECOVERY_BOOT == get_boot_mode()) {
			tpd_button(xpos, ypos, 1);
		}
#endif
	} else {
		input_report_key(ssl_priv->input, BTN_TOUCH, 0);
		input_mt_sync(ssl_priv->input);

#ifdef TPD_HAVE_BUTTON
		if (FACTORY_BOOT == get_boot_mode()
		    || RECOVERY_BOOT == get_boot_mode()) {
			tpd_button(0, 0, 0);
		}
#endif
	}
}

void filter_reg_update(void)
{
int reg_value;

   ForceTouch_threshold = ssd60xx_read_reg(ssl_priv->client, 0xFC, 2);//ForceTouch 20150729
   ForceTouch_upperbound = ((ForceTouch_threshold>>6)&0x3F);
   ForceTouch_lowerbound = (ForceTouch_threshold&0x3F);
   ForceTouch_hysteresis = ((ForceTouch_threshold>>12)&0xF);

   Reg_C4 = ssd60xx_read_reg(ssl_priv->client, 0xC4, 2);// 20150730 
   Reg_C5 = ssd60xx_read_reg(ssl_priv->client, 0xC5, 2);// 20150730 
   Reg_C6 = ssd60xx_read_reg(ssl_priv->client, 0xC6, 2);// 20150730 
   Reg_C7 = ssd60xx_read_reg(ssl_priv->client, 0xC7, 2);// 20150730 
   Reg_C8 = ssd60xx_read_reg(ssl_priv->client, 0xC8, 2);// 20150730

/* 20150806
Usage of FB (Edge Filter):
  - Edge X2 offset (D15 - D08)
  - Edge X1 offset (D7 - D0)
*/
#ifdef FORCE_EDGE_FILTER
   reg_value = ssd60xx_read_reg(ssl_priv->client, EDGE_FILTER_REG, 2);
   edge_filter_x1_offset = (reg_value>>11)&0x7;
   edge_filter_x2_offset = (reg_value>>7)&0xf;
   edge_filter_y1_offset = (reg_value>>4)&0x7;
   edge_filter_y2_offset = reg_value&0xf;
   edge_filter_enable = (reg_value>>15)&0x1;
   edge_filter_scale = (reg_value&0x4000) == 0? 1 : 4;
#endif

#ifdef CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER
/*
Usage of E5 (index filter register):
  - enable:1/disable:0 (D15)
  - penalty enable:1/disable:0 (D14)
  - penalty pressure scale (D13 - D12)
  - 1st frame penalty enable:1/disable:0 (D11)
  - no. of fingers to turn on (D10 - D08)
  - min. frames to confirm finger (D7 - D0)
*/
	reg_value = ssd60xx_read_reg(ssl_priv->client, INDEX_FILTER_REG, 2);
	jitter_filter_on = reg_value>>15;
	jitter_filter_penalty_on = (reg_value>>14)&0x1;
  jitter_filter_penalty_pressure_scale = (reg_value>>12)&0x3 + 1;
  jitter_filter_penalty_1st_frame = (reg_value>>11)&0x1;
        ssd_jitter_count = reg_value&0xff;
        jitter_finger_num = (reg_value>>8)&0x07;
        jitter_finger_num_backup = jitter_finger_num;
#endif 


#ifdef SPECIAL_FINGER_SIZE_FILTER
	reg_value = ssd60xx_read_reg(ssl_priv->client, FINGER_SIZE_FILTER, 2);
	finger_size_filter_on = reg_value>>15;
	finger_size_filter_ratio = reg_value&0x7fff;
#endif 
}

#ifdef CONFIG_HARD_PRESS_HACK_A
/*
find the bounding rectangle
remove the middle point if it is a potential ghost finger
*/
int ssd_hard_press_hack(int *arrayX, int *arrayY, int *arrayP, int finger_num)
{
  if (finger_num < 3)   return finger_num; // no need to do filtering when less than 3 fingers
  int fin_cnt = 0;
  int i = 0;
  int max_p = 0;
  int max_p_2 = 0;
  int max_p_index = 0;
  int max_p_2_index = 0;
  int threshold = ForceTouch_threshold>0 ? ((ForceTouch_threshold * 3) << 2) : 0;
  if (threshold == 0) return;   // error checking
  for (i = 0; i < FINGERNO; i++) {
    if (arrayX[i] != 0xFFF) {
      fin_cnt++;
      if (arrayP[i] > max_p) {
        max_p_2 = max_p;
        max_p_2_index = max_p_index;

        max_p = arrayP[i];
        max_p_index = i;
      }
      else if (arrayP[i] > max_p_2) {
        max_p_2 = arrayP[i];
        max_p_2_index = i;
      }
    }
  }

  if (fin_cnt < 3 || arrayX[max_p_2_index] == arrayX[max_p_index] || arrayY[max_p_2_index] == arrayY[max_p_index]) return finger_num; // no need to do filtering when less than 3 fingers

  if (max_p < threshold) return finger_num;        // the maximum pressure less than threshold, no need filtering

  for (i = 0; i < FINGERNO; i++) {
    if (arrayX[i] != 0xFFF) {
      if (i != max_p_index && i != max_p_2_index) {
        int max_x, max_y, min_x, min_y;
        if (arrayX[max_p_2_index] < arrayX[max_p_index]) max_x = arrayX[max_p_index];
        else max_x = arrayX[max_p_2_index];
        if (arrayY[max_p_2_index] < arrayY[max_p_index]) max_y = arrayY[max_p_index];
        else max_y = arrayY[max_p_2_index];
        if (min_x < arrayX[i] && arrayX[i] < max_x && min_y < arrayY[i] && arrayY[i] < max_y) {
              fin_cnt -= 1;
              arrayX[i] = 0xFFF;
              arrayY[i] = 0xFFF;
              arrayP[i] = 0;
        }
      }
    }
  }

  return fin_cnt;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER
void find_max_pressure(/*unsigned int *x_pos, unsigned int *y_pos, */unsigned int *pressure) {
  int i, max = 0;
  for (i = 0; i < FINGERNO; i++) {
    if (pressure[i] > max) {
      max = pressure[i];
    }
  }
  jitter_max_pressure = max;
}

void ssd_jitter_filter(unsigned short i, unsigned int *x_pos, unsigned int *y_pos, unsigned int *pressure, int finger_num)
{
        int reg_value = ssd60xx_read_reg(ssl_priv->client, PALM_STATUS_REG, 2);
        unsigned short palm_status = (reg_value & 0x1);

        if (jitter_filter_on == 0) {
            return;
        }

        if (*x_pos == 0xFFF) {
            jitter_filter_count[i] = 0;
            jitter_penalty[i] = 0;
            return;
        }
        if (jitter_filter_penalty_on == 0 || upmu_is_chr_det() == KAL_FALSE) {
          jitter_penalty[i] = 0;
        }

        else {
          if (ssl_priv->FingerX[i] == 0xFFF || jitter_filter_penalty_1st_frame == 0 ) {
            if (*pressure == 0) {
              // don't touch it
            }
            else if (jitter_max_pressure == 0) {
              jitter_penalty[i] = 0; // prevent error
            }
            else {
              jitter_penalty[i] = jitter_max_pressure / (*pressure * jitter_filter_penalty_pressure_scale);
            }
            if (jitter_penalty[i] > 5) {
              jitter_penalty[i] = 5;
            }
          }
        }

        if (upmu_is_chr_det()) {
            // just to make it safe, charger raised noise floor which increase the chance of ghost finger, even though the noise seems small
            if (jitter_finger_num_backup > 1) jitter_finger_num = (jitter_finger_num_backup-1);
        }
        else {
            jitter_finger_num = jitter_finger_num_backup;
        }

        // see whether condition fulfilled to start counting
        if (jitter_filter_count[i] == 0) {
            // if finger num exceed defined value && this is a new finger
            if (ssl_priv->FingerX[i] == 0xFFF) {     // new finger
                if (finger_num > jitter_finger_num) {   // finger num exceed defined value
                  *x_pos = *y_pos = 0xFFF;
                  *pressure = 0;
                  SSL_DEBUG("=======ssl_jittr_filter finger %d (started),  count %d, finger number= %d \n", i, jitter_filter_count[i], finger_num);
                  jitter_filter_count[i]++;
                }
                else {
                    // palm case: when palm rejection happens, the palm is blocked by fw.
                    // however, there will be a situation such that the hard press might introduce ghost finger within the palm area.
                    // in this situation, this ghost finger by hard press is the only finger reported, that means finger_num == 1 (or less than jitter_finger_num)
                    // this will obviously escape from the conditions
                    if (palm_status) {      // palm rejected and a new finger comes in, to be safe, kill it
                        *x_pos = *y_pos = 0xFFF;
                        *pressure = 0;
                        SSL_DEBUG("=======ssl_jittr_filter finger %d (started bcoz of palm),  count %d, finger number= %d \n", i, jitter_filter_count[i], finger_num);
                        jitter_filter_count[i]++;
                    }
                }
            }
        }
        // else, if counter already started and not yet finished counting and finger is still reported (finger is reported if you get to here!), keep rejecting
        else if (jitter_filter_count[i] < (ssd_jitter_count+jitter_penalty[i])) {
            *x_pos = *y_pos = 0xFFF;
            *pressure = 0;
            SSL_DEBUG("=======ssl_jittr_filter finger %d,  count %d, finger number= %d (penalty=%d)\n", i, jitter_filter_count[i], finger_num, jitter_penalty[i]);
            jitter_filter_count[i]++;
        }
        // otherwise, this finger shall be reported (note, that might be a real finger or prolonged noise)
}
#endif

static int ssd60xx_work(void)
{
	int i;
	unsigned short xpos = 0, ypos = 0, press = 0;
	int FingerInfo;
	int EventStatus;
	int FingerStatus;
	int FingerX[FINGERNO];
	int FingerY[FINGERNO];
	int FingerP[FINGERNO];
	int FingerCount;
	int KeyStatus;
	int Touch_status;
	//unsigned short ForceTouch_status;  //ForceTouch 20150729
        int max_pressure = 0;

	if (ssl_ts_suspend_flag) {
		return -1;
	}

	SSL_DEBUG("enter %s\n", __func__);
	/*----------------------------------------------------------

	EventStatus = ssd60xx_read_reg(ssl_priv->client, 0x31, 2);
	SSL_DEBUG("Register 0x31=0x%04X!\n", EventStatus);

	EventStatus = ssd60xx_read_reg(ssl_priv->client, 0x32, 2);
	SSL_DEBUG("Register 0x32=0x%04X!\n", EventStatus);

	EventStatus = ssd60xx_read_reg(ssl_priv->client, 0x77, 2);
	SSL_DEBUG("Register 0x77=0x%04X!\n", EventStatus);
	 ----------------------------------------------------------*/

	Touch_status = ssd60xx_read_reg(ssl_priv->client, 0x78, 2);
	SSL_DEBUG("Register 0x78=0x%04X!\n", Touch_status);



	EventStatus = ssd60xx_read_reg(ssl_priv->client, EVENT_STATUS, 2);
	FingerStatus = EventStatus >> 4;
	SSL_DEBUG("EventStatus=0x%04X!\n", EventStatus);
	FingerCount = 0;
	//ForceTouch_status = 0;  //ForceTouch 20150729

	for (i = 0; i < FINGERNO; i++) {
		if ((FingerStatus >> i) & 0x1) {
			FingerInfo = ssd60xx_read_reg(ssl_priv->client, (FINGER01_REG + i), 4);
			xpos = ((FingerInfo >> 4) & 0xF00) | ((FingerInfo >> 24) & 0xFF);
			ypos = ((FingerInfo >> 0) & 0xF00) | ((FingerInfo >> 16) & 0xFF);
			press = ((FingerInfo >> 0) & 0x0FF) * PRESS_MAX / 255;
			FingerCount++;

                        if (press > max_pressure) {
                          max_pressure = press;
                        }


			xpos = xpos * 600 / 1024;        //For MTK Scaling
			ypos = ypos * 1024 / 600;        //For MTK Scaling
			if (xpos > 600 || ypos > 1024) {
                            xpos = ypos = 0xFFF;
                            press = 0;
                            FingerCount = FingerCount-1;
			}

                        FingerY[i] = xpos;      //For MTK Remapping
                        FingerX[i] = ypos;      //For MTK Remapping
                        FingerP[i] = press;
                        SSL_DEBUG("start: %i, %i, %i (%i,%i) \n", i, edge_filter_x[i],edge_filter_y[i],FingerX[i],FingerY[i]);
#ifdef FORCE_EDGE_FILTER
                        if (edge_filter_enable) {
                          if (ssl_priv->FingerX[i] == 0xFFF || edge_filter_switch[i] == 1) {
                            unsigned short x_state = 1, y_state = 1;
                            unsigned short smoothing = 0;
                            edge_filter_switch[i] = 1;
                            if(FingerX[i]<= (edge_filter_x1_offset*edge_filter_scale)) {FingerX[i] = 0;}
                            else if(FingerX[i]>= (1024 - edge_filter_x2_offset*edge_filter_scale)) {FingerX[i] = 1023;}
                            else {x_state = 0;}
                            if(FingerY[i]<= (edge_filter_y1_offset*edge_filter_scale)) {FingerY[i] = 0;}
                            else if(FingerY[i]>= (600 - edge_filter_y2_offset*edge_filter_scale)) {FingerY[i] = 599;}
                            else {y_state = 0;}
                            if (x_state == 0 && y_state == 0) {edge_filter_switch[i] = 0;
                              if (ssl_priv->FingerX[i] != 0xFFF) {
                                smoothing = 1;
                              }
                            }
                            if (x_state == 1 && y_state == 1) {edge_filter_switch[i] = 0;}      // to avoid this fix hurts linearity
                            if (ssl_priv->FingerX[i] == 0xFFF) {
                              edge_filter_x[i] = FingerX[i];
                              edge_filter_y[i] = FingerY[i];
                              SSL_DEBUG("edge: %i, %i, %i (%i,%i) (1)\n", i, edge_filter_x[i],edge_filter_y[i],FingerX[i],FingerY[i]);
                            }
                            else if (edge_filter_switch[i] == 1) {
                              if (x_state == 0) {
                                if (abs(FingerX[i]-edge_filter_x[i])>5) {
                                  edge_filter_x[i] = FingerX[i];
                                  SSL_DEBUG("edge: %i, %i, %i (%i,%i) (2)\n", i, edge_filter_x[i],edge_filter_y[i],FingerX[i],FingerY[i]);
                                }
                              }
                              if (y_state == 0) {
                                if (abs(FingerY[i]-edge_filter_y[i])>5) {
                                  edge_filter_y[i] = FingerY[i];
                                  SSL_DEBUG("edge: %i, %i, %i (%i,%i) (3)\n", i, edge_filter_x[i],edge_filter_y[i],FingerX[i],FingerY[i]);
                                }
                              }
                              FingerX[i] = edge_filter_x[i];
                              FingerY[i] = edge_filter_y[i];
                            }
                            else if (edge_filter_switch[i] == 0) {
                              if (smoothing == 1) {
                                FingerX[i] += edge_filter_x[i];
                                FingerX[i] >>= 1;
                                FingerY[i] += edge_filter_y[i];
                                FingerY[i] >>= 1;
                              }
                            }
                            SSL_DEBUG("edge: %i, %i, %i (%i,%i) (4)\n", i, edge_filter_x[i],edge_filter_y[i],FingerX[i],FingerY[i]);
                          }
                          SSL_DEBUG("edge: %i, %i, %i (%i,%i) \n", i, edge_filter_x[i],edge_filter_y[i],FingerX[i],FingerY[i]);
                        }
#endif

		} else {
			xpos = ypos = 0xFFF;
			press = 0;
                        FingerX[i] = xpos;
                        FingerY[i] = ypos;
                        FingerP[i] = press;
#ifdef FORCE_EDGE_FILTER
                        edge_filter_switch[i] = 0;
#endif
		}
	}

        //if ((max_pressure > ForceTouch_upperbound && ForceTouch_state == 0) || (max_pressure > ForceTouch_lowerbound && ForceTouch_state == 1)) {
        if ((max_pressure > ForceTouch_upperbound && ForceTouch_state == 0) || (max_pressure > ForceTouch_lowerbound && ForceTouch_state > 0)) {
            //ForceTouch_status=1;
            //ForceTouch_alive = 1;
            ForceTouch_state = ForceTouch_hysteresis;
            SSL_DEBUG("pressure: status on = %i\n", max_pressure);
        }
        else {
          if (ForceTouch_state > 0) {
            ForceTouch_state--;
          }
          SSL_DEBUG("pressure: status off = %i\n", max_pressure);
        }

//	if (ForceTouch_state == 1 /*ForceTouch_status==1*/) { //return 0;  //ForceTouch 20150729
        if (ForceTouch_state > 0 /*ForceTouch_status==1*/) { //return 0;  //ForceTouch 20150729
		for (i = 0; i < FINGERNO; i++) {
			FingerX[i] = 0xFFF;
			FingerY[i] = 0xFFF;
			FingerP[i] = 0;
		}
		//FingerCount = 0;
                SSL_DEBUG("pressure: status count = %i\n", ForceTouch_state);
	 }

#ifdef SPECIAL_FINGER_SIZE_FILTER
	int sum_weight=0;
	int need_finger_sz_filter=0;

	if(finger_size_filter_on){
		if(upmu_is_chr_det()){
			if(FingerCount>1)
				need_finger_sz_filter=1;
		}
		else{
			if(FingerCount>2)
				need_finger_sz_filter=1;
		}
		for (i = 0; i < FINGERNO; i++) {
			if(FingerX[i] != 0xFFF)
				sum_weight +=FingerP[i];
		}
		for (i = 0; i < FINGERNO; i++) {
			if(need_finger_sz_filter && FingerY[i] > BUTTON_REGION){
				if(FingerP[i]*finger_size_filter_ratio*FingerCount<sum_weight){
					unsigned int delta = (finger_weight[i] > FingerP[i]) ? (finger_weight[i] - FingerP[i]) : (FingerP[i] - finger_weight[i]);
					unsigned int min = (finger_weight[i] > FingerP[i]) ? (FingerP[i]>>1) : (finger_weight[i]>>1);
					if (confidence[i] < 10) {
						if (delta > min || FingerP[i] == 0) {
							SSL_DEBUG("finger_size_filter, finger[%i] weight=%d filtered\n", i,FingerP[i]);
							FingerY[i] = 0xFFF;
							FingerX[i] = 0xFFF;
							FingerP[i] = 0;
							if (FingerP[i] == 0) {
								confidence[i] >>= 1;
							}
							else {
								if (confidence[i] > 0) {
									confidence[i]--;
								}
							}
						}
						else {
							confidence[i]++;
						}
					}
					else {
						if (delta > min) {
							if (confidence[i] > 0) {
								confidence[i]--;
							}
							if (FingerP[i] == 0) {
								confidence[i] >>= 1;
							}
						}
						else if (FingerP[i] == 0) {
							confidence[i] = 0;
						}
						else {
							confidence[i]++;
						}
					}
				}
				else {
					confidence[i]++;
				}
			}
			finger_weight[i] = FingerP[i];
			if (FingerP[i] == 0 || FingerX[i] == 0xFFF) confidence[i] = 0;
			//SSL_DEBUG("pos[5]: %i, %i (%i)\n", i, confidence[i],finger_weight[i]);
		}
	}
#endif 
	
        if((FingerCount == 0 && Touch_status > Reg_C4)||(FingerCount == 1 && Touch_status > Reg_C5)||
          (FingerCount == 2 && Touch_status > Reg_C6) || (FingerCount == 3 && Touch_status > Reg_C7)||
		(FingerCount >= 4  && Touch_status > Reg_C8)) {
                  //if (ForceTouch_state == 0/* && ForceTouch_alive == 0*/) {
                  if (!(ForceTouch_state != 0 && (EventStatus&0x0008) == 0)) {
                    SSL_DEBUG("ssd60xx 0x78 = %x , FingerCount = %d (%i) \n",Touch_status , FingerCount, ForceTouch_state);
                    ssd60xx_write_reg(ssl_priv->client, 0xA2, 0x00, 0x01, 2);
                    mdelay(300);
                    //return 0;
                    FingerCount = 0;
                    for (i = 0; i < FINGERNO; i++) {
                        FingerX[i] = 0xFFF;
                        FingerY[i] = 0xFFF;
                        FingerP[i] = 0;
                    }
                  }
   	}

#ifdef CONFIG_TOUCHSCREEN_SSL_TOUCH_KEY
	KeyStatus = ssd60xx_read_reg(ssl_priv->client, KEY_STATUS, 2);
	SSL_DEBUG("KeyStatus=0x%x\n", KeyStatus);

	for (i = 0; i < 4; i++) {
		if ((KeyStatus >> i) & 0x1) {
			FingerX[0] = tpd_keys_dim_local[i][0];
			FingerY[0] = tpd_keys_dim_local[i][1];
			FingerP[0] = 20;
			if (FingerCount == 0) {
				FingerCount++;
			}
		}
	}
#endif
	if (FingerCount == 0) {
		for (i = 0; i < FINGERNO; i++) {
			ssl_priv->FingerX[i] = 0xFFF;
			ssl_priv->FingerY[i] = 0xFFF;
			ssl_priv->FingerP[i] = 0xFFF;
			jitter_penalty[i] = 0;
		}
		ssd60xx_touch_down_up(0, 0, 0, 0, 0);
                //ForceTouch_alive = 0;
                ForceTouch_state = 0;
                SSL_DEBUG("pressure: status reset = %i\n", max_pressure);
	} else {
                if (ForceTouch_state > 0) {
                  ssd60xx_touch_down_up(0, 0, 0, 0, 0);
                }
                else {
#ifdef CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER
                  find_max_pressure(/*FingerX, FingerY, */FingerP);
#endif
                  for (i = 0; i < FINGERNO; i++) {
#ifdef CONFIG_TOUCHSCREEN_SSL_JITTER_FILTER
                          ssd_jitter_filter(i, &FingerX[i], &FingerY[i], &FingerP[i], FingerCount);
#endif
                          xpos = FingerX[i];
                          ypos = FingerY[i];
                          press = FingerP[i];
                          if (xpos != 0xFFF) {
                                  ssd60xx_touch_down_up(i, xpos, ypos, press, 1);
                          }
                          ssl_priv->FingerX[i] = xpos;
                          ssl_priv->FingerY[i] = ypos;
                          ssl_priv->FingerP[i] = press;
                  }
                }
	}
#ifdef CONFIG_HARD_PRESS_HACK_A
	FingerCount = ssd_hard_press_hack(FingerX,FingerY,FingerP,FingerCount);
#endif
	input_sync(ssl_priv->input);
	return 0;
}

static int ssd60xx_event_handler(void *unused)
{
	struct sched_param param = {
		.sched_priority = RTPM_PRIO_TPD
	};
	sched_setscheduler(current, SCHED_RR, &param);

	SSL_DEBUG("enter ssd60xx_event_handler!\n");

	do {
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);

		tpd_flag = 0;

		set_current_state(TASK_RUNNING);
		ssd60xx_work();
	} while (!kthread_should_stop());

	return 0;
}

static void ssd60xx_interrupt_handler(void)
{
	SSL_DEBUG("enter ssd60xx_interrupt_handler!\n");

	tpd_flag = 1;
	wake_up_interruptible(&waiter);
}

static enum hrtimer_restart ssd60xx_timer_handler(struct hrtimer *timer)
{
	if (ssl_priv->use_irq == ENABLE_TIMER) {
		SSL_DEBUG("enter ssd60xx_interrupt_handler!\n");

		tpd_flag = 1;
		wake_up_interruptible(&waiter);
		hrtimer_start(&ssl_priv->timer, ktime_set(0, MICRO_TIME_INTERUPT), HRTIMER_MODE_REL);
	}
	return HRTIMER_NORESTART;
}

static int ssd60xx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct input_dev *input_dev;
	int ret;
	int err = 0;

	SSL_DEBUG("enter %s\n", __func__);

	/* /1. kmalloc priv data */
	ssl_priv = kzalloc(sizeof(*ssl_priv), GFP_KERNEL);
	if (!ssl_priv) {
		printk("%s: kzalloc error!\n", __func__);
		return -1;
	}

	/* /2. set other info */
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
	hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_2800, "TP");
#endif

#ifdef TPD_POWER_SOURCE_1800
	hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif

#ifdef TPD_CLOSE_POWER_IN_SLEEP
	hwPowerDown(TPD_POWER_SOURCE, "TP");
	hwPowerOn(TPD_POWER_SOURCE, VOL_3300, "TP");
	msleep(100);
#endif

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	/* mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP); */
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_DOWN);

	msleep(100);

	ssl_priv->client = client;
	ssl_priv->input = tpd->dev;
	ssl_priv->use_irq = DEFAULT_USED;

	/*3. init chip inifo*/
	ssd60xx_device_reset();
	msleep(50);

	/*###############################################################################*/
	/*4 use can set here retu*/
	ret = ssd60xx_read_reg(ssl_priv->client, DEVICE_ID_REG, 2);
	if (ret != SSD60XX_DEVICE_ID) {
		ssd60xx_device_reset();
		msleep(50);
		ret = ssd60xx_read_reg(ssl_priv->client, DEVICE_ID_REG, 2);
		if (ret != SSD60XX_DEVICE_ID) {
		printk(KERN_ERR "i2c_smbus_read_byte error!!\n");
		return -1;
		}
	}
/*###############################################################################*/

	/* read chip ID */
	ssl_priv->device_id = ssd60xx_read_reg(ssl_priv->client, DEVICE_ID_REG, 2);
	ssl_priv->version_id = ssd60xx_read_reg(ssl_priv->client, VERSION_ID_REG, 2);
	ssl_priv->finger_count = ssd60xx_read_reg(ssl_priv->client, FINGER_COUNT_REG, 2);
	printk("ssd60xx Device ID    : 0x%04X\n", ssl_priv->device_id);
	printk("ssd60xx Version ID   : 0x%04X\n", ssl_priv->version_id);
	printk("ssd60xx Finger Count : 0x%04X\n", ssl_priv->finger_count);

	msleep(20);
	ssd60xx_device_patch(ssl_priv->client);
	ssd60xx_device_init(ssl_priv->client);

	/*
	 * ###############################################################################
	 * 5. set work queue
	 */
	ssd60xx_thread = kthread_run(ssd60xx_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(ssd60xx_thread)) {
		ret = PTR_ERR(ssd60xx_thread);
		TPD_DMESG(TPD_DEVICE "failed to create kernel thread: %d\n", ret);
	}

	/*
	 * ###############################################################################
	 * 6. set irq or timer for read pointer
	 */
	if (ssl_priv->use_irq == ENABLE_INT) {
		mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, ssd60xx_interrupt_handler, 0);
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	} else if (ssl_priv->use_irq == ENABLE_TIMER) {
		hrtimer_init(&ssl_priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ssl_priv->timer.function = ssd60xx_timer_handler;
		hrtimer_start(&ssl_priv->timer, ktime_set(0, MICRO_TIME_INTERUPT), HRTIMER_MODE_REL);
		SSL_DEBUG("timer_init OK!\n");
	}
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (FINGERNO - 1), 0, 0);

	tpd_load_status = 1;

/* =============================================================================== */
#ifdef MISC_TP_CHR
	err = misc_register(&ssd_misc);
	if (err < 0) {
		printk("%s: could not register ssd_misc device\n", __func__);
	}
#endif
/* =============================================================================== */

/* ******************************************************************************* */
#ifdef TPD_PROC_DEBUG
	err = sysfs_create_group(&ssl_priv->client->dev.kobj, &ssdtouch_attr_group);
	if (err) {
		printk("%s: sysfs_create_group error!\n", __func__);
	}
#endif
   filter_reg_update();
/* ******************************************************************************* */

/* ############################################################################### */
#ifdef SSL_AUTO_UPGRADE
	if (ssd_auto_update_init() < 0) {
		printk("update thread error........");
	}
#endif
/* ############################################################################### */

	SSL_DEBUG("%s ok\n", __func__);

	return 0;
}

static void ssd60xx_resume(struct early_suspend *h)
{
	int i = 0;

	SSL_DEBUG("enter %s\n", __func__);

	for (i = 0; i < sizeof(Resume) / sizeof(Resume[0]); i++) {
		ssd60xx_write_reg(ssl_priv->client, Resume[i].Reg, Resume[i].Data1, Resume[i].Data2, Resume[i].No);
	}

	/* ssd60xx_device_init(ssl_priv->client); */

	ssl_ts_suspend_flag = 0;
#ifdef TPD_HAVE_BUTTON
	if (FACTORY_BOOT == get_boot_mode()
	    || RECOVERY_BOOT == get_boot_mode()) {
		tpd_button(0, 0, 0);
	}
#endif
    msleep(10);
    filter_reg_update();

}

static void ssd60xx_suspend(struct early_suspend *h)
{
	int i = 0;

	SSL_DEBUG("enter %s\n", __func__);

	for (i = 0; i < sizeof(Suspend) / sizeof(Suspend[0]); i++) {
		ssd60xx_write_reg(ssl_priv->client, Suspend[i].Reg, Suspend[i].Data1, Suspend[i].Data2, Suspend[i].No);
	}

	ssl_ts_suspend_flag = 1;
}

static int ssd60xx_remove(struct i2c_client *client)
{
	SSL_DEBUG("enter %s\n", __func__);

/* ******************************************************************************* */
#ifdef TPD_PROC_DEBUG
	printk("%s: remove_proc_entry!\n", __func__);
	sysfs_remove_group(&ssl_priv->client->dev.kobj, &ssdtouch_attr_group);
#endif
/* ******************************************************************************* */

/* =============================================================================== */
#ifdef MISC_TP_CHR
	printk("%s: misc_deregister!\n", __func__);
	misc_deregister(&ssd_misc);
#endif
/* =============================================================================== */

	kfree(ssl_priv);

	return 0;
}

static int ssd60xx_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

static int tpd_local_init(void)
{
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		TPD_DMESG("ssd60xx unable to add i2c driver.\n");
		return -1;
	}

	if (tpd_load_status == 0) {
		TPD_DMESG("ssd60xx add error touch panel driver.\n");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}

	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (FINGERNO - 1), 0, 0);

#ifdef TPD_HAVE_BUTTON
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);	/* initialize tpd button data */
#endif

#if 0				/* (defined(TPD_WARP_START) && defined(TPD_WARP_END)) */
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if 0				/* (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION)) */
	memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = SSD60XX_I2C_NAME,
	.tpd_local_init = tpd_local_init,
	.suspend = ssd60xx_suspend,
	.resume = ssd60xx_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};

/* called when loaded into kernel */
static int __init ssd60xx_driver_init(void)
{
	int ret;
	SSL_DEBUG("enter %s\n", __func__);

	i2c_register_board_info(2, &ssd60xx_i2c_tpd, 1);
	if (tpd_driver_add(&tpd_device_driver) < 0) {
		TPD_DMESG("add ssd60xx driver failed\n");
	}
	SSL_DEBUG("Solomon SSD60XX I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
	SSL_DEBUG("Solomon SSD60XX I2C Touchscreen Driver Version %s\n", DRIVER_VERSION);

	return ret;
}

/* should never be called */
static void __exit ssd60xx_driver_exit(void)
{
	SSL_DEBUG("enter %s\n", __func__);
	tpd_driver_remove(&tpd_device_driver);
}

module_init(ssd60xx_driver_init);
module_exit(ssd60xx_driver_exit);

MODULE_AUTHOR("Solomon Systech (ShenZhen) Limited - binkazhang@solomon-systech.com");
MODULE_DESCRIPTION("ssd60xx Touchscreen Driver 1.6");
MODULE_LICENSE("GPL v2");
