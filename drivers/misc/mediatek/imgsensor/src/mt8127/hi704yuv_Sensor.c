#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"
#include "hi704yuv_Sensor.h"
#include "hi704yuv_Camera_Sensor_para.h"
#include "hi704yuv_CameraCustomized.h"

#define HI704YUV_DEBUG
#ifdef HI704YUV_DEBUG
#define SENSORDB printk
#else
#define SENSORDB(x, ...)
#endif

#if 0
extern int iReadReg(u16 a_u2Addr, u8 * a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
static int sensor_id_fail;
#define HI704_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, HI704_WRITE_ID)
#define HI704_write_cmos_sensor_2(addr, para, bytes) iWriteReg((u16) addr , (u32) para , sbytes, HI704_WRITE_ID)
kal_uint16 HI704_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	iReadReg((u16) addr, (u8 *) & get_byte, HI704_WRITE_ID);
	return get_byte;
}

#endif
static DEFINE_SPINLOCK(hi704_yuv_drv_lock);

extern int iReadRegI2C(u8 * a_pSendData, u16 a_sizeSendData, u8 * a_pRecvData,
		       u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 * a_pSendData, u16 a_sizeSendData, u16 i2cId);
kal_uint16 HI704_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	char puSendCmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd, 2, HI704_WRITE_ID);
	return 0;
}

kal_uint16 HI704_read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte = 0;
	char puSendCmd = { (char)(addr & 0xFF) };
	iReadRegI2C(&puSendCmd, 1, (u8 *) & get_byte, 1, HI704_WRITE_ID);
	return get_byte;
}

/*******************************************************************************
Adapter for Winmo typedef
********************************************************************************/
#define WINMO_USE 0

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x, ...)
#define TEXT

/*******************************************************************************
* follow is define by jun
********************************************************************************/
MSDK_SENSOR_CONFIG_STRUCT HI704SensorConfigData;
SENSOR_EXIF_INFO_STRUCT HI704ExifInfo;
static struct HI704_sensor_STRUCT HI704_sensor;
static kal_uint32 HI704_zoom_factor;
static int sensor_id_fail;
/*************************************************************************
 * FUNCTION
 *	HI704_read_Shutter
 *
 * DESCRIPTION
 *	This function read e-shutter of HI704 .
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 HI704_Read_Shutter(void)
{

	UINT8 high_exp, mid_exp, low_exp = 0;
	UINT32 EXP, t_gain, ISO, CLK = 0, shutter;

	HI704_write_cmos_sensor(0x03, 0x20);

	high_exp = HI704_read_cmos_sensor(0x80);
	mid_exp = HI704_read_cmos_sensor(0x81);
	low_exp = HI704_read_cmos_sensor(0x82);

	CLK = HI704_sensor.pv_pclk;
	shutter = ((high_exp << 16) | (mid_exp << 8) | low_exp);
	printk("HYNIXshutter=%d  ", shutter);
	EXP = shutter * 8 * 1000 / CLK;	// returns in ms
	SENSORDB
	    ("HI704ExifInfo %s,high_exp = %d,mid_exp = %d,low_exp = %d,EXP = %d",
	     __func__, high_exp, mid_exp, low_exp, EXP);
	printk("HYNIXEXP=%d  ", EXP);
	if (EXP < 1)
		EXP = 1;

	HI704ExifInfo.CapExposureTime = EXP;

	HI704_write_cmos_sensor(0x03, 0x20);
	t_gain = HI704_read_cmos_sensor(0xb0);
	SENSORDB("t_gain = %d \n", t_gain);

	printk("HI704ExifInfo t_gain=%d  ", t_gain);

	ISO = t_gain * 100 / 32;
	if (ISO < 150)
		HI704ExifInfo.RealISOValue = AE_ISO_100;
	else if (ISO < 250)
		HI704ExifInfo.RealISOValue = AE_ISO_200;
	else if (ISO < 450)
		HI704ExifInfo.RealISOValue = AE_ISO_400;
	else
		HI704ExifInfo.RealISOValue = AE_ISO_800;

	printk("HYNIXISO=%d  ", ISO);

	//  HI704ExifInfo.RealISOValue = ISO;
	return shutter;
}				/* HI704_read_shutter */

#define HI258_LOAD_FROM_T_FLASH

#ifdef HI258_LOAD_FROM_T_FLASH
/*kal_uint16 HI258_write_cmos_sensor(kal_uint8 addr, kal_uint8 para);*/

#define HI256_OP_CODE_INI		0x00	/* Initial value. */
#define HI256_OP_CODE_REG		0x01	/* Register */
#define HI256_OP_CODE_DLY		0x02	/* Delay */
#define HI256_OP_CODE_END		0x03	/* End of initial setting. */

typedef struct {
	u16 init_reg;
	u16 init_val;		/* Save the register value and delay tick */
	u8 op_code;		/* 0 - Initial value, 1 - Register, 2 - Delay, 3 - End of setting. */
} HI256_initial_set_struct;

HI256_initial_set_struct HI256_Init_Reg[2000];

static u32 strtol(const char *nptr, u8 base)
{
	u8 ret;
	if (!nptr || (base != 16 && base != 10 && base != 8)) {
		printk("%s(): NULL pointer input\n", __FUNCTION__);
		return -1;
	}
	for (ret = 0; *nptr; nptr++) {
		if ((base == 16 && *nptr >= 'A' && *nptr <= 'F') ||
		    (base == 16 && *nptr >= 'a' && *nptr <= 'f') ||
		    (base >= 10 && *nptr >= '0' && *nptr <= '9') ||
		    (base >= 8 && *nptr >= '0' && *nptr <= '7')) {
			ret *= base;
			if (base == 16 && *nptr >= 'A' && *nptr <= 'F')
				ret += *nptr - 'A' + 10;
			else if (base == 16 && *nptr >= 'a' && *nptr <= 'f')
				ret += *nptr - 'a' + 10;
			else if (base >= 10 && *nptr >= '0' && *nptr <= '9')
				ret += *nptr - '0';
			else if (base >= 8 && *nptr >= '0' && *nptr <= '7')
				ret += *nptr - '0';
		} else
			return ret;
	}
	return ret;
}

u8 Hi258_Initialize_from_T_Flash(void)
{
	u8 *curr_ptr = NULL;
	u32 file_size = 0;
	u32 i = 0, j = 0;
	u8 func_ind[4] = { 0 };	/* REG or DLY */
	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	static u8 data_buff[50 * 1024];

	fp = filp_open("/data/hi258_sd.dat", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return 2;
	} else
		printk("Hi256_Initialize_from_T_Flash Open File Success\n");

	fs = get_fs();
	set_fs(KERNEL_DS);

	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos);
	filp_close(fp, NULL);
	set_fs(fs);

	printk("1\n");

	/* Start parse the setting witch read from t-flash. */
	curr_ptr = data_buff;
	while (curr_ptr < (data_buff + file_size)) {
		while ((*curr_ptr == ' ') || (*curr_ptr == '\t'))	/* Skip the Space & TAB */
			curr_ptr++;

		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '*')) {
			while (!(((*curr_ptr) == '*')
				 && ((*(curr_ptr + 1)) == '/'))) {
				curr_ptr++;	/* Skip block comment code. */
			}

			while (!((*curr_ptr == 0x0D)
				 && (*(curr_ptr + 1) == 0x0A))) {
				curr_ptr++;
			}

			curr_ptr += 2;	/* Skip the enter line */

			continue;
		}

		if (((*curr_ptr) == '/') || ((*curr_ptr) == '{') || ((*curr_ptr) == '}')) {	/* Comment line, skip it. */
			while (!((*curr_ptr == 0x0D)
				 && (*(curr_ptr + 1) == 0x0A))) {
				curr_ptr++;
			}

			curr_ptr += 2;	/* Skip the enter line */

			continue;
		}
		/* This just content one enter line. */
		if (((*curr_ptr) == 0x0D) && ((*(curr_ptr + 1)) == 0x0A)) {
			curr_ptr += 2;
			continue;
		}
		memcpy(func_ind, curr_ptr, 3);

		if (strcmp((const char *)func_ind, "REG") == 0) {	/* REG */
			curr_ptr += 6;	/* Skip "REG(0x" or "DLY(" */
			HI256_Init_Reg[i].op_code = HI256_OP_CODE_REG;

			HI256_Init_Reg[i].init_reg =
			    strtol((const char *)curr_ptr, 16);
			curr_ptr += 6;	/* Skip "00, 0x" */

			HI256_Init_Reg[i].init_val =
			    strtol((const char *)curr_ptr, 16);
			curr_ptr += 4;	/* Skip "00);" */
		} else {	/* DLY */

			/* Need add delay for this setting. */
			curr_ptr += 4;
			HI256_Init_Reg[i].op_code = HI256_OP_CODE_DLY;

			HI256_Init_Reg[i].init_reg = 0xFF;
			HI256_Init_Reg[i].init_val = strtol((const char *)curr_ptr, 10);	/* Get the delay ticks, the delay should less then 50 */
		}
		i++;

		/* Skip to next line directly. */
		while (!((*curr_ptr == 0x0D) && (*(curr_ptr + 1) == 0x0A))) {
			curr_ptr++;
		}
		curr_ptr += 2;
	}
	printk("2\n");
	/* (0xFFFF, 0xFFFF) means the end of initial setting. */
	HI256_Init_Reg[i].op_code = HI256_OP_CODE_END;
	HI256_Init_Reg[i].init_reg = 0xFF;
	HI256_Init_Reg[i].init_val = 0xFF;
	i++;

	/* Start apply the initial setting to sensor. */
#if 1
	for (j = 0; j < i; j++) {
		if (HI256_Init_Reg[j].op_code == HI256_OP_CODE_END) {	/* End of the setting. */
			break;
		} else if (HI256_Init_Reg[j].op_code == HI256_OP_CODE_DLY) {
			msleep(HI256_Init_Reg[j].init_val);	/* Delay */
		} else if (HI256_Init_Reg[j].op_code == HI256_OP_CODE_REG) {
			HI704_write_cmos_sensor((kal_uint8)
						HI256_Init_Reg[j].init_reg,
						(kal_uint8)
						HI256_Init_Reg[j].init_val);
		} else {
			printk("REG ERROR!\n");
		}
	}
#endif
	printk("3\n");
	return 1;
}

#endif
const HI704_SENSOR_INIT_INFO HI704_Initial_Setting_Info[] = {
/*PAGE 0*/
/*Image Size/Windowing/HSYNC/VSYNC[Type1]*/
	{0x03, 0x00},		/*PAGEMODE(0x03) */

#if 0
/* under construction !*/
/* under construction !*/
/* under construction !*/
#endif

	{0x03, 0x00},
	{0x10, 0x00},
	{0x11, 0x90},		//XY Flip
	{0x12, 0x04},
	{0x20, 0x00},
	{0x21, 0x02},		//Flip 0x02, None 0x04
	{0x22, 0x00},
	{0x23, 0x00},		//Flip 0x00, None 0x04

	{0x40, 0x01},		//Hblank 296
	{0x41, 0x58},
	{0x42, 0x00},
	{0x43, 0x14},		//Vblank 04

//BLC
	{0x80, 0x2e},		//don't touch
	{0x81, 0x7e},		//don't touch
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},		//*** Change 100406
	{0x85, 0x4b},		//*** Change 100406
	{0x89, 0x48},		//don't touch
	{0x90, 0x0f},		//BLC_TIME_TH_ON
	{0x91, 0x0f},		//BLC_TIME_TH_OFF
	{0x92, 0x78},		//BLC_AG_TH_ON
	{0x93, 0x70},		//BLC_AG_TH_OFF

	{0x98, 0x38},
	{0x99, 0x42},		//don't touch
	{0xa8, 0x42},		//don't touch

//Page 2  Last Update 15_03_10
	{0x03, 0x02},
	{0x22, 0xa7},		//don't touch
	{0x52, 0xa2},		//don't touch
	{0x54, 0x10},		//don't touch
	{0x55, 0x18},		//don't touch
	{0x56, 0x0c},		//don't touch
	{0x59, 0x00},		//don't touch

	{0x60, 0x11},		//don't touch
	{0x61, 0x1b},		//don't touch
	{0x62, 0x11},		//don't touch
	{0x63, 0x1a},		//don't touch
	{0x64, 0x11},		//don't touch
	{0x65, 0x1a},		//don't touch
	{0x72, 0x12},		//don't touch
	{0x73, 0x19},		//don't touch
	{0x74, 0x12},		//don't touch
	{0x75, 0x19},		//don't touch
	{0x80, 0x1d},		//don't touch
	{0xa0, 0x1d},		//don't touch
	{0xa5, 0x1d},		//don't touch
	{0xb8, 0x10},		//don't touch
	{0xb9, 0x13},		//don't touch

	{0xc0, 0x04},		//don't touch
	{0xc1, 0x0d},		//don't touch
	{0xc2, 0x04},		//don't touch
	{0xc3, 0x0d},		//don't touch
	{0xc4, 0x05},		//don't touch
	{0xc5, 0x0c},		//don't touch
	{0xc6, 0x05},		//don't touch
	{0xc7, 0x0c},		//don't touch
	{0xc8, 0x06},		//don't touch
	{0xc9, 0x0b},		//don't touch
	{0xca, 0x06},		//don't touch
	{0xcb, 0x0b},		//don't touch
	{0xcc, 0x06},		//don't touch
	{0xcd, 0x0a},		//don't touch
	{0xce, 0x06},		//don't touch
	{0xcf, 0x0a},		//don't touch
	{0xd0, 0x03},		//don't touch
	{0xd1, 0x1c},		//don't touch

//Page 10
	{0x03, 0x10},
	{0x10, 0x00},
	{0x11, 0x43},
	{0x12, 0x30},
	{0x40, 0x80},
	{0x41, 0x08},
	{0x48, 0x80},
	{0x50, 0x78},

	{0x60, 0x7d},
	{0x61, 0x00},
	{0x62, 0xb2},
	{0x63, 0x8a},
	{0x64, 0xa0},
	{0x65, 0x88},
	{0x66, 0x80},
	{0x67, 0x36},
	{0x68, 0x80},

//LPF
	{0x03, 0x11},
	{0x10, 0x20},
	{0x11, 0x07},
	{0x20, 0x00},
	{0x21, 0x60},
	{0x23, 0x0A},
	{0x60, 0x12},
	{0x61, 0x85},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},

	{0x67, 0x70},
	{0x68, 0x24},
	{0x69, 0x04},

//Page 12
//2D
	{0x03, 0x12},
	{0x40, 0xD3},
	{0x41, 0x09},
	{0x50, 0x16},
	{0x51, 0x24},
	{0x70, 0x1F},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x12},
	{0x75, 0x12},
	{0x76, 0x20},
	{0x77, 0x80},
	{0x78, 0x88},
	{0x79, 0x18},

/////////////////////
	{0x90, 0x3d},
	{0x91, 0x34},
	{0x99, 0x28},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x28},
	{0x9f, 0x28},
	{0xb0, 0x7d},
	{0xb5, 0x44},
	{0xb6, 0x82},
	{0xb7, 0x52},
	{0xb8, 0x44},
	{0xb9, 0x15},
/////////////////////

//Edge
	{0x03, 0x13},
	{0x10, 0x01},
	{0x11, 0x89},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},
	{0x20, 0x03},
	{0x21, 0x05},
	{0x23, 0x25},
	{0x24, 0x21},
	{0x25, 0x08},
	{0x26, 0x40},
	{0x27, 0x00},
	{0x28, 0x08},
	{0x29, 0x80},
	{0x2A, 0xE0},
	{0x2B, 0x10},
	{0x2C, 0x28},
	{0x2D, 0x40},
	{0x2E, 0x00},
	{0x2F, 0x00},
	{0x30, 0x11},
	{0x80, 0x04},
	{0x81, 0x07},
	{0x90, 0x05},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x30},
	{0x94, 0x30},
	{0x95, 0x10},

//Shading
	{0x03, 0x14},
	{0x10, 0x01},
	{0x20, 0x7e},
	{0x21, 0x95},
	{0x22, 0x2f},
	{0x23, 0x1c},
	{0x24, 0x1f},

//Page 15 CMC
	{0x03, 0x15},
	{0x10, 0x03},
	{0x14, 0x52},		//0802 by Sam
	{0x16, 0x52},
	{0x17, 0x2d},
	{0x30, 0xfd},
	{0x31, 0xb9},
	{0x32, 0x40},
	{0x33, 0x2b},
	{0x34, 0xdd},
	{0x35, 0x33},
	{0x36, 0x07},
	{0x37, 0x3a},
	{0x38, 0x70},
//CMC OFS
	{0x40, 0xa0},
	{0x41, 0xf0},
	{0x42, 0x88},
	{0x43, 0x18},
	{0x44, 0x68},
	{0x45, 0x40},
	{0x46, 0x00},
	{0x47, 0x38},
	{0x48, 0xa0},

//Gamma
	{0x03, 0x16},

	{0x30, 0x00},
	{0x31, 0x0b},
	{0x32, 0x15},
	{0x33, 0x3d},
	{0x34, 0x6c},
	{0x35, 0x89},
	{0x36, 0xa0},
	{0x37, 0xb1},
	{0x38, 0xc6},
	{0x39, 0xd1},
	{0x3a, 0xda},
	{0x3b, 0xe9},
	{0x3c, 0xf5},
	{0x3d, 0xfb},
	{0x3e, 0xff},

/*Page 17 AE*/
	{0x03, 0x17},
	{0xc4, 0x3c},
	{0xc5, 0x32},

/*Page 20 AE*/
	{0x03, 0x20},
	{0x10, 0x0c},
	{0x11, 0x44},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},

	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},

	{0x30, 0xf8},

	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22},
	{0x3c, 0xde},

	{0x60, 0x95},
	{0x68, 0x3c},
	{0x69, 0x64},
	{0x6A, 0x28},
	{0x6B, 0xc8},

	{0x70, 0x2c},		//For Y decay
	{0x76, 0x22},
	{0x77, 0x02},
	{0x78, 0x12},
	{0x79, 0x27},
	{0x7a, 0x23},
	{0x7c, 0x1d},
	{0x7d, 0x22},

	{0x83, 0x00},		//EXP Normal 30.00 fps
	{0x84, 0xd2},
	{0x85, 0x1c},
	{0x86, 0x00},		//EXPMin 6827.73 fps
	{0x87, 0xee},
	{0x88, 0x03},		//EXP Max 60hz 8.00 fps
	{0x89, 0x13},
	{0x8a, 0xe9},
	{0xa0, 0x02},		//EXP Max 50hz 8.33 fps
	{0xa1, 0xf6},
	{0xa2, 0xa0},
	{0x8B, 0x3f},		//EXP100
	{0x8C, 0x38},
	{0x8D, 0x34},		//EXP120
	{0x8E, 0x87},
	{0x9c, 0x06},		//EXP Limit 975.39 fps
	{0x9d, 0x82},
	{0x9e, 0x00},		//EXP Unit
	{0x9f, 0xee},
	{0xa3, 0x00},		//Outdoor Int
	{0xa4, 0x34},

	{0xb0, 0x3d},
	{0xb1, 0x14},
	{0xb2, 0xe1},

	{0xb3, 0x30},
	{0xb4, 0x20},
	{0xb5, 0x50},
	{0xb6, 0x38},
	{0xb7, 0x30},
	{0xb8, 0x2c},
	{0xb9, 0x2a},
	{0xba, 0x28},
	{0xbb, 0x27},
	{0xbc, 0xa0},
	{0xbd, 0xa0},

	{0xc0, 0x16},		//0x1a->0x16
	{0xc3, 0x48},
	{0xc4, 0x48},

//Page 22 AWB
	{0x03, 0x22},
	{0x10, 0xe2},
	{0x11, 0x2e},
	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},
	{0x3a, 0x88},
	{0x3b, 0xc4},
	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x02},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x80, 0x35},
	{0x81, 0x20},
	{0x82, 0x40},
	{0x83, 0x58},		//RMAX Default
	{0x84, 0x21},		//RMIN Default
	{0x85, 0x60},		//BMAX Default
	{0x86, 0x25},		//BMIN Default
	{0x87, 0x44},		//RMAXB Default
	{0x88, 0x20},		//RMINB Default
	{0x89, 0x40},		//BMAXB Default
	{0x8a, 0x20},		//BMINB Default
	{0x8b, 0x04},		//OUT TH
	{0x8d, 0x22},
	{0x8e, 0x91},

	{0x8f, 0x63},		//awb_slope_th0
	{0x90, 0x61},		//awb_slope_th1
	{0x91, 0x5b},		//awb_slope_th2
	{0x92, 0x52},		//awb_slope_th3
	{0x93, 0x45},		//awb_slope_th4
	{0x94, 0x3c},		//awb_slope_th5
	{0x95, 0x34},		//awb_slope_th6
	{0x96, 0x2f},		//awb_slope_th7
	{0x97, 0x28},		//awb_slope_th8
	{0x98, 0x24},		//awb_slope_th9
	{0x99, 0x21},		//awb_slope_th10
	{0x9a, 0x20},		//awb_slope_th11

	{0x9b, 0x0f},		//awb_slope_delta1

	{0xb0, 0x30},
	{0xb1, 0x48},

//{0x9b, 0x0f}, //awb_slope_delta1

	

	{0x03, 0x17},		//Page 17
	{0xC0, 0x01},
	{0xC4, 0x3c},		//FLK200
	{0xC5, 0x32},		//FLK240

	{0x03, 0x20},
	{0x10, 0x0c},		//AE OFF
	{0x18, 0x38},		//AE RESET

	{0x83, 0x00},		//EXP Normal 30.00 fps
	{0x84, 0xd2},
	{0x85, 0x1c},
	{0x86, 0x00},		//EXPMin 6827.73 fps
	{0x87, 0xee},
	{0x88, 0x03},		//EXP Max 60hz 8.00 fps
	{0x89, 0x13},
	{0x8a, 0xe9},
	{0xa0, 0x02},		//EXP Max 50hz 8.33 fps
	{0xa1, 0xf6},
	{0xa2, 0xa0},
	{0x8B, 0x3f},		//EXP100
	{0x8C, 0x38},
	{0x8D, 0x34},		//EXP120
	{0x8E, 0x87},
	{0x9c, 0x06},		//EXP Limit 975.39 fps
	{0x9d, 0x82},
	{0x9e, 0x00},		//EXP Unit
	{0x9f, 0xee},



	{0x83, 0x00},		//EXP Normal 30.00 fps
	{0x84, 0xbe},
	{0x85, 0x6e},
	{0x86, 0x00},		//EXPMin 6827.73 fps
	{0x87, 0xfa},
	{0x88, 0x02},		//EXP Max 60hz 8.00 fps
	{0x89, 0xf9},
	{0x8a, 0xb8},
	{0xa0, 0x02},		//EXP Max 50hz 8.33 fps
	{0xa1, 0xf8},
	{0xa2, 0xb8},
	{0x8B, 0x3f},		//EXP100
	{0x8C, 0x7a},
	{0x8D, 0x34},		//EXP120
	{0x8E, 0xbc},
	{0x9c, 0x04},		//EXP Limit 975.39 fps
	{0x9d, 0x65},
	{0x9e, 0x00},		//EXP Unit
	{0x9f, 0xfa},
	{0xa3, 0x00},		//Outdoor Int
	{0xa4, 0x34},

	{0x03, 0x22},
	{0x10, 0xfb},

	{0x03, 0x20},
	{0x10, 0x8c},
	
   {0x03, 0x20},
	{0x10, 0xec},		//AE ON with AFC ON
	{0x18, 0x30},	
	
   {0x03, 0x00},
   {0x01, 0xf0},
   {0xff, 0xff},		//End of Initial Setting
};

static void HI704_Initial_Setting(void)
{
	kal_uint32 iEcount;
	for (iEcount = 0;
	     (!((0xff == (HI704_Initial_Setting_Info[iEcount].address))
		&& (0xff == (HI704_Initial_Setting_Info[iEcount].data))));
	     iEcount++) {
		HI704_write_cmos_sensor(HI704_Initial_Setting_Info
					[iEcount].address,
					HI704_Initial_Setting_Info
					[iEcount].data);
	}
}

static void HI704_Init_Parameter(void)
{
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.first_init = KAL_TRUE;
	HI704_sensor.pv_mode = KAL_TRUE;
	HI704_sensor.night_mode = KAL_FALSE;
	HI704_sensor.MPEG4_Video_mode = KAL_FALSE;

	HI704_sensor.cp_pclk = HI704_sensor.pv_pclk;

	HI704_sensor.pv_dummy_pixels = 0;
	HI704_sensor.pv_dummy_lines = 0;
	HI704_sensor.cp_dummy_pixels = 0;
	HI704_sensor.cp_dummy_lines = 0;

	HI704_sensor.wb = 0;
	HI704_sensor.exposure = 0;
	HI704_sensor.effect = 0;
	HI704_sensor.banding = AE_FLICKER_MODE_50HZ;

	HI704_sensor.pv_line_length = 640;
	HI704_sensor.pv_frame_height = 480;
	HI704_sensor.cp_line_length = 640;
	HI704_sensor.cp_frame_height = 480;
	spin_unlock(&hi704_yuv_drv_lock);
}

static kal_uint8 HI704_power_on(void)
{
	kal_uint8 HI704_sensor_id = 0;
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.pv_pclk = 13000000;
	spin_unlock(&hi704_yuv_drv_lock);
	/*Software Reset */
	HI704_write_cmos_sensor(0x01, 0xf1);
	HI704_write_cmos_sensor(0x01, 0xf3);
	HI704_write_cmos_sensor(0x01, 0xf1);

	/* Read Sensor ID  */
	HI704_sensor_id = HI704_read_cmos_sensor(0x04);
	SENSORDB("[HI704YUV]:lixing read Sensor ID:%x\n", HI704_sensor_id);
	return HI704_sensor_id;

}

int HI704_SEN_RUN_TEST_PATTERN = 0;
#define HI704_TEST_PATTERN_CHECKSUM 0x786a9657

UINT32 HI704SetTestPatternMode(kal_bool bEnable)
{
	/*output color bar */
	HI704_write_cmos_sensor(0x03, 0x00);
	HI704_write_cmos_sensor(0x50, 0x04);
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	HI704Open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI704Open(void)
{
	spin_lock(&hi704_yuv_drv_lock);
	sensor_id_fail = 0;
	spin_unlock(&hi704_yuv_drv_lock);
	SENSORDB("[Enter]:HI704 Open func:");

	if (HI704_power_on() != HI704_SENSOR_ID) {
		SENSORDB("[HI704]Error:read sensor ID fail\n");
		spin_lock(&hi704_yuv_drv_lock);
		sensor_id_fail = 1;
		spin_unlock(&hi704_yuv_drv_lock);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
#ifdef HI258_LOAD_FROM_T_FLASH
	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	static kal_uint8 fromsd;

	printk("HI258 Open File Start\n");
	fp = filp_open("/data/hi258_sd.dat", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		fromsd = 0;
		printk("open file error\n");
	} else {
		printk("open file success\n");
		fromsd = 1;
		filp_close(fp, NULL);
	}
	if (fromsd == 1)
		Hi258_Initialize_from_T_Flash();
	else {
		/* Apply sensor initail setting */
		HI704_Initial_Setting();
		HI704_Init_Parameter();
	}
#endif

	/* Apply sensor initail setting */
	//HI704_Initial_Setting();
	//HI704_Init_Parameter();

	SENSORDB("[Exit]:HI704 Open func\n");
	return ERROR_NONE;
}				/* HI704Open() */

/*************************************************************************
* FUNCTION
*	HI704_GetSensorID
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
#ifdef SLT_DEVINFO_CMM
#include  <linux/dev_info.h>
static struct devinfo_struct *s_DEVINFO_ccm;
#endif
static kal_uint32 HI704_GetSensorID(kal_uint32 * sensorID)
{
	SENSORDB("[Enter]:HI704 Open func ");
	*sensorID = HI704_power_on();
#ifdef SLT_DEVINFO_CMM
	char mid_info = 0;
	char dev_vendorlist[0x20][20] =
	    { "null", "sunny", "truly", "A-kerr", "LiteArray", "Darling",
		"Qtech", "OFlim", "Huaquan"
	};
	s_DEVINFO_ccm =
	    (struct devinfo_struct *)kmalloc(sizeof(struct devinfo_struct),
					     GFP_KERNEL);
	s_DEVINFO_ccm->device_type = "CCM-S";	/*dongge 20131219 add -S sub, -M main */
	s_DEVINFO_ccm->device_module = "H5PV-W6200HQ-JDK";	/*can change if got module id */
	s_DEVINFO_ccm->device_vendor = "Huaquan";
	s_DEVINFO_ccm->device_ic = "hi708";
	s_DEVINFO_ccm->device_version = "Hynix";
	s_DEVINFO_ccm->device_info = "30W";
#endif

	if (*sensorID != HI704_SENSOR_ID) {
		SENSORDB("[HI704]Error:read sensor ID fail\n");
		spin_lock(&hi704_yuv_drv_lock);
		sensor_id_fail = 1;
		spin_unlock(&hi704_yuv_drv_lock);
		*sensorID = 0xFFFFFFFF;
#ifdef SLT_DEVINFO_CMM
		s_DEVINFO_ccm->device_used = DEVINFO_UNUSED;
		devinfo_check_add_device(s_DEVINFO_ccm);
#endif
		return ERROR_SENSOR_CONNECT_FAIL;
	}
#ifdef SLT_DEVINFO_CMM
	s_DEVINFO_ccm->device_used = DEVINFO_USED;
	devinfo_check_add_device(s_DEVINFO_ccm);
#endif
	return ERROR_NONE;
}				/* HI704Open  */

/*************************************************************************
* FUNCTION
*	HI704Close
*
* DESCRIPTION
*	This function is to turn off sensor module power.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI704Close(void)
{

	return ERROR_NONE;
}				/* HI704Close() */

static void HI704_Set_Mirror_Flip(kal_uint8 image_mirror)
{
    /********************************************************
    * Page Mode 0: Reg 0x0011 bit[1:0] = [Y Flip : X Flip]
    * 0: Off; 1: On.
    *********************************************************/
	kal_uint8 temp_data;
	SENSORDB("[Enter]:HI704 set Mirror_flip func:image_mirror=%d\n",
		 image_mirror);
	HI704_write_cmos_sensor(0x03, 0x00);
	temp_data = (HI704_read_cmos_sensor(0x11));
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.mirror = temp_data;
	spin_unlock(&hi704_yuv_drv_lock);
	HI704_write_cmos_sensor(0x11, 0x90);
	SENSORDB("[Exit]:HI704 set Mirror_flip func\n");
}

#if 0
static void HI704_set_dummy(kal_uint16 dummy_pixels, kal_uint16 dummy_lines)
{
	HI704_write_cmos_sensor(0x03, 0x00);
	HI704_write_cmos_sensor(0x40, ((dummy_pixels & 0x0F00)) >> 8);
	HI704_write_cmos_sensor(0x41, (dummy_pixels & 0xFF));
	HI704_write_cmos_sensor(0x42, ((dummy_lines & 0xFF00) >> 8));
	HI704_write_cmos_sensor(0x43, (dummy_lines & 0xFF));
}
#endif

/* 640 * 480*/
static void HI704_Set_VGA_mode(void)
{
	kal_uint32 temp;

	HI704_write_cmos_sensor(0x03, 0x00);
	HI704_write_cmos_sensor(0x10, 0x00);	/*VGA Size */

	HI704_write_cmos_sensor(0x20, 0x00);
	HI704_write_cmos_sensor(0x21, 0x02);	//For XYFlip
	HI704_write_cmos_sensor(0x22, 0x00);
	HI704_write_cmos_sensor(0x23, 0x00);	//For XYFlip



	HI704_write_cmos_sensor(0x03, 0x11);
	HI704_write_cmos_sensor(0x10, 0x25);

	HI704_write_cmos_sensor(0x03, 0x20);

	HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) & 0x7f);	/*Close AE */
	HI704_write_cmos_sensor(0x03, 0x20);
	HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) | 0x80);
}

void HI704_night_mode(kal_bool enable)
{
	SENSORDB("[Enter]HI704 night mode func:enable = %d\n", enable);
	SENSORDB("HI704_sensor.video_mode = %d\n",
		 HI704_sensor.MPEG4_Video_mode);
	SENSORDB("HI704_sensor.night_mode = %d\n", HI704_sensor.night_mode);
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.night_mode = enable;
	spin_unlock(&hi704_yuv_drv_lock);

	if (HI704_sensor.MPEG4_Video_mode == KAL_TRUE)

		if (enable) {
			/*HI704_Cal_Min_Frame_Rate(HI704_MIN_FRAMERATE_5); */
			HI704_write_cmos_sensor(0x03, 0x00);
			HI704_write_cmos_sensor(0x01, 0xf1);

			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x83, 0x00);
			HI704_write_cmos_sensor(0x84, 0xc3);
			HI704_write_cmos_sensor(0x85, 0x3c);
			HI704_write_cmos_sensor(0x86, 0x00);
			HI704_write_cmos_sensor(0x87, 0xee);
			HI704_write_cmos_sensor(0x88, 0x04);
			HI704_write_cmos_sensor(0x89, 0xec);
			HI704_write_cmos_sensor(0x8a, 0xa8);
			HI704_write_cmos_sensor(0xa0, 0x04);
			HI704_write_cmos_sensor(0xa1, 0xec);
			HI704_write_cmos_sensor(0xa2, 0xa8);
			HI704_write_cmos_sensor(0xb1, 0x14);
			HI704_write_cmos_sensor(0xb2, 0xff);

			HI704_write_cmos_sensor(0x03, 0x00);
			HI704_write_cmos_sensor(0x01, 0xf0);

			/*  HI704_write_cmos_sensor(0x18, 0x38); */
		} else {
			/*HI704_Cal_Min_Frame_Rate(HI704_MIN_FRAMERATE_10); */

		}
}

/*************************************************************************
* FUNCTION
*	HI704Preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static UINT32 HI704Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
			   MSDK_SENSOR_CONFIG_STRUCT * sensor_config_data)
{
	spin_lock(&hi704_yuv_drv_lock);
	sensor_config_data->SensorImageMirror = IMAGE_NORMAL;
	if (HI704_sensor.first_init == KAL_TRUE) {
		HI704_sensor.MPEG4_Video_mode = HI704_sensor.MPEG4_Video_mode;
	} else {
		HI704_sensor.MPEG4_Video_mode = !HI704_sensor.MPEG4_Video_mode;
	}
	spin_unlock(&hi704_yuv_drv_lock);

	SENSORDB("[Enter]:HI704 preview func:");
	SENSORDB("HI704_sensor.video_mode = %d\n",
		 HI704_sensor.MPEG4_Video_mode);
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.first_init = KAL_FALSE;
	HI704_sensor.pv_mode = KAL_TRUE;
	spin_unlock(&hi704_yuv_drv_lock);

	{
		SENSORDB("[HI704]preview set_VGA_mode\n");
		HI704_Set_VGA_mode();
	}

	HI704_Set_Mirror_Flip(sensor_config_data->SensorImageMirror);

	SENSORDB("[Exit]:HI704 preview func\n");
	return TRUE;
}				/* HI704_Preview */

UINT32 HI704Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
		    MSDK_SENSOR_CONFIG_STRUCT * sensor_config_data)
{
	SENSORDB("[HI704][Enter]HI704_capture_func\n");
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.pv_mode = KAL_FALSE;
	spin_unlock(&hi704_yuv_drv_lock);

	return ERROR_NONE;
}				/* HM3451Capture() */

UINT32 HI704GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *
			  pSensorResolution)
{
	SENSORDB("[Enter]:HI704 get Resolution func\n");

	pSensorResolution->SensorFullWidth = HI704_IMAGE_SENSOR_FULL_WIDTH - 10;
	pSensorResolution->SensorFullHeight =
	    HI704_IMAGE_SENSOR_FULL_HEIGHT - 10 - 10;
	pSensorResolution->SensorPreviewWidth =
	    HI704_IMAGE_SENSOR_PV_WIDTH - 16;
	pSensorResolution->SensorPreviewHeight =
	    HI704_IMAGE_SENSOR_PV_HEIGHT - 12 - 10;
	pSensorResolution->SensorVideoWidth = HI704_IMAGE_SENSOR_PV_WIDTH - 16;
	pSensorResolution->SensorVideoHeight =
	    HI704_IMAGE_SENSOR_PV_HEIGHT - 12 - 10;
	pSensorResolution->Sensor3DFullWidth =
	    HI704_IMAGE_SENSOR_FULL_WIDTH - 10;
	pSensorResolution->Sensor3DFullHeight =
	    HI704_IMAGE_SENSOR_FULL_HEIGHT - 10 - 10;
	pSensorResolution->Sensor3DPreviewWidth =
	    HI704_IMAGE_SENSOR_PV_WIDTH - 16;
	pSensorResolution->Sensor3DPreviewHeight =
	    HI704_IMAGE_SENSOR_PV_HEIGHT - 12 - 10;
	pSensorResolution->Sensor3DVideoWidth =
	    HI704_IMAGE_SENSOR_PV_WIDTH - 16;
	pSensorResolution->Sensor3DVideoHeight =
	    HI704_IMAGE_SENSOR_PV_HEIGHT - 12 - 10;

	SENSORDB("[Exit]:HI704 get Resolution func\n");
	return ERROR_NONE;
}				/* HI704GetResolution() */

UINT32 HI704GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_INFO_STRUCT * pSensorInfo,
		    MSDK_SENSOR_CONFIG_STRUCT * pSensorConfigData)
{
	SENSORDB("[Enter]:HI704 getInfo func:ScenarioId = %d\n", ScenarioId);

	pSensorInfo->SensorPreviewResolutionX = HI704_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY = HI704_IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX = HI704_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY = HI704_IMAGE_SENSOR_FULL_HEIGHT;

	pSensorInfo->SensorCameraPreviewFrameRate = 30;
	pSensorInfo->SensorVideoFrameRate = 30;
	pSensorInfo->SensorStillCaptureFrameRate = 30;
	pSensorInfo->SensorWebCamCaptureFrameRate = 15;
	pSensorInfo->SensorResetActiveHigh = FALSE;
	pSensorInfo->SensorResetDelayCount = 4;
	pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_YUYV;	/*SENSOR_OUTPUT_FORMAT_YVYU; */
	pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorInterruptDelayLines = 1;
	pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_PARALLEL;

	pSensorInfo->CaptureDelayFrame = 4;
	pSensorInfo->PreviewDelayFrame = 8;
	pSensorInfo->VideoDelayFrame = 0;
	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		pSensorInfo->SensorClockFreq = 26;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = 1;
		pSensorInfo->SensorGrabStartY = 10;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
		pSensorInfo->SensorClockFreq = 26;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = 1;
		pSensorInfo->SensorGrabStartY = 10;
		break;
	default:
		pSensorInfo->SensorClockFreq = 26;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = 1;
		pSensorInfo->SensorGrabStartY = 10;
		break;
	}
	memcpy(pSensorConfigData, &HI704SensorConfigData,
	       sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	SENSORDB("[Exit]:HI704 getInfo func\n");
	return ERROR_NONE;
}				/* HI704GetInfo() */

UINT32 HI704Control(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * pImageWindow,
		    MSDK_SENSOR_CONFIG_STRUCT * pSensorConfigData)
{
	SENSORDB("[Enter]:HI704 Control func:ScenarioId = %d\n", ScenarioId);

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		/*case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: */
		/*case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO: */
		HI704Preview(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		/*case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: */
		HI704Capture(pImageWindow, pSensorConfigData);
		break;
	default:
		break;
	}

	SENSORDB("[Exit]:HI704 Control func\n");
	return TRUE;
}				/* HI704Control() */

/*************************************************************************
* FUNCTION
*	HI704_set_param_wb
*
* DESCRIPTION
*	wb setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_wb(UINT16 para)
{
	/*This sensor need more time to balance AWB, */
	/*we suggest higher fps or drop some frame to avoid garbage color when preview initial */
	SENSORDB("[Enter]HI704 set_param_wb func:para = %d\n", para);

	if (HI704_sensor.wb == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.wb = para;
	spin_unlock(&hi704_yuv_drv_lock);

	switch (para) {
	case AWB_MODE_AUTO:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x2e);
			HI704_write_cmos_sensor(0x83, 0x58);
			HI704_write_cmos_sensor(0x84, 0x21);
			HI704_write_cmos_sensor(0x85, 0x60);
			HI704_write_cmos_sensor(0x86, 0x25);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_CLOUDY_DAYLIGHT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x10, 0x6a);
			HI704_write_cmos_sensor(0x80, 0x50);
			HI704_write_cmos_sensor(0x81, 0x20);
			HI704_write_cmos_sensor(0x82, 0x24);
			HI704_write_cmos_sensor(0x83, 0x65);
			HI704_write_cmos_sensor(0x84, 0x45);
			HI704_write_cmos_sensor(0x85, 0x2a);
			HI704_write_cmos_sensor(0x86, 0x1c);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_DAYLIGHT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x59);
			HI704_write_cmos_sensor(0x82, 0x29);
			HI704_write_cmos_sensor(0x83, 0x60);
			HI704_write_cmos_sensor(0x84, 0x50);
			HI704_write_cmos_sensor(0x85, 0x2f);
			HI704_write_cmos_sensor(0x86, 0x23);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_INCANDESCENT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x29);
			HI704_write_cmos_sensor(0x82, 0x54);
			HI704_write_cmos_sensor(0x83, 0x2e);
			HI704_write_cmos_sensor(0x84, 0x23);
			HI704_write_cmos_sensor(0x85, 0x58);
			HI704_write_cmos_sensor(0x86, 0x4f);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_FLUORESCENT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x41);
			HI704_write_cmos_sensor(0x82, 0x42);
			HI704_write_cmos_sensor(0x83, 0x44);
			HI704_write_cmos_sensor(0x84, 0x34);
			HI704_write_cmos_sensor(0x85, 0x46);
			HI704_write_cmos_sensor(0x86, 0x3a);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_TUNGSTEN:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x80, 0x24);
			HI704_write_cmos_sensor(0x81, 0x20);
			HI704_write_cmos_sensor(0x82, 0x58);
			HI704_write_cmos_sensor(0x83, 0x27);
			HI704_write_cmos_sensor(0x84, 0x22);
			HI704_write_cmos_sensor(0x85, 0x58);
			HI704_write_cmos_sensor(0x86, 0x52);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_OFF:
		{
			SENSORDB("HI704 AWB OFF");
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x10, 0xe2);
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}				/* HI704_set_param_wb */

/*************************************************************************
* FUNCTION
*	HI704_set_param_effect
*
* DESCRIPTION
*	effect setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_effect(UINT16 para)
{
	SENSORDB("[Enter]HI704 set_param_effect func:para = %d\n", para);

	if (HI704_sensor.effect == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.effect = para;
	spin_unlock(&hi704_yuv_drv_lock);

	switch (para) {
	case MEFFECT_OFF:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x30);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0x80);
			HI704_write_cmos_sensor(0x45, 0x80);

			HI704_write_cmos_sensor(0x47, 0x7f);
			HI704_write_cmos_sensor(0x03, 0x13);
			HI704_write_cmos_sensor(0x20, 0x07);
			HI704_write_cmos_sensor(0x21, 0x07);
		}
		break;
	case MEFFECT_SEPIA:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x23);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0x70);
			HI704_write_cmos_sensor(0x45, 0x98);

			HI704_write_cmos_sensor(0x47, 0x7f);
			HI704_write_cmos_sensor(0x03, 0x13);
			HI704_write_cmos_sensor(0x20, 0x07);
			HI704_write_cmos_sensor(0x21, 0x07);
		}
		break;
	case MEFFECT_NEGATIVE:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x08);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x14, 0x00);
		}
		break;
	case MEFFECT_SEPIAGREEN:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x03);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0x30);
			HI704_write_cmos_sensor(0x45, 0x50);
		}
		break;
	case MEFFECT_SEPIABLUE:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x03);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0xb0);
			HI704_write_cmos_sensor(0x45, 0x40);
		}
		break;
	case MEFFECT_MONO:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x03);
			HI704_write_cmos_sensor(0x44, 0x80);
			HI704_write_cmos_sensor(0x45, 0x80);
		}
		break;
	default:
		return KAL_FALSE;
	}

	return KAL_TRUE;
}				/* HI704_set_param_effect */

/*************************************************************************
* FUNCTION
*	HI704_set_param_banding
*
* DESCRIPTION
*	banding setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_banding(UINT16 para)
{
	SENSORDB("[Enter]HI704 set_param_banding func:para = %d\n", para);

	if (HI704_sensor.banding == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.banding = para;
	spin_unlock(&hi704_yuv_drv_lock);

	switch (para) {
	case AE_FLICKER_MODE_50HZ:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x10, 0x9c);
		}
		break;
	case AE_FLICKER_MODE_60HZ:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x10, 0x8c);
		}
		break;
	default:
		return KAL_FALSE;
	}

	return KAL_TRUE;
}				/* HI704_set_param_banding */

/*************************************************************************
* FUNCTION
*	HI704_set_param_exposure
*
* DESCRIPTION
*	exposure setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_exposure(UINT16 para)
{
	SENSORDB("[Enter]HI704 set_param_exposure func:para = %d\n", para);

	if (HI704_sensor.exposure == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.exposure = para;
	spin_unlock(&hi704_yuv_drv_lock);

	HI704_write_cmos_sensor(0x03, 0x10);
	HI704_write_cmos_sensor(0x12, HI704_read_cmos_sensor(0x12) | 0x10);
	switch (para) {
	case AE_EV_COMP_13:	/*+4 EV */
		HI704_write_cmos_sensor(0x40, 0x60);
		break;
	case AE_EV_COMP_10:	/*+3 EV */
		HI704_write_cmos_sensor(0x40, 0x48);
		break;
	case AE_EV_COMP_07:	/*+2 EV */
		HI704_write_cmos_sensor(0x40, 0x30);
		break;
	case AE_EV_COMP_03:	/* +1 EV */
		HI704_write_cmos_sensor(0x40, 0x18);
		break;
	case AE_EV_COMP_00:	/* +0 EV */
		HI704_write_cmos_sensor(0x40, 0x80);
		break;
	case AE_EV_COMP_n03:
			    /*-1 EV*/
		HI704_write_cmos_sensor(0x40, 0x98);
		break;
	case AE_EV_COMP_n07:
			    /*-2 EV */
		HI704_write_cmos_sensor(0x40, 0xb0);
		break;
	case AE_EV_COMP_n10:
			    /*-3 EV*/
		HI704_write_cmos_sensor(0x40, 0xc8);
		break;
	case AE_EV_COMP_n13:
			    /*-4 EV*/
		HI704_write_cmos_sensor(0x40, 0xe0);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}				/* HI704_set_param_exposure */

void HI704_set_AE_mode(UINT32 iPara)
{
	UINT8 temp_AE_reg = 0;
	SENSORDB("HI704_set_AE_mode = %d E \n", iPara);
	HI704_write_cmos_sensor(0x03, 0x20);
	temp_AE_reg = HI704_read_cmos_sensor(0x10);

	if (AE_MODE_OFF == iPara) {
		/* turn off AEC/AGC */
		HI704_write_cmos_sensor(0x10, temp_AE_reg & ~0x10);
	} else {
		HI704_write_cmos_sensor(0x10, temp_AE_reg | 0x10);
	}
}

UINT32 HI704YUVSensorSetting(FEATURE_ID iCmd, UINT32 iPara)
{
	SENSORDB("[Enter]HI704YUVSensorSetting func:cmd = %d\n", iCmd);

	switch (iCmd) {
	case FID_SCENE_MODE:
		if (iPara == SCENE_MODE_OFF) {
			HI704_night_mode(FALSE);
		} else if (iPara == SCENE_MODE_NIGHTSCENE) {
			HI704_night_mode(TRUE);
		}
		break;
	case FID_AWB_MODE:
		HI704_set_param_wb(iPara);
		break;
	case FID_COLOR_EFFECT:
		HI704_set_param_effect(iPara);
		break;
	case FID_AE_EV:
		HI704_set_param_exposure(iPara);
		break;
	case FID_AE_FLICKER:
		HI704_set_param_banding(iPara);
		break;
	case FID_ZOOM_FACTOR:
		spin_lock(&hi704_yuv_drv_lock);
		HI704_zoom_factor = iPara;
		spin_unlock(&hi704_yuv_drv_lock);
		break;
	case FID_AE_SCENE_MODE:
		HI704_set_AE_mode(iPara);
		break;
	default:
		break;
	}
	return TRUE;
}				/* HI704YUVSensorSetting */

UINT32 HI704YUVSetVideoMode(UINT16 u2FrameRate)
{
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.MPEG4_Video_mode = KAL_TRUE;
	spin_unlock(&hi704_yuv_drv_lock);
	SENSORDB("[Enter]HI704 Set Video Mode:FrameRate= %d\n", u2FrameRate);
	SENSORDB("HI704_sensor.video_mode = %d\n",
		 HI704_sensor.MPEG4_Video_mode);

	if (u2FrameRate == 30)
		u2FrameRate = 20;

	spin_lock(&hi704_yuv_drv_lock);
	/*HI704_sensor.fix_framerate = u2FrameRate * 10; */
	spin_unlock(&hi704_yuv_drv_lock);

	if (HI704_sensor.fix_framerate <= 300) {
		/*HI704_Fix_Video_Frame_Rate(HI704_sensor.fix_framerate); */
	} else {
		SENSORDB("Wrong Frame Rate");
	}

	return TRUE;
}

void HI704GetAFMaxNumFocusAreas(UINT32 * pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
	SENSORDB("HI704GetAFMaxNumFocusAreas *pFeatureReturnPara32 = %d\n",
		 *pFeatureReturnPara32);
}

void HI704GetAEMaxNumMeteringAreas(UINT32 * pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
	SENSORDB("HI704GetAEMaxNumMeteringAreas *pFeatureReturnPara32 = %d\n",
		 *pFeatureReturnPara32);
}

void HI704GetExifInfo(UINT32 exifAddr)
{
	HI704_Read_Shutter();
	SENSOR_EXIF_INFO_STRUCT *pExifInfo =
	    (SENSOR_EXIF_INFO_STRUCT *) exifAddr;

	//pExifInfo->RealISOValue = GC0312ExifInfo.RealISOValue;
	//pExifInfo->AWBMode = GC0312ExifInfo.AWBMode;
	pExifInfo->RealISOValue = HI704ExifInfo.RealISOValue;
	pExifInfo->CapExposureTime = HI704ExifInfo.CapExposureTime * 1000;

	printk("HYNIXExifInfo.RealISOValue is %d", HI704ExifInfo.RealISOValue);
	printk("HYNIXExifInfo.CapExposureTime is %d",
	       HI704ExifInfo.CapExposureTime);
}

UINT32 HI704FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
			   UINT8 * pFeaturePara, UINT32 * pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData =
	    (MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData =
	    (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ = HI704_IMAGE_SENSOR_FULL_WIDTH;
		*pFeatureReturnPara16 = HI704_IMAGE_SENSOR_FULL_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*pFeatureReturnPara16++ = HI704_IMAGE_SENSOR_PV_WIDTH;
		*pFeatureReturnPara16 = HI704_IMAGE_SENSOR_PV_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		/*pFeatureReturnPara32 = HI704_sensor_pclk/10; */
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:

		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		HI704_night_mode((BOOL) * pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		HI704_write_cmos_sensor(pSensorRegData->RegAddr,
					pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		pSensorRegData->RegData =
		    HI704_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		memcpy(pSensorConfigData, &HI704SensorConfigData,
		       sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_GET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_GROUP_INFO:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
		break;
	case SENSOR_FEATURE_GET_GROUP_COUNT:
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		HI704SetTestPatternMode((BOOL) * pFeatureData16);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_YUV_CMD:
		HI704YUVSensorSetting((FEATURE_ID) * pFeatureData32,
				      *(pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		HI704YUVSetVideoMode(*pFeatureData16);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		HI704_GetSensorID(pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
		HI704GetAFMaxNumFocusAreas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
		HI704GetAEMaxNumMeteringAreas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_EXIF_INFO:
		SENSORDB("SENSOR_FEATURE_GET_EXIF_INFO\n");
		SENSORDB("EXIF addr = 0x%x\n", *pFeatureData32);
		HI704GetExifInfo(*pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*pFeatureReturnPara32 = HI704_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;
	default:
		break;
	}
	return ERROR_NONE;
}				/* HI704FeatureControl() */

SENSOR_FUNCTION_STRUCT SensorFuncHI704 = {
	HI704Open,
	HI704GetInfo,
	HI704GetResolution,
	HI704FeatureControl,
	HI704Control,
	HI704Close
};

UINT32 HI704_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT * pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncHI704;

	return ERROR_NONE;
}				/* SensorInit() */
