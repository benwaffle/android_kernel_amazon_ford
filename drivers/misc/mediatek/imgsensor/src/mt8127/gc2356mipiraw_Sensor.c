/*******************************************************************************************/

/*******************************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <asm/system.h>

#include <linux/proc_fs.h>

#include <linux/dma-mapping.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc2356mipiraw_Sensor.h"
#include "gc2356mipiraw_Camera_Sensor_para.h"
#include "gc2356mipiraw_CameraCustomized.h"
static DEFINE_SPINLOCK(gc2356mipiraw_drv_lock);

#define GC2356_DEBUG
/*#define GC2356_DEBUG_SOFIA*/
#define GC2356_TEST_PATTERN_CHECKSUM (0x9d1c9dad)	/*(0x5593c632)*/

#ifdef GC2356_DEBUG
#define GC2356DB(fmt, arg...) pr_debug("[GC2356mipiraw][%s] " fmt, __func__, ##arg)
#else
#define GC2356DB(fmt, arg...)
#endif

#ifdef GC2356_DEBUG_SOFIA
#define GC2356DBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[GC2356Raw] ",  fmt, ##arg)
#else
#define GC2356DBSOFIA(fmt, arg...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 GC2356_FeatureControl_PERIOD_PixelNum = GC2356_PV_PERIOD_PIXEL_NUMS;
kal_uint32 GC2356_FeatureControl_PERIOD_LineNum = GC2356_PV_PERIOD_LINE_NUMS;
UINT16 gc2356VIDEO_MODE_TARGET_FPS = 30;
MSDK_SENSOR_CONFIG_STRUCT GC2356SensorConfigData;
MSDK_SCENARIO_ID_ENUM GC2356CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/*FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT GC2356SensorCCT[] = CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT GC2356SensorReg[ENGINEER_END] =
    CAMERA_SENSOR_REG_DEFAULT_VALUE;
/*FIXME: old factors and DIDNOT use now. e*/

static GC2356_PARA_STRUCT gc2356;

#define Sleep(ms) mdelay(ms)
#define GC2356_ORIENTATION IMAGE_NORMAL	/*IMAGE_NORMAL*/

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData,
		       u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

/*************************************************************************
* FUNCTION
*    GC2356MIPI_write_cmos_sensor
*
* DESCRIPTION
*    This function wirte data to CMOS sensor through I2C
*
* PARAMETERS
*    addr: the 16bit address of register
*    para: the 8bit value of register
*
* RETURNS
*    None
*
* LOCAL AFFECTED
*
*************************************************************************/
static void GC2356_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	kal_uint8 out_buff[2];

	out_buff[0] = addr;
	out_buff[1] = para;

	iWriteRegI2C((u8 *) out_buff, 2, GC2356MIPI_WRITE_ID);
}

/*************************************************************************
* FUNCTION
*    GC2035_read_cmos_sensor
*
* DESCRIPTION
*    This function read data from CMOS sensor through I2C.
*
* PARAMETERS
*    addr: the 16bit address of register
*
* RETURNS
*    8bit data read through I2C
*
* LOCAL AFFECTED
*
*************************************************************************/
static kal_uint8 GC2356_read_cmos_sensor(kal_uint8 addr)
{
	kal_uint8 in_buff[1] = { 0xFF };
	kal_uint8 out_buff[1];

	out_buff[0] = addr;

	if (0 !=
	    iReadRegI2C((u8 *) out_buff, 1, (u8 *) in_buff, 1,
			GC2356MIPI_WRITE_ID)) {
		GC2356DB("ERROR: GC2356MIPI_read_cmos_sensor \n");
	}
	return in_buff[0];
}

void GC2356_SetGain(UINT16 iGain)
{

#define ANALOG_GAIN_1 64	/*1.00x*/
#define ANALOG_GAIN_2 88	/*1.37x*/
#define ANALOG_GAIN_3 121	/*1.89x*/
#define ANALOG_GAIN_4 168	/*2.59x*/
#define ANALOG_GAIN_5 239	/*3.70x*/
#define ANALOG_GAIN_6 330	/*5.06x*/
#define ANALOG_GAIN_7 470	/*7.15x*/

	kal_uint16 iReg, temp, temp1;

	GC2356DB("GC2356MIPI_SetGain iGain = %d \n", iGain);

	GC2356_write_cmos_sensor(0xb1, 0x01);
	GC2356_write_cmos_sensor(0xb2, 0x00);

	iReg = iGain;
	/*digital gain*/
	/*GC2356MIPI_write_cmos_sensor(0xb1, iReg>>6);*/
	/*GC2356MIPI_write_cmos_sensor(0xb2, (iReg<<2)&0xfc);*/

	if (iReg < 0x40)
		iReg = 0x40;
	if (iReg > 512)
		iReg = 512;

	if ((ANALOG_GAIN_1 <= iReg) && (iReg < ANALOG_GAIN_2)) {
		/*analog gain*/
		GC2356_write_cmos_sensor(0xb6, 0x00);
		temp = iReg;
		GC2356_write_cmos_sensor(0xb1, temp >> 6);
		GC2356_write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		GC2356DB("GC2356 analogic gain 1x , GC2356 add pregain = %d\n",
			 temp);

	} else if ((ANALOG_GAIN_2 <= iReg) && (iReg < ANALOG_GAIN_3)) {
		/*analog gain*/
		GC2356_write_cmos_sensor(0xb6, 0x01);
		temp = 64 * iReg / ANALOG_GAIN_2;
		GC2356_write_cmos_sensor(0xb1, temp >> 6);
		GC2356_write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		GC2356DB
		    ("GC2356 analogic gain 1.45x , GC2356 add pregain = %d\n",
		     temp);

	}

	else if ((ANALOG_GAIN_3 <= iReg) && (iReg < ANALOG_GAIN_4)) {
		/*analog gain*/
		GC2356_write_cmos_sensor(0xb6, 0x02);
		temp = 64 * iReg / ANALOG_GAIN_3;
		GC2356_write_cmos_sensor(0xb1, temp >> 6);
		GC2356_write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		GC2356DB
		    ("GC2356 analogic gain 2.02x , GC2356 add pregain = %d\n",
		     temp);
	}
/*else if(ANALOG_GAIN_4<= iReg)*/
	else {
		/*analog gain*/
		GC2356_write_cmos_sensor(0xb6, 0x03);
		temp = 64 * iReg / ANALOG_GAIN_4;
		GC2356_write_cmos_sensor(0xb1, temp >> 6);
		GC2356_write_cmos_sensor(0xb2, (temp << 2) & 0xfc);

		GC2356DB("GC2356 analogic gain 2.8x, temp = %d\n", temp);
	}

}				/*GC2356_SetGain_SetGain*/

void GC2356_camera_para_to_sensor(void)
{
}

void GC2356_sensor_to_camera_para(void)
{
}

kal_int32 GC2356_get_sensor_group_count(void)
{
	return GROUP_TOTAL_NUMS;
}

void GC2356_get_sensor_group_info(kal_uint16 group_idx,
				  kal_int8 *group_name_ptr,
				  kal_int32 *item_count_ptr)
{
}

void GC2356_get_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx,
				 MSDK_SENSOR_ITEM_INFO_STRUCT *info_ptr)
{
}

kal_bool GC2356_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx,
				     kal_int32 ItemValue)
{
	return KAL_TRUE;
}

static void GC2356_SetDummy(const kal_uint32 iPixels, const kal_uint32 iLines)
{
	kal_uint32 hb = 0;
	kal_uint32 vb = 0;

	if (SENSOR_MODE_PREVIEW == gc2356.sensorMode) {	/*SXGA size output*/
		hb = GC2356_DEFAULT_DUMMY_PIXEL_NUMS + iPixels;
		vb = GC2356_DEFAULT_DUMMY_LINE_NUMS + iLines;
	} else if (SENSOR_MODE_VIDEO == gc2356.sensorMode) {
		hb = GC2356_DEFAULT_DUMMY_PIXEL_NUMS + iPixels;
		vb = GC2356_DEFAULT_DUMMY_LINE_NUMS + iLines;
	} else {
		hb = GC2356_DEFAULT_DUMMY_PIXEL_NUMS + iPixels;
		vb = GC2356_DEFAULT_DUMMY_LINE_NUMS + iLines;
	}

	/*if(gc2356.maxExposureLines > frame_length -4 )*/
	/*return;*/

	/*ASSERT(line_length < GC2356_MAX_LINE_LENGTH);   0xCCCC*/
	/*ASSERT(frame_length < GC2356_MAX_FRAME_LENGTH)  0xFFFF*/

	/*Set HB*/
	GC2356_write_cmos_sensor(0x05, (hb >> 8) & 0xFF);
	GC2356_write_cmos_sensor(0x06, hb & 0xFF);

	/*Set VB*/
	GC2356_write_cmos_sensor(0x07, (vb >> 8) & 0xFF);
	GC2356_write_cmos_sensor(0x08, vb & 0xFF);
	GC2356DB("GC2356_SetDummy framelength=%d\n",
		 vb + GC2356_VALID_LINE_NUMS);

}				/*GC2356_SetDummy*/

static void GC2356_Sensor_Init(void)
{
	/* SYS */
	GC2356_write_cmos_sensor(0xfe, 0x80);
	GC2356_write_cmos_sensor(0xfe, 0x80);
	GC2356_write_cmos_sensor(0xfe, 0x80);
	GC2356_write_cmos_sensor(0xf2, 0x00);	/*sync_pad_io_ebi*/
	GC2356_write_cmos_sensor(0xf6, 0x00);	/*up down*/
	GC2356_write_cmos_sensor(0xf7, 0x31);	/*19 pll enable*/
	GC2356_write_cmos_sensor(0xf8, 0x06);	/*Pll mode 2 86--reduce frequency in dark*/
	GC2356_write_cmos_sensor(0xf9, 0x0e);	/*de[0] pll enable*/
	GC2356_write_cmos_sensor(0xfa, 0x00);	/*div*/
	GC2356_write_cmos_sensor(0xfc, 0x06);	/*4e*/
	/*GC2356_write_cmos_sensor(0xfd,0x00);*/
	GC2356_write_cmos_sensor(0xfe, 0x00);

	/* ANALOG & CISCTL */
	/*AEC&frame length*/
	GC2356_write_cmos_sensor(0x03, 0x04);	/*0a -> 04*/
	GC2356_write_cmos_sensor(0x04, 0xb0);	/*41 -> b0*/
	GC2356_write_cmos_sensor(0x05, 0x01);	/*max 30fps  03*/
	GC2356_write_cmos_sensor(0x06, 0x1c);
	GC2356_write_cmos_sensor(0x07, 0x00);
	GC2356_write_cmos_sensor(0x08, 0x0e);	/*22*/
	GC2356_write_cmos_sensor(0x0a, 0x00);	/*row start*/
	GC2356_write_cmos_sensor(0x0c, 0x04);	/*0c col start*/
	GC2356_write_cmos_sensor(0x0d, 0x04);
	GC2356_write_cmos_sensor(0x0e, 0xc0);	/*c0*/
	GC2356_write_cmos_sensor(0x0f, 0x06);
	GC2356_write_cmos_sensor(0x10, 0x50);	/*Window setting 1616x1216*/
	/*GC2356_write_cmos_sensor(0x11,0x00);
	   GC2356_write_cmos_sensor(0x12,0x18);
	   GC2356_write_cmos_sensor(0x13,0x11);
	   GC2356_write_cmos_sensor(0x14,0x01);
	   GC2356_write_cmos_sensor(0x15,0x00);
	   GC2356_write_cmos_sensor(0x16,0xc1); */
	GC2356_write_cmos_sensor(0x17, 0x17);	/*14*/
	GC2356_write_cmos_sensor(0x19, 0x0b);
	GC2356_write_cmos_sensor(0x1b, 0x49);	/*48*/
	GC2356_write_cmos_sensor(0x1c, 0x12);
	GC2356_write_cmos_sensor(0x1d, 0x10);	/*double reset*/
	GC2356_write_cmos_sensor(0x1e, 0xbc);	/*a8 col_r/rowclk_mode/rsthigh_en FPN*/
	GC2356_write_cmos_sensor(0x1f, 0xc8);	/*c8 rsgl_s_mode/vpix_s_mode 灯管横条纹*/
	GC2356_write_cmos_sensor(0x20, 0x71);
	GC2356_write_cmos_sensor(0x21, 0x20);	/*rsg 灯管横条纹   40*/
	GC2356_write_cmos_sensor(0x22, 0xa0);	/*e0       80  f0*/
	GC2356_write_cmos_sensor(0x23, 0x51);	/*51*/
	GC2356_write_cmos_sensor(0x24, 0x19);	/*0b 55*/
	/*GC2356_write_cmos_sensor(0x25,0x10);*/  /*10*/
	/*GC2356_write_cmos_sensor(0x26,0x00);*/
	GC2356_write_cmos_sensor(0x27, 0x20);
	GC2356_write_cmos_sensor(0x28, 0x00);	/*10*/
	/*GC2356_write_cmos_sensor(0x29,0x00);*/
	/*GC2356_write_cmos_sensor(0x2a,0x00);*/
	GC2356_write_cmos_sensor(0x2b, 0x81);	/*00 sf_s_mode FPN*/
	GC2356_write_cmos_sensor(0x2c, 0x38);	/*50 5c ispg FPN*/
	/*GC2356_write_cmos_sensor(0x2d,0x15);*/
	GC2356_write_cmos_sensor(0x2e, 0x16);	/*05 eq width*/
	GC2356_write_cmos_sensor(0x2f, 0x14);	/*[3:0]tx_width 写0能改麻点*/
	GC2356_write_cmos_sensor(0x30, 0x00);
	GC2356_write_cmos_sensor(0x31, 0x01);
	GC2356_write_cmos_sensor(0x32, 0x02);
	GC2356_write_cmos_sensor(0x33, 0x03);
	GC2356_write_cmos_sensor(0x34, 0x07);
	GC2356_write_cmos_sensor(0x35, 0x0b);
	GC2356_write_cmos_sensor(0x36, 0x0f);

	/* gain */
	GC2356_write_cmos_sensor(0xb0, 0x50);
	GC2356_write_cmos_sensor(0xb1, 0x01);
	GC2356_write_cmos_sensor(0xb2, 0x00);
	GC2356_write_cmos_sensor(0xb3, 0x40);
	GC2356_write_cmos_sensor(0xb4, 0x40);
	GC2356_write_cmos_sensor(0xb5, 0x40);
	GC2356_write_cmos_sensor(0xb6, 0x00);

	/* crop */
	GC2356_write_cmos_sensor(0x92, 0x02);
	GC2356_write_cmos_sensor(0x95, 0x04);
	GC2356_write_cmos_sensor(0x96, 0xb0);
	GC2356_write_cmos_sensor(0x97, 0x06);
	GC2356_write_cmos_sensor(0x98, 0x40);	/*out window set 1600x1200*/

	/*      BLK  */
	GC2356_write_cmos_sensor(0x18, 0x02);	/*1a-lily*/
	GC2356_write_cmos_sensor(0x1a, 0x01);	/*09-lily 01*/
	GC2356_write_cmos_sensor(0x40, 0x42);	/*43-lily*/
	GC2356_write_cmos_sensor(0x4e, 0x3c);	/*BLK select*/
	GC2356_write_cmos_sensor(0x4f, 0x00);
	GC2356_write_cmos_sensor(0x5e, 0x00);	/*18-lily offset ratio*/
	GC2356_write_cmos_sensor(0x66, 0x20);	/*20-lily dark ratio*/
	GC2356_write_cmos_sensor(0x6a, 0x00);	/*39*/
	GC2356_write_cmos_sensor(0x6b, 0x00);
	GC2356_write_cmos_sensor(0x6c, 0x00);
	GC2356_write_cmos_sensor(0x6d, 0x00);
	GC2356_write_cmos_sensor(0x6e, 0x00);
	GC2356_write_cmos_sensor(0x6f, 0x00);
	GC2356_write_cmos_sensor(0x70, 0x00);
	GC2356_write_cmos_sensor(0x71, 0x00);	/*manual offset*/

	/* Dark sun */
	GC2356_write_cmos_sensor(0x87, 0x03);
	GC2356_write_cmos_sensor(0xe0, 0xe7);	/*dark sun en/extend mode*/
	GC2356_write_cmos_sensor(0xe2, 0x03);
	GC2356_write_cmos_sensor(0xe3, 0xc0);	/*clamp*/

	 /*MIPI*/ GC2356_write_cmos_sensor(0xfe, 0x03);
	GC2356_write_cmos_sensor(0x01, 0x87);
	GC2356_write_cmos_sensor(0x02, 0x11);
	GC2356_write_cmos_sensor(0x03, 0x91);
	GC2356_write_cmos_sensor(0x04, 0x01);
	GC2356_write_cmos_sensor(0x05, 0x00);
	GC2356_write_cmos_sensor(0x06, 0xa2);
	GC2356_write_cmos_sensor(0x10, 0x91);
	GC2356_write_cmos_sensor(0x11, 0x2b);
	GC2356_write_cmos_sensor(0x15, 0x60);
	GC2356_write_cmos_sensor(0x12, 0xd0);
	GC2356_write_cmos_sensor(0x13, 0x07);
	GC2356_write_cmos_sensor(0x21, 0x10);
	GC2356_write_cmos_sensor(0x22, 0x03);
	GC2356_write_cmos_sensor(0x23, 0x20);
	GC2356_write_cmos_sensor(0x24, 0x02);
	GC2356_write_cmos_sensor(0x25, 0x10);
	GC2356_write_cmos_sensor(0x26, 0x08);
	GC2356_write_cmos_sensor(0x29, 0x02);
	GC2356_write_cmos_sensor(0x2a, 0x0a);
	GC2356_write_cmos_sensor(0x2b, 0x08);
	GC2356_write_cmos_sensor(0x40, 0x00);
	GC2356_write_cmos_sensor(0x41, 0x00);
	GC2356_write_cmos_sensor(0x42, 0x40);
	GC2356_write_cmos_sensor(0x43, 0x06);
	GC2356_write_cmos_sensor(0xfe, 0x00);

}				/*  GC2356_Sensor_Init  */

UINT32 GC2356Open(void)
{
	volatile signed int i;
	kal_uint16 sensor_id = 0;
	GC2356DB("GC2356Open enter :\n ");
	sensor_id =
	    ((GC2356_read_cmos_sensor(0xf0) << 8) |
	     GC2356_read_cmos_sensor(0xf1));
	GC2356DB("GC2356MIPIGetSensorID:%x \n", sensor_id);
	spin_lock(&gc2356mipiraw_drv_lock);
	gc2356.sensorMode = SENSOR_MODE_INIT;
	gc2356.GC2356AutoFlickerMode = KAL_FALSE;
	gc2356.GC2356VideoMode = KAL_FALSE;
	gc2356.shutter = 0xff;
	gc2356.Gain = 64;
	spin_unlock(&gc2356mipiraw_drv_lock);
	GC2356_Sensor_Init();

	GC2356DB("GC2356Open exit :\n ");

	return ERROR_NONE;
}

extern IMM_auxadc_GetOneChannelValue_Cali(int Channel, int *voltage);
UINT32 GC2356GetSensorID(UINT32 *sensorID)
{
	int retry = 1;
	int adc_val = 0;

	GC2356DB("GC2356GetSensorID enter :\n ");

	/*check if sensor ID correct*/
	*sensorID =
	    ((GC2356_read_cmos_sensor(0xf0) << 8) |
	     GC2356_read_cmos_sensor(0xf1));
	IMM_auxadc_GetOneChannelValue_Cali(12, &adc_val);
	if ((*sensorID == GC2355MIPI_SENSOR_ID) && (adc_val > 100000)) {	/*0.1v = 100000uv */
		*sensorID = GC2356MIPI_SENSOR_ID;
	}
	GC2356DB("[%s,%d]:  adc_val = %d  \n", __FUNCTION__, __LINE__, adc_val);
	GC2356DB("GC2356MIPIGetSensorID:%x \n", *sensorID);

	if (*sensorID != GC2356MIPI_SENSOR_ID) {
		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

void GC2356_SetShutter(kal_uint32 iShutter)
{
	/*kal_uint8 i=0;*/
	if (MSDK_SCENARIO_ID_CAMERA_ZSD == GC2356CurrentScenarioId) {
		/*GC2356DB("always UPDATE SHUTTER when gc2356.sensorMode == SENSOR_MODE_CAPTURE\n");*/
	} else {
		if (gc2356.sensorMode == SENSOR_MODE_CAPTURE) {
			/*GC2356DB("capture!!DONT UPDATE SHUTTER!!\n");*/
			/*return;*/
		}
	}
	if (gc2356.shutter == iShutter) {
	/*return;*/
    }
	spin_lock(&gc2356mipiraw_drv_lock);
	gc2356.shutter = iShutter;

	spin_unlock(&gc2356mipiraw_drv_lock);
	if (gc2356.shutter > 16383)
		gc2356.shutter = 16383;
	if (gc2356.shutter < 12)
		gc2356.shutter = 12;
	GC2356_write_cmos_sensor(0x03, (gc2356.shutter >> 8) & 0x3F);
	GC2356_write_cmos_sensor(0x04, gc2356.shutter & 0xFF);

	GC2356DB("GC2356_SetShutter:%x \n", iShutter);

	/*GC2356DB("GC2356_SetShutter 0x03=%x, 0x04=%x \n",GC2356_read_cmos_sensor(0x03),GC2356_read_cmos_sensor(0x04));*/
	return;
}				/*  GC2356_SetShutter   */

UINT32 GC2356_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2;
	UINT32 shutter = 0;
	temp_reg1 = GC2356_read_cmos_sensor(0x03);	/*AEC[b19~b16]*/
	temp_reg2 = GC2356_read_cmos_sensor(0x04);	/*AEC[b15~b8]*/
	shutter = ((temp_reg1 << 8) | (temp_reg2));

	return shutter;
}

void GC2356_NightMode(kal_bool bEnable)
{
}

UINT32 GC2356Close(void)
{
	return ERROR_NONE;
}

void GC2356SetFlipMirror(kal_int32 imgMirror)
{
	kal_int16 mirror = 0, flip = 0;

	switch (imgMirror) {
	case IMAGE_NORMAL:	/*IMAGE_NORMAL:*/
		GC2356_write_cmos_sensor(0x17, 0x14);	/*bit[1][0]*/
		GC2356_write_cmos_sensor(0x92, 0x03);
		GC2356_write_cmos_sensor(0x94, 0x0b);
		break;
	case IMAGE_H_MIRROR:
		GC2356_write_cmos_sensor(0x17, 0x15);
		break;
	case IMAGE_V_MIRROR:
		GC2356_write_cmos_sensor(0x17, 0x16);
		break;
	case IMAGE_HV_MIRROR:
		GC2356_write_cmos_sensor(0x17, 0x17);
		break;
	}
}

UINT32 GC2356Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	GC2356DB("GC2356Preview enter:");

	spin_lock(&gc2356mipiraw_drv_lock);
	gc2356.sensorMode = SENSOR_MODE_PREVIEW;	/*Need set preview setting after capture mode*/
	gc2356.DummyPixels = 0;	/*define dummy pixels and lines*/
	gc2356.DummyLines = 0;
	GC2356_FeatureControl_PERIOD_PixelNum =
	    GC2356_PV_PERIOD_PIXEL_NUMS + gc2356.DummyPixels;
	GC2356_FeatureControl_PERIOD_LineNum =
	    GC2356_PV_PERIOD_LINE_NUMS + gc2356.DummyLines;
	spin_unlock(&gc2356mipiraw_drv_lock);

	/*GC2356_write_shutter(gc2356.shutter);*/
	/*write_GC2356_gain(gc2356.pvGain);*/
	/*set mirror & flip*/
	/*GC2356DB("[GC2356Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);*/
	spin_lock(&gc2356mipiraw_drv_lock);
	gc2356.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&gc2356mipiraw_drv_lock);
	/*if(SENSOR_MODE_PREVIEW==gc2356.sensorMode ) return ERROR_NONE;*/

	GC2356DB("GC2356Preview mirror:%d\n",
		 sensor_config_data->SensorImageMirror);
	GC2356SetFlipMirror(GC2356_ORIENTATION);
	GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);
	GC2356DB("GC2356Preview exit: \n");
	mdelay(100);
	return ERROR_NONE;
}				/* GC2356Preview() */

UINT32 GC2356Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	GC2356DB("GC2356Video enter:");
	if (gc2356.sensorMode == SENSOR_MODE_VIDEO) {
	} else
		/*GC2356VideoSetting();*/

		spin_lock(&gc2356mipiraw_drv_lock);
	gc2356.sensorMode = SENSOR_MODE_VIDEO;
	GC2356_FeatureControl_PERIOD_PixelNum =
	    GC2356_VIDEO_PERIOD_PIXEL_NUMS + gc2356.DummyPixels;
	GC2356_FeatureControl_PERIOD_LineNum =
	    GC2356_VIDEO_PERIOD_LINE_NUMS + gc2356.DummyLines;
/*
	spin_unlock(&gc2356mipiraw_drv_lock);
	spin_lock(&gc2356mipiraw_drv_lock);
*/
	gc2356.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&gc2356mipiraw_drv_lock);
	GC2356SetFlipMirror(GC2356_ORIENTATION);
	GC2356DB("GC2356Video mirror:%d\n",
		 sensor_config_data->SensorImageMirror);
	GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);
	/*GC2356DBSOFIA("[GC2356Video]frame_len=%x\n", ((GC2356_read_cmos_sensor(0x380e)<<8)+GC2356_read_cmos_sensor(0x380f)));*/
	mdelay(100);
	GC2356DB("GC2356Video exit:\n");
	return ERROR_NONE;
}

UINT32 GC2356Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 shutter = gc2356.shutter;
	kal_uint32 temp_data;
	if (SENSOR_MODE_CAPTURE == gc2356.sensorMode) {
		GC2356DB("GC2356Capture BusrtShot!!!\n");
	} else {
		GC2356DB("GC2356Capture enter:\n");

		spin_lock(&gc2356mipiraw_drv_lock);
		gc2356.pvShutter = shutter;
		gc2356.sensorGlobalGain = temp_data;
		gc2356.pvGain = gc2356.sensorGlobalGain;
		spin_unlock(&gc2356mipiraw_drv_lock);

		GC2356DB
		    ("[GC2356Capture]gc2356.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",
		     gc2356.shutter, shutter, gc2356.sensorGlobalGain);


		spin_lock(&gc2356mipiraw_drv_lock);
		gc2356.sensorMode = SENSOR_MODE_CAPTURE;
		gc2356.imgMirror = sensor_config_data->SensorImageMirror;
		gc2356.DummyPixels = 0;/*define dummy pixels and lines*/
		gc2356.DummyLines = 0;
		GC2356_FeatureControl_PERIOD_PixelNum =
		    GC2356_FULL_PERIOD_PIXEL_NUMS + gc2356.DummyPixels;
		GC2356_FeatureControl_PERIOD_LineNum =
		    GC2356_FULL_PERIOD_LINE_NUMS + gc2356.DummyLines;
		spin_unlock(&gc2356mipiraw_drv_lock);

		/*GC2356DB("[GC2356Capture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);*/

		GC2356DB("GC2356capture mirror:%d\n",
			 sensor_config_data->SensorImageMirror);
		GC2356SetFlipMirror(GC2356_ORIENTATION);
		GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);

		if (GC2356CurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD) {
			GC2356DB("GC2356Capture exit ZSD!!\n");
			return ERROR_NONE;
		}
		GC2356DB("GC2356Capture exit:\n");
	}

	return ERROR_NONE;
}				/* GC2356Capture() */

UINT32 GC2356GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *
			   pSensorResolution)
{

	GC2356DB("GC2356GetResolution!!\n");
	pSensorResolution->SensorPreviewWidth = GC2356_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight = GC2356_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorFullWidth = GC2356_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight = GC2356_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorVideoWidth = GC2356_IMAGE_SENSOR_VIDEO_WIDTH;
	pSensorResolution->SensorVideoHeight = GC2356_IMAGE_SENSOR_VIDEO_HEIGHT;
	return ERROR_NONE;
}				/* GC2356GetResolution() */

UINT32 GC2356GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		     MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		     MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	pSensorInfo->SensorPreviewResolutionX = GC2356_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY = GC2356_IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX = GC2356_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY = GC2356_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&gc2356mipiraw_drv_lock);
	gc2356.imgMirror = pSensorConfigData->SensorImageMirror;
	spin_unlock(&gc2356mipiraw_drv_lock);

	pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_B;

	pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
	pSensorInfo->MIPIsensorType = MIPI_OPHY_CSI2;

	pSensorInfo->CaptureDelayFrame = 4;
	pSensorInfo->PreviewDelayFrame = 3;
	pSensorInfo->VideoDelayFrame = 3;

	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
	pSensorInfo->AEShutDelayFrame = 0;	/* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 0;	/* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 2;

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockRisingCount = 0;

		pSensorInfo->SensorGrabStartX = GC2356_PV_X_START;
		pSensorInfo->SensorGrabStartY = GC2356_PV_Y_START;

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockRisingCount = 0;

		pSensorInfo->SensorGrabStartX = GC2356_VIDEO_X_START;
		pSensorInfo->SensorGrabStartY = GC2356_VIDEO_Y_START;

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockRisingCount = 0;

		pSensorInfo->SensorGrabStartX = GC2356_FULL_X_START;	/*2*GC2356_IMAGE_SENSOR_PV_STARTX;*/
		pSensorInfo->SensorGrabStartY = GC2356_FULL_Y_START;	/*2*GC2356_IMAGE_SENSOR_PV_STARTY;*/

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	default:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockRisingCount = 0;

		pSensorInfo->SensorGrabStartX = GC2356_PV_X_START;
		pSensorInfo->SensorGrabStartY = GC2356_PV_Y_START;

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	}

	memcpy(pSensorConfigData, &GC2356SensorConfigData,
	       sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	return ERROR_NONE;
}				/* GC2356GetInfo() */

UINT32 GC2356Control(MSDK_SCENARIO_ID_ENUM ScenarioId,
		     MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		     MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	spin_lock(&gc2356mipiraw_drv_lock);
	GC2356CurrentScenarioId = ScenarioId;
	spin_unlock(&gc2356mipiraw_drv_lock);
	GC2356DB("GC2356CurrentScenarioId=%d\n", GC2356CurrentScenarioId);
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		GC2356Preview(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		GC2356Capture(pImageWindow, pSensorConfigData);
		break;
	default:
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* GC2356Control() */

UINT32 GC2356SetVideoMode(UINT16 u2FrameRate)
{
	kal_uint32 MIN_Frame_length = 0, extralines = 0;
	GC2356DB("[GC2356SetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&gc2356mipiraw_drv_lock);
	gc2356VIDEO_MODE_TARGET_FPS = u2FrameRate;
	spin_unlock(&gc2356mipiraw_drv_lock);

	if (u2FrameRate == 0) {
		GC2356DB("Disable Video Mode or dynimac fps\n");
		spin_lock(&gc2356mipiraw_drv_lock);
		gc2356.DummyPixels = 0;
		gc2356.DummyLines = extralines;
		spin_unlock(&gc2356mipiraw_drv_lock);
		return KAL_TRUE;
	}

	if (u2FrameRate > 30 || u2FrameRate < 5)
		GC2356DB("error frame rate seting %d fps\n", u2FrameRate);

	if (gc2356.sensorMode == SENSOR_MODE_VIDEO)	{
		if (u2FrameRate == 30) {
			spin_lock(&gc2356mipiraw_drv_lock);
			gc2356.DummyPixels = 0;
			gc2356.DummyLines = 0;
			spin_unlock(&gc2356mipiraw_drv_lock);

			GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);
		} else {
			spin_lock(&gc2356mipiraw_drv_lock);
			gc2356.DummyPixels = 0;
			MIN_Frame_length =
			    (GC2356MIPI_VIDEO_CLK) /
			    (GC2356_VIDEO_PERIOD_PIXEL_NUMS +
			     gc2356.DummyPixels) / u2FrameRate;
			gc2356.DummyLines =
			    MIN_Frame_length - GC2356_VALID_LINE_NUMS -
			    GC2356_DEFAULT_DUMMY_LINE_NUMS;
			spin_unlock(&gc2356mipiraw_drv_lock);
			GC2356DB("GC2356SetVideoMode MIN_Frame_length %d\n",
				 MIN_Frame_length);

			GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);
		}
	} else if (gc2356.sensorMode == SENSOR_MODE_CAPTURE) {
		GC2356DB("-------[GC2356SetVideoMode]ZSD???---------\n");
		if (u2FrameRate == 30) {
			spin_lock(&gc2356mipiraw_drv_lock);
			gc2356.DummyPixels = 0;
			gc2356.DummyLines = 0;
			spin_unlock(&gc2356mipiraw_drv_lock);

			GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);
		} else {
			spin_lock(&gc2356mipiraw_drv_lock);
			gc2356.DummyPixels = 0;
			MIN_Frame_length =
			    (GC2356MIPI_VIDEO_CLK) /
			    (GC2356_VIDEO_PERIOD_PIXEL_NUMS +
			     gc2356.DummyPixels) / u2FrameRate;
			gc2356.DummyLines =
			    MIN_Frame_length - GC2356_VALID_LINE_NUMS -
			    GC2356_DEFAULT_DUMMY_LINE_NUMS;
			spin_unlock(&gc2356mipiraw_drv_lock);

			GC2356_SetDummy(gc2356.DummyPixels, gc2356.DummyLines);
		}
	}

	return KAL_TRUE;
}

UINT32 GC2356SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	if (bEnable) {
		spin_lock(&gc2356mipiraw_drv_lock);
		gc2356.GC2356AutoFlickerMode = KAL_TRUE;
		spin_unlock(&gc2356mipiraw_drv_lock);
	} else {
		spin_lock(&gc2356mipiraw_drv_lock);
		gc2356.GC2356AutoFlickerMode = KAL_FALSE;
		spin_unlock(&gc2356mipiraw_drv_lock);
		GC2356DB("Disable Auto flicker\n");
	}

	return ERROR_NONE;
}

UINT32 GC2356SetTestPatternMode(kal_bool bEnable)
{
	GC2356DB("[GC2356SetTestPatternMode] Test pattern enable:%d\n",
		 bEnable);
	if (bEnable) {
		GC2356_write_cmos_sensor(0x8b, 0xb2);	/*bit[4]: 1 enable test pattern, 0 disable test pattern*/
	} else {
		GC2356_write_cmos_sensor(0x8b, 0xa2);	/*bit[4]: 1 enable test pattern, 0 disable test pattern*/
	}
	return ERROR_NONE;
}

UINT32 GC2356MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId,
					   MUINT32 frameRate)
{
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength, frameHeight;

	GC2356DB
	    ("GC2356MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",
	     scenarioId, frameRate);
	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pclk = GC2356MIPI_PREVIEW_CLK;
		lineLength = GC2356_PV_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - GC2356_VALID_LINE_NUMS;
		gc2356.sensorMode = SENSOR_MODE_PREVIEW;
		GC2356_SetDummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pclk = GC2356MIPI_VIDEO_CLK;
		lineLength = GC2356_VIDEO_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - GC2356_VALID_LINE_NUMS;
		gc2356.sensorMode = SENSOR_MODE_VIDEO;
		GC2356_SetDummy(0, dummyLine);
		break;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pclk = GC2356MIPI_CAPTURE_CLK;
		lineLength = GC2356_FULL_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - GC2356_VALID_LINE_NUMS;
		gc2356.sensorMode = SENSOR_MODE_CAPTURE;
		GC2356_SetDummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

UINT32 GC2356MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId,
					       MUINT32 *pframeRate)
{

	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*pframeRate = 300;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		*pframeRate = 300;
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
		*pframeRate = 300;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

UINT32 GC2356FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
			    UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	UINT32 SensorRegNumber;
	UINT32 i;
	PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData =
	    (PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData =
	    (MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData =
	    (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo =
	    (MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
	MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo =
	    (MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
	MSDK_SENSOR_ENG_INFO_STRUCT *pSensorEngInfo =
	    (MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ = GC2356_IMAGE_SENSOR_FULL_WIDTH;
		*pFeatureReturnPara16 = GC2356_IMAGE_SENSOR_FULL_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*pFeatureReturnPara16++ = GC2356_FeatureControl_PERIOD_PixelNum;
		*pFeatureReturnPara16 = GC2356_FeatureControl_PERIOD_LineNum;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		switch (GC2356CurrentScenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*pFeatureReturnPara32 = GC2356MIPI_PREVIEW_CLK;
			*pFeatureParaLen = 4;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*pFeatureReturnPara32 = GC2356MIPI_VIDEO_CLK;
			*pFeatureParaLen = 4;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pFeatureReturnPara32 = GC2356MIPI_CAPTURE_CLK;
			*pFeatureParaLen = 4;
			break;
		default:
			*pFeatureReturnPara32 = GC2356MIPI_CAPTURE_CLK;
			*pFeatureParaLen = 4;
			break;
		}
		break;

	case SENSOR_FEATURE_SET_ESHUTTER:
		GC2356_SetShutter(*pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		GC2356_NightMode((BOOL) * pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		GC2356_SetGain((UINT16) *pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		/*GC2356_isp_master_clock=*pFeatureData32;*/
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		GC2356_write_cmos_sensor(pSensorRegData->RegAddr,
					 pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		pSensorRegData->RegData =
		    GC2356_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
		SensorRegNumber = FACTORY_END_ADDR;
		for (i = 0; i < SensorRegNumber; i++) {
			spin_lock(&gc2356mipiraw_drv_lock);
			GC2356SensorCCT[i].Addr = *pFeatureData32++;
			GC2356SensorCCT[i].Para = *pFeatureData32++;
			spin_unlock(&gc2356mipiraw_drv_lock);
		}
		break;
	case SENSOR_FEATURE_GET_CCT_REGISTER:
		SensorRegNumber = FACTORY_END_ADDR;
		if (*pFeatureParaLen <
		    (SensorRegNumber * sizeof(SENSOR_REG_STRUCT) + 4))
			return FALSE;
		*pFeatureData32++ = SensorRegNumber;
		for (i = 0; i < SensorRegNumber; i++) {
			*pFeatureData32++ = GC2356SensorCCT[i].Addr;
			*pFeatureData32++ = GC2356SensorCCT[i].Para;
		}
		break;
	case SENSOR_FEATURE_SET_ENG_REGISTER:
		SensorRegNumber = ENGINEER_END;
		for (i = 0; i < SensorRegNumber; i++) {
			spin_lock(&gc2356mipiraw_drv_lock);
			GC2356SensorReg[i].Addr = *pFeatureData32++;
			GC2356SensorReg[i].Para = *pFeatureData32++;
			spin_unlock(&gc2356mipiraw_drv_lock);
		}
		break;
	case SENSOR_FEATURE_GET_ENG_REGISTER:
		SensorRegNumber = ENGINEER_END;
		if (*pFeatureParaLen <
		    (SensorRegNumber * sizeof(SENSOR_REG_STRUCT) + 4))
			return FALSE;
		*pFeatureData32++ = SensorRegNumber;
		for (i = 0; i < SensorRegNumber; i++) {
			*pFeatureData32++ = GC2356SensorReg[i].Addr;
			*pFeatureData32++ = GC2356SensorReg[i].Para;
		}
		break;
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
		if (*pFeatureParaLen >= sizeof(NVRAM_SENSOR_DATA_STRUCT)) {
			pSensorDefaultData->Version =
			    NVRAM_CAMERA_SENSOR_FILE_VERSION;
			pSensorDefaultData->SensorId = GC2356MIPI_SENSOR_ID;
			memcpy(pSensorDefaultData->SensorEngReg,
			       GC2356SensorReg,
			       sizeof(SENSOR_REG_STRUCT) * ENGINEER_END);
			memcpy(pSensorDefaultData->SensorCCTReg,
			       GC2356SensorCCT,
			       sizeof(SENSOR_REG_STRUCT) * FACTORY_END_ADDR);
		} else
			return FALSE;
		*pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		memcpy(pSensorConfigData, &GC2356SensorConfigData,
		       sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		GC2356_camera_para_to_sensor();
		break;

	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
		GC2356_sensor_to_camera_para();
		break;
	case SENSOR_FEATURE_GET_GROUP_COUNT:
		*pFeatureReturnPara32++ = GC2356_get_sensor_group_count();
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_GROUP_INFO:
		GC2356_get_sensor_group_info(pSensorGroupInfo->GroupIdx,
					     pSensorGroupInfo->GroupNamePtr,
					     &pSensorGroupInfo->ItemCount);
		*pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_GET_ITEM_INFO:
		GC2356_get_sensor_item_info(pSensorItemInfo->GroupIdx,
					    pSensorItemInfo->ItemIdx,
					    pSensorItemInfo);
		*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		break;

	case SENSOR_FEATURE_SET_ITEM_INFO:
		GC2356_set_sensor_item_info(pSensorItemInfo->GroupIdx,
					    pSensorItemInfo->ItemIdx,
					    pSensorItemInfo->ItemValue);
		*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		break;

	case SENSOR_FEATURE_GET_ENG_INFO:
		pSensorEngInfo->SensorId = 129;
		pSensorEngInfo->SensorType = CMOS_SENSOR;
		pSensorEngInfo->SensorOutputDataFormat =
		    SENSOR_OUTPUT_FORMAT_RAW_B;
		*pFeatureParaLen = sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;

	case SENSOR_FEATURE_INITIALIZE_AF:
		break;
	case SENSOR_FEATURE_CONSTANT_AF:
		break;
	case SENSOR_FEATURE_MOVE_FOCUS_LENS:
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		GC2356SetVideoMode(*pFeatureData16);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		GC2356GetSensorID(pFeatureReturnPara32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		GC2356SetAutoFlickerMode((BOOL) * pFeatureData16,
					 *(pFeatureData16 + 1));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		GC2356SetTestPatternMode((BOOL) * pFeatureData16);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*pFeatureReturnPara32 = GC2356_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		GC2356MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM) *
						    pFeatureData32,
						    *(pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		GC2356MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)
							*pFeatureData32,
							(MUINT32
							 *) (*(pFeatureData32 +
							       1)));
		break;
	default:
		break;
	}
	return ERROR_NONE;
}				/* GC2356FeatureControl() */

SENSOR_FUNCTION_STRUCT SensorFuncGC2356 = {
	GC2356Open,
	GC2356GetInfo,
	GC2356GetResolution,
	GC2356FeatureControl,
	GC2356Control,
	GC2356Close
};

UINT32 GC2356_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncGC2356;

	return ERROR_NONE;
}				/* SensorInit() */
