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

#include "gc2355mipiraw_huaquan_Sensor.h"
#include "gc2355mipiraw_huaquan_Camera_Sensor_para.h"
#include "gc2355mipiraw_huaquan_CameraCustomized.h"
static DEFINE_SPINLOCK(gc2355mipiraw_drv_lock);

#define GC2355_HUAQUAN_DEBUG
//#define GC2355_HUAQUAN_DEBUG_SOFIA
#define GC2355_HUAQUAN_TEST_PATTERN_CHECKSUM (0x9d1c9dad)//(0x5593c632)

#ifdef GC2355_HUAQUAN_DEBUG
	//#define GC2355_HUAQUANDB(fmt, arg...) pr_debug("[GC2355_HUAQUANmipiraw][%s] " fmt, __func__, ##arg)
	#define GC2355_HUAQUANDB(fmt, arg...) printk("[GC2355_HUAQUANmipiraw][%s] " fmt, __func__, ##arg)
#else
	#define GC2355_HUAQUANDB(fmt, arg...)
#endif

#ifdef GC2355_HUAQUAN_DEBUG_SOFIA
	#define GC2355_HUAQUANDBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[GC2355_HUAQUANRaw] ",  fmt, ##arg)
#else
	#define GC2355_HUAQUANDBSOFIA(fmt, arg...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 GC2355_HUAQUAN_FeatureControl_PERIOD_PixelNum=GC2355_HUAQUAN_PV_PERIOD_PIXEL_NUMS;
kal_uint32 GC2355_HUAQUAN_FeatureControl_PERIOD_LineNum=GC2355_HUAQUAN_PV_PERIOD_LINE_NUMS;
UINT16  gc2355_huaquan_VIDEO_MODE_TARGET_FPS = 30;
MSDK_SENSOR_CONFIG_STRUCT GC2355_HUAQUANSensorConfigData;
MSDK_SCENARIO_ID_ENUM GC2355_HUAQUANCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT GC2355_HUAQUANSensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT GC2355_HUAQUANSensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static GC2355_HUAQUAN_PARA_STRUCT gc2355;

#define Sleep(ms) mdelay(ms)
#define GC2355_HUAQUAN_ORIENTATION IMAGE_NORMAL //IMAGE_HV_MIRROR //IMAGE_NORMAL

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

static kal_uint16 moduleid = 0x00;

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
static kal_uint8 GC2355_HUAQUAN_read_cmos_sensor(kal_uint8 addr)
{
  kal_uint8 in_buff[1] = {0xFF};
  kal_uint8 out_buff[1];
  
  out_buff[0] = addr;

    if (0 != iReadRegI2C((u8*)out_buff , 1, (u8*)in_buff, 1, GC2355_HUAQUANMIPI_WRITE_ID)) {
        GC2355_HUAQUANDB("ERROR: GC2355_HUAQUANMIPI_read_cmos_sensor \n");
    }
  return in_buff[0];
}


/*************************************************************************
* FUNCTION
*    GC2355_HUAQUANMIPI_write_cmos_sensor
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
static void GC2355_HUAQUAN_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
kal_uint8 out_buff[2];

    out_buff[0] = addr;
    out_buff[1] = para;

    iWriteRegI2C((u8*)out_buff , 2, GC2355_HUAQUANMIPI_WRITE_ID); 
	
	GC2355_HUAQUANDB("cc read 0x%x  =%x \n", addr, GC2355_HUAQUAN_read_cmos_sensor(addr));
}



void GC2355_HUAQUAN_SetGain(UINT16 iGain)
{

#define ANALOG_GAIN_1 64  // 1.00x
#define ANALOG_GAIN_2 88  //1.37x
#define ANALOG_GAIN_3 121  // 1.89x
#define ANALOG_GAIN_4 168  // 2.59x
#define ANALOG_GAIN_5 239  // 3.70x
#define ANALOG_GAIN_6 330  // 5.06x
#define ANALOG_GAIN_7 470  // 7.15x

	kal_uint16 iReg,temp,temp1;

	GC2355_HUAQUANDB("GC2355_HUAQUANMIPI_SetGain iGain = %d \n",iGain);

	//GC2355_HUAQUAN_write_cmos_sensor(0xb1, 0x01);
	//GC2355_HUAQUAN_write_cmos_sensor(0xb2, 0x00);

	iReg = iGain;
	//digital gain
	//GC2355_HUAQUANMIPI_write_cmos_sensor(0xb1, iReg>>6);
	//GC2355_HUAQUANMIPI_write_cmos_sensor(0xb2, (iReg<<2)&0xfc);
	if(iReg==0)
		return;

	if(iReg < 0x40)
		iReg = 0x40;
	if(iReg > 511)
		iReg = 511;

	if((ANALOG_GAIN_1<= iReg)&&(iReg < ANALOG_GAIN_2))
	{
			//analog gain
			GC2355_HUAQUAN_write_cmos_sensor(0xb6,0x00);//
			temp = iReg;
			GC2355_HUAQUAN_write_cmos_sensor(0xb1, temp>>6);
			GC2355_HUAQUAN_write_cmos_sensor(0xb2, (temp<<2)&0xfc);
			GC2355_HUAQUANDB("GC2355_HUAQUAN analogic gain 1x , GC2355_HUAQUAN add pregain = %d\n",temp);

	}
	else if((ANALOG_GAIN_2<= iReg)&&(iReg < ANALOG_GAIN_3))
	{
			//analog gain
			GC2355_HUAQUAN_write_cmos_sensor(0xb6,0x01);//
			temp = 64*iReg/ANALOG_GAIN_2;
			GC2355_HUAQUAN_write_cmos_sensor(0xb1, temp>>6);
			GC2355_HUAQUAN_write_cmos_sensor(0xb2, (temp<<2)&0xfc);
			GC2355_HUAQUANDB("GC2355_HUAQUAN analogic gain 1.45x , GC2355_HUAQUAN add pregain = %d\n",temp);

	}

	else if((ANALOG_GAIN_3<= iReg)&&(iReg < ANALOG_GAIN_4))
	{
			//analog gain
			GC2355_HUAQUAN_write_cmos_sensor(0xb6,	0x02);//
			temp = 64*iReg/ANALOG_GAIN_3;
			GC2355_HUAQUAN_write_cmos_sensor(0xb1, temp>>6);
			GC2355_HUAQUAN_write_cmos_sensor(0xb2, (temp<<2)&0xfc);
			GC2355_HUAQUANDB("GC2355_HUAQUAN analogic gain 2.02x , GC2355_HUAQUAN add pregain = %d\n",temp);	
	}

//	else if(ANALOG_GAIN_4<= iReg)
    else
	{
			//analog gain
			GC2355_HUAQUAN_write_cmos_sensor(0xb6,	0x03);//
			temp = 64*iReg/ANALOG_GAIN_4;
			GC2355_HUAQUAN_write_cmos_sensor(0xb1, temp>>6);
			GC2355_HUAQUAN_write_cmos_sensor(0xb2, (temp<<2)&0xfc);

			GC2355_HUAQUANDB("GC2355_HUAQUAN analogic gain 2.8x, temp = %d\n", temp);
	}

}   /*  GC2355_HUAQUAN_SetGain_SetGain  */


void GC2355_HUAQUAN_camera_para_to_sensor(void)
{}

void GC2355_HUAQUAN_sensor_to_camera_para(void)
{}

kal_int32  GC2355_HUAQUAN_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void GC2355_HUAQUAN_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{}
void GC2355_HUAQUAN_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{}
kal_bool GC2355_HUAQUAN_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{    return KAL_TRUE;}

static void GC2355_HUAQUAN_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
 	kal_uint32 hb = 0;
	kal_uint32 vb = 0;

	if ( SENSOR_MODE_PREVIEW == gc2355.sensorMode )	//SXGA size output
	{
		hb = GC2355_HUAQUAN_DEFAULT_DUMMY_PIXEL_NUMS + iPixels;
		vb = GC2355_HUAQUAN_DEFAULT_DUMMY_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_VIDEO== gc2355.sensorMode )
	{
		hb = GC2355_HUAQUAN_DEFAULT_DUMMY_PIXEL_NUMS + iPixels;
		vb = GC2355_HUAQUAN_DEFAULT_DUMMY_LINE_NUMS + iLines;
	}
	else//QSXGA size output
	{
		hb = GC2355_HUAQUAN_DEFAULT_DUMMY_PIXEL_NUMS + iPixels;
		vb = GC2355_HUAQUAN_DEFAULT_DUMMY_LINE_NUMS + iLines;
	}

	//if(gc2355.maxExposureLines > frame_length -4 )
	//	return;

	//ASSERT(line_length < GC2355_HUAQUAN_MAX_LINE_LENGTH);		//0xCCCC
	//ASSERT(frame_length < GC2355_HUAQUAN_MAX_FRAME_LENGTH);	//0xFFFF

	//Set HB
	GC2355_HUAQUAN_write_cmos_sensor(0x05, (hb >> 8)& 0xFF);
	GC2355_HUAQUAN_write_cmos_sensor(0x06, hb & 0xFF);

	//Set VB
	GC2355_HUAQUAN_write_cmos_sensor(0x07, (vb >> 8) & 0xFF);
	GC2355_HUAQUAN_write_cmos_sensor(0x08, vb & 0xFF);
	GC2355_HUAQUANDB("GC2355_HUAQUAN_SetDummy framelength=%d\n",vb+GC2355_HUAQUAN_VALID_LINE_NUMS);

}   /*  GC2355_HUAQUAN_SetDummy */

static void GC2355_HUAQUAN_Sensor_Init(void)
{
	/* SYS */
	GC2355_HUAQUAN_write_cmos_sensor(0xfe,0x80);
	GC2355_HUAQUAN_write_cmos_sensor(0xfe,0x80);
	GC2355_HUAQUAN_write_cmos_sensor(0xfe,0x80);
	GC2355_HUAQUAN_write_cmos_sensor(0xf2,0x00); //sync_pad_io_ebi
	GC2355_HUAQUAN_write_cmos_sensor(0xf6,0x00); //up down
	GC2355_HUAQUAN_write_cmos_sensor(0xf7,0x19); //19 //pll enable
	GC2355_HUAQUAN_write_cmos_sensor(0xf8,0x06); //Pll mode 2  /////86--\B0\B5\B4\A6\BD\B5Ƶ
	GC2355_HUAQUAN_write_cmos_sensor(0xf9,0x0e); //de//[0] pll enable
	GC2355_HUAQUAN_write_cmos_sensor(0xfa,0x00); //div
	GC2355_HUAQUAN_write_cmos_sensor(0xfc,0x06); //4e
	//GC2355_HUAQUAN_write_cmos_sensor(0xfd,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0xfe,0x00);
	
	/* ANALOG & CISCTL*/
	// AEC&frame length//
	GC2355_HUAQUAN_write_cmos_sensor(0x03,0x04);  // 0a -> 04
	GC2355_HUAQUAN_write_cmos_sensor(0x04,0xb0);  // 41 -> b0
	GC2355_HUAQUAN_write_cmos_sensor(0x05,0x01); //max 30fps  03
	GC2355_HUAQUAN_write_cmos_sensor(0x06,0x1c); 
	GC2355_HUAQUAN_write_cmos_sensor(0x07,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x08,0x0e); //22
	GC2355_HUAQUAN_write_cmos_sensor(0x0a,0x00); //row start
	GC2355_HUAQUAN_write_cmos_sensor(0x0c,0x04); //0c//col start
	GC2355_HUAQUAN_write_cmos_sensor(0x0d,0x04);
	GC2355_HUAQUAN_write_cmos_sensor(0x0e,0xc0); //c0
	GC2355_HUAQUAN_write_cmos_sensor(0x0f,0x06); 
	GC2355_HUAQUAN_write_cmos_sensor(0x10,0x50); //Window setting 1616x1216
	/*GC2355_HUAQUAN_write_cmos_sensor(0x11,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x12,0x18); //sh_delay
	GC2355_HUAQUAN_write_cmos_sensor(0x13,0x11);
	GC2355_HUAQUAN_write_cmos_sensor(0x14,0x01);
	GC2355_HUAQUAN_write_cmos_sensor(0x15,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x16,0xc1);*/
	GC2355_HUAQUAN_write_cmos_sensor(0x17,0x17);//14
	//GC2355_HUAQUAN_write_cmos_sensor(0x18,0x02);
	GC2355_HUAQUAN_write_cmos_sensor(0x19,0x0b);
	GC2355_HUAQUAN_write_cmos_sensor(0x1b,0x48); //48
	GC2355_HUAQUAN_write_cmos_sensor(0x1c,0x12);
	GC2355_HUAQUAN_write_cmos_sensor(0x1d,0x10); //double reset
	GC2355_HUAQUAN_write_cmos_sensor(0x1e,0xbc); //a8//col_r/rowclk_mode/rsthigh_en FPN
	GC2355_HUAQUAN_write_cmos_sensor(0x1f,0xc9); //c8//rsgl_s_mode/vpix_s_mode \B5ƹܺ\E1\CC\F5\CE\C6
	GC2355_HUAQUAN_write_cmos_sensor(0x20,0x71);
	GC2355_HUAQUAN_write_cmos_sensor(0x21,0x20); //rsg \B5ƹܺ\E1\CC\F5\CE\C6	//////40
	GC2355_HUAQUAN_write_cmos_sensor(0x22,0xa0); //e0	//80  //f0
	GC2355_HUAQUAN_write_cmos_sensor(0x23,0x51); //51
	GC2355_HUAQUAN_write_cmos_sensor(0x24,0x19); //0b //55
	//GC2355_HUAQUAN_write_cmos_sensor(0x25,0x10);	//10
	//GC2355_HUAQUAN_write_cmos_sensor(0x26,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x27,0x20);
	GC2355_HUAQUAN_write_cmos_sensor(0x28,0x00);//10
	//GC2355_HUAQUAN_write_cmos_sensor(0x29,0x00);
	//GC2355_HUAQUAN_write_cmos_sensor(0x2a,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x2b,0x80); //00 sf_s_mode FPN
	GC2355_HUAQUAN_write_cmos_sensor(0x2c,0x38); //50 //5c ispg FPN
	//GC2355_HUAQUAN_write_cmos_sensor(0x2d,0x15);
	GC2355_HUAQUAN_write_cmos_sensor(0x2e,0x16); //05//eq width
	GC2355_HUAQUAN_write_cmos_sensor(0x2f,0x14); //[3:0]tx_width д0\C4ܸ\C4\C2\E9\B5\E3
	GC2355_HUAQUAN_write_cmos_sensor(0x30,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x31,0x01);
	GC2355_HUAQUAN_write_cmos_sensor(0x32,0x02);
	GC2355_HUAQUAN_write_cmos_sensor(0x33,0x03);
	GC2355_HUAQUAN_write_cmos_sensor(0x34,0x07);
	GC2355_HUAQUAN_write_cmos_sensor(0x35,0x0b);
	GC2355_HUAQUAN_write_cmos_sensor(0x36,0x0f);
	
	/* gain */
	GC2355_HUAQUAN_write_cmos_sensor(0xb0,0x50);
	GC2355_HUAQUAN_write_cmos_sensor(0xb1,0x01);
	GC2355_HUAQUAN_write_cmos_sensor(0xb2,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0xb3,0x40);
	GC2355_HUAQUAN_write_cmos_sensor(0xb4,0x40);
	GC2355_HUAQUAN_write_cmos_sensor(0xb5,0x40);
	GC2355_HUAQUAN_write_cmos_sensor(0xb6,0x00);
	
	/* crop */
	GC2355_HUAQUAN_write_cmos_sensor(0x92,0x02);
	GC2355_HUAQUAN_write_cmos_sensor(0x95,0x04);
	GC2355_HUAQUAN_write_cmos_sensor(0x96,0xb0);
	GC2355_HUAQUAN_write_cmos_sensor(0x97,0x06);
	GC2355_HUAQUAN_write_cmos_sensor(0x98,0x40); //out window set 1600x1200
	
	/*	BLK  */
	GC2355_HUAQUAN_write_cmos_sensor(0x18,0x02);//1a-lily
	GC2355_HUAQUAN_write_cmos_sensor(0x1a,0x01);//09-lily //01
	GC2355_HUAQUAN_write_cmos_sensor(0x40,0x42);//43-lily
	GC2355_HUAQUAN_write_cmos_sensor(0x41,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x44,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x45,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x46,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x47,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x48,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x49,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x4a,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x4b,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x4e,0x3c); //BLK select
	GC2355_HUAQUAN_write_cmos_sensor(0x4f,0x00); 
	GC2355_HUAQUAN_write_cmos_sensor(0x5e,0x00);//18-lily //offset ratio
	GC2355_HUAQUAN_write_cmos_sensor(0x66,0x20);//20-lily //dark ratio
	GC2355_HUAQUAN_write_cmos_sensor(0x6a,0x00);//39
	GC2355_HUAQUAN_write_cmos_sensor(0x6b,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x6c,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x6d,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x6e,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x6f,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x70,0x00);
	GC2355_HUAQUAN_write_cmos_sensor(0x71,0x00); //manual offset
	
	/* Dark sun */
	GC2355_HUAQUAN_write_cmos_sensor(0x87,0x03); //
	GC2355_HUAQUAN_write_cmos_sensor(0xe0,0xe7); //dark sun en/extend mode
	GC2355_HUAQUAN_write_cmos_sensor(0xe2,0x03);
	GC2355_HUAQUAN_write_cmos_sensor(0xe3,0xc0); //clamp
	GC2355_HUAQUAN_write_cmos_sensor(0xe4, 0x01);
	GC2355_HUAQUAN_write_cmos_sensor(0xe5, 0xff);	/*add by lucifer*/
	
	/*MIPI*/
	  GC2355_HUAQUAN_write_cmos_sensor(0xfe, 0x03);
	  GC2355_HUAQUAN_write_cmos_sensor(0x01, 0x83);
	  GC2355_HUAQUAN_write_cmos_sensor(0x02, 0x11);
	  GC2355_HUAQUAN_write_cmos_sensor(0x03, 0x91);
	  GC2355_HUAQUAN_write_cmos_sensor(0x04, 0x01);
	  GC2355_HUAQUAN_write_cmos_sensor(0x05, 0x00);
	  GC2355_HUAQUAN_write_cmos_sensor(0x06, 0xa2);
	  GC2355_HUAQUAN_write_cmos_sensor(0x10, 0x92);//91//94//1lane raw8, 90
	  GC2355_HUAQUAN_write_cmos_sensor(0x11, 0x2b);//2b
	  GC2355_HUAQUAN_write_cmos_sensor(0x15, 0x60);
	  GC2355_HUAQUAN_write_cmos_sensor(0x12, 0xd0); //d0//40
	  GC2355_HUAQUAN_write_cmos_sensor(0x13, 0x07); //07//06
	  GC2355_HUAQUAN_write_cmos_sensor(0x21, 0x10);
	  GC2355_HUAQUAN_write_cmos_sensor(0x22, 0x05);
	  GC2355_HUAQUAN_write_cmos_sensor(0x23, 0x30);
	  GC2355_HUAQUAN_write_cmos_sensor(0x24, 0x02);
	  GC2355_HUAQUAN_write_cmos_sensor(0x25, 0x15);
	  GC2355_HUAQUAN_write_cmos_sensor(0x26, 0x08);
	  GC2355_HUAQUAN_write_cmos_sensor(0x27, 0x06);
	  GC2355_HUAQUAN_write_cmos_sensor(0x29, 0x06);//04
	  GC2355_HUAQUAN_write_cmos_sensor(0x2a, 0x0a);
	  GC2355_HUAQUAN_write_cmos_sensor(0x2b, 0x08);
	  GC2355_HUAQUAN_write_cmos_sensor(0x40, 0x00);
	  GC2355_HUAQUAN_write_cmos_sensor(0x41, 0x00);    
	  GC2355_HUAQUAN_write_cmos_sensor(0x42, 0x40);
	  GC2355_HUAQUAN_write_cmos_sensor(0x43, 0x06);  
	  GC2355_HUAQUAN_write_cmos_sensor(0xfe, 0x00);
	

}   /*  GC2355_HUAQUAN_Sensor_Init  */

UINT32 GC2355_HUAQUANOpen(void)
{
	volatile signed int i;
	kal_uint16 sensor_id = 0;
	GC2355_HUAQUANDB("GC2355_HUAQUANOpen enter :\n ");
	sensor_id=((GC2355_HUAQUAN_read_cmos_sensor(0xf0) << 8) | GC2355_HUAQUAN_read_cmos_sensor(0xf1)); 
	GC2355_HUAQUANDB("GC2355_HUAQUANMIPIGetSensorID:%x \n",sensor_id);
	spin_lock(&gc2355mipiraw_drv_lock);
	gc2355.sensorMode = SENSOR_MODE_INIT;
	gc2355.GC2355_HUAQUANAutoFlickerMode = KAL_FALSE;
	gc2355.GC2355_HUAQUANVideoMode = KAL_FALSE;
	gc2355.shutter = 0xff;
	gc2355.Gain = 64;
	spin_unlock(&gc2355mipiraw_drv_lock);
	GC2355_HUAQUAN_Sensor_Init();

	GC2355_HUAQUANDB("GC2355_HUAQUANOpen exit :\n ");

    return ERROR_NONE;
}
UINT32 GC2355HUAQUANGetModuleID()
{
	printk("GC2355HUAQUANGetModuleID\n");
	return moduleid;
}

extern IMM_auxadc_GetOneChannelValue_Cali(int Channel, int *voltage);
UINT32 GC2355_HUAQUANGetSensorID(UINT32 *sensorID)
{
    int  retry = 1;
    int adc_val = 0;
    GC2355_HUAQUANDB("GC2355_HUAQUANGetSensorID enter :\n ");

    // check if sensor ID correct
    *sensorID =((GC2355_HUAQUAN_read_cmos_sensor(0xf0) << 8) | GC2355_HUAQUAN_read_cmos_sensor(0xf1)); 
    //IMM_auxadc_GetOneChannelValue_Cali(12, &adc_val);
    //if ((*sensorID == GC2355MIPI_SENSOR_ID_HUAQUAN) && (adc_val < 100000)) {   /*0.1v = 100000uv , 0v :huaqun ; >0.1:qunhui*/
	if (*sensorID == GC2355MIPI_SENSOR_ID_HUAQUAN) {
	*sensorID = GC2355MIPI_SENSOR_ID_HUAQUAN;
	moduleid = 0x02;

   } else {
	*sensorID = 0xFFFFFFFF;
	return ERROR_SENSOR_CONNECT_FAIL;
   }
	GC2355_HUAQUANDB("GC2355_HUAQUANMIPIGetSensorID:%x \n",*sensorID);
 /*
    if (*sensorID != GC2355MIPI_SENSOR_ID_HUAQUAN) {
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
  */
    return ERROR_NONE;
}


void GC2355_HUAQUAN_SetShutter(kal_uint32 iShutter)
{
    //kal_uint8 i=0;
	if(MSDK_SCENARIO_ID_CAMERA_ZSD == GC2355_HUAQUANCurrentScenarioId )
	{
		//GC2355_HUAQUANDB("always UPDATE SHUTTER when gc2355.sensorMode == SENSOR_MODE_CAPTURE\n");
	}
	else{
		if(gc2355.sensorMode == SENSOR_MODE_CAPTURE)
		{
			//GC2355_HUAQUANDB("capture!!DONT UPDATE SHUTTER!!\n");
			//return;
		}
	}
	if(gc2355.shutter == iShutter);
		//return;
	
   spin_lock(&gc2355mipiraw_drv_lock);
   gc2355.shutter= iShutter;
  
   spin_unlock(&gc2355mipiraw_drv_lock);
   if(gc2355.shutter > 16383) gc2355.shutter = 16383;
   if(gc2355.shutter < 12) gc2355.shutter = 12;
   GC2355_HUAQUAN_write_cmos_sensor(0x03, (gc2355.shutter>>8) & 0x3F);
   GC2355_HUAQUAN_write_cmos_sensor(0x04, gc2355.shutter & 0xFF);
   	
   
   GC2355_HUAQUANDB("GC2355_HUAQUAN_SetShutter:%x \n",iShutter); 
   	
 //  GC2355_HUAQUANDB("GC2355_HUAQUAN_SetShutter 0x03=%x, 0x04=%x \n",GC2355_HUAQUAN_read_cmos_sensor(0x03),GC2355_HUAQUAN_read_cmos_sensor(0x04));
   return;
}   /*  GC2355_HUAQUAN_SetShutter   */

UINT32 GC2355_HUAQUAN_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2;
	UINT32 shutter =0;
	temp_reg1 = GC2355_HUAQUAN_read_cmos_sensor(0x03);    // AEC[b19~b16]
	temp_reg2 = GC2355_HUAQUAN_read_cmos_sensor(0x04);    // AEC[b15~b8]
	shutter  = ((temp_reg1 << 8)| (temp_reg2));

	return shutter;
}

void GC2355_HUAQUAN_NightMode(kal_bool bEnable)
{}


UINT32 GC2355_HUAQUANClose(void)
{    return ERROR_NONE;}

void GC2355_HUAQUANSetFlipMirror(kal_int32 imgMirror)
{
	kal_int16 mirror=0,flip=0;

    switch (imgMirror)
    {
        case IMAGE_NORMAL://IMAGE_NORMAL:
			GC2355_HUAQUAN_write_cmos_sensor(0x17,0x14);//bit[1][0]
			GC2355_HUAQUAN_write_cmos_sensor(0x92,0x03);
			GC2355_HUAQUAN_write_cmos_sensor(0x94,0x0b);
            break;
        case IMAGE_H_MIRROR://IMAGE_H_MIRROR:
            GC2355_HUAQUAN_write_cmos_sensor(0x17,0x15);
//			GC2355_HUAQUAN_write_cmos_sensor(0x92,0x03);
//			GC2355_HUAQUAN_write_cmos_sensor(0x94,0x0b);
            break;
        case IMAGE_V_MIRROR://IMAGE_V_MIRROR:
            GC2355_HUAQUAN_write_cmos_sensor(0x17,0x16);
//			GC2355_HUAQUAN_write_cmos_sensor(0x92,0x02);
//			GC2355_HUAQUAN_write_cmos_sensor(0x94,0x0b);
            break;
        case IMAGE_HV_MIRROR://IMAGE_HV_MIRROR:
			GC2355_HUAQUAN_write_cmos_sensor(0x17,0x17);
//			GC2355_HUAQUAN_write_cmos_sensor(0x92,0x02);
//			GC2355_HUAQUAN_write_cmos_sensor(0x94,0x0b);
            break;
    }
}

UINT32 GC2355_HUAQUANPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	GC2355_HUAQUANDB("GC2355_HUAQUANPreview enter:");

	
	spin_lock(&gc2355mipiraw_drv_lock);
	gc2355.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	gc2355.DummyPixels = 0;//define dummy pixels and lines
	gc2355.DummyLines = 0 ;
	GC2355_HUAQUAN_FeatureControl_PERIOD_PixelNum=GC2355_HUAQUAN_PV_PERIOD_PIXEL_NUMS+ gc2355.DummyPixels;
	GC2355_HUAQUAN_FeatureControl_PERIOD_LineNum=GC2355_HUAQUAN_PV_PERIOD_LINE_NUMS+gc2355.DummyLines;
	spin_unlock(&gc2355mipiraw_drv_lock);

	//GC2355_HUAQUAN_write_shutter(gc2355.shutter);
	//write_GC2355_HUAQUAN_gain(gc2355.pvGain);
	//set mirror & flip
	//GC2355_HUAQUANDB("[GC2355_HUAQUANPreview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&gc2355mipiraw_drv_lock);
	gc2355.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&gc2355mipiraw_drv_lock);
	//if(SENSOR_MODE_PREVIEW==gc2355.sensorMode ) return ERROR_NONE;
	
	GC2355_HUAQUANDB("GC2355_HUAQUANPreview mirror:%d\n",sensor_config_data->SensorImageMirror);
	GC2355_HUAQUANSetFlipMirror(GC2355_HUAQUAN_ORIENTATION);
	GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);
	GC2355_HUAQUANDB("GC2355_HUAQUANPreview exit: \n");
	mdelay(100);
    return ERROR_NONE;
}	/* GC2355_HUAQUANPreview() */


UINT32 GC2355_HUAQUANVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	GC2355_HUAQUANDB("GC2355_HUAQUANVideo enter:");
	if(gc2355.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
		//GC2355_HUAQUANVideoSetting();
	
	spin_lock(&gc2355mipiraw_drv_lock);
	gc2355.sensorMode = SENSOR_MODE_VIDEO;
	GC2355_HUAQUAN_FeatureControl_PERIOD_PixelNum=GC2355_HUAQUAN_VIDEO_PERIOD_PIXEL_NUMS+ gc2355.DummyPixels;
	GC2355_HUAQUAN_FeatureControl_PERIOD_LineNum=GC2355_HUAQUAN_VIDEO_PERIOD_LINE_NUMS+gc2355.DummyLines;
/*	
	spin_unlock(&gc2355mipiraw_drv_lock);

	//GC2355_HUAQUAN_write_shutter(gc2355.shutter);
	//write_GC2355_HUAQUAN_gain(gc2355.pvGain);

	spin_lock(&gc2355mipiraw_drv_lock);
*/
	gc2355.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&gc2355mipiraw_drv_lock);
	GC2355_HUAQUANSetFlipMirror(GC2355_HUAQUAN_ORIENTATION);
	GC2355_HUAQUANDB("GC2355_HUAQUANVideo mirror:%d\n",sensor_config_data->SensorImageMirror);
	GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);
	//GC2355_HUAQUANDBSOFIA("[GC2355_HUAQUANVideo]frame_len=%x\n", ((GC2355_HUAQUAN_read_cmos_sensor(0x380e)<<8)+GC2355_HUAQUAN_read_cmos_sensor(0x380f)));
	mdelay(100);
	GC2355_HUAQUANDB("GC2355_HUAQUANVideo exit:\n");
    return ERROR_NONE;
}


UINT32 GC2355_HUAQUANCapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
 	kal_uint32 shutter = gc2355.shutter;
	kal_uint32 temp_data;
	if( SENSOR_MODE_CAPTURE== gc2355.sensorMode)
	{
		GC2355_HUAQUANDB("GC2355_HUAQUANCapture BusrtShot!!!\n");
	}else{
		GC2355_HUAQUANDB("GC2355_HUAQUANCapture enter:\n");
		
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.pvShutter =shutter;
		gc2355.sensorGlobalGain = temp_data;
		gc2355.pvGain =gc2355.sensorGlobalGain;
		spin_unlock(&gc2355mipiraw_drv_lock);

		GC2355_HUAQUANDB("[GC2355_HUAQUANCapture]gc2355.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",gc2355.shutter, shutter,gc2355.sensorGlobalGain);

		// Full size setting
		//.GC2355_HUAQUANCaptureSetting();

		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.sensorMode = SENSOR_MODE_CAPTURE;
		gc2355.imgMirror = sensor_config_data->SensorImageMirror;
		gc2355.DummyPixels = 0;//define dummy pixels and lines                                                                                                         
		gc2355.DummyLines = 0 ;    
		GC2355_HUAQUAN_FeatureControl_PERIOD_PixelNum = GC2355_HUAQUAN_FULL_PERIOD_PIXEL_NUMS + gc2355.DummyPixels;
		GC2355_HUAQUAN_FeatureControl_PERIOD_LineNum = GC2355_HUAQUAN_FULL_PERIOD_LINE_NUMS + gc2355.DummyLines;
		spin_unlock(&gc2355mipiraw_drv_lock);

		//GC2355_HUAQUANDB("[GC2355_HUAQUANCapture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);
		
		GC2355_HUAQUANDB("GC2355_HUAQUANcapture mirror:%d\n",sensor_config_data->SensorImageMirror);
		GC2355_HUAQUANSetFlipMirror(GC2355_HUAQUAN_ORIENTATION);
		GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);

	    if(GC2355_HUAQUANCurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
	    {
			GC2355_HUAQUANDB("GC2355_HUAQUANCapture exit ZSD!!\n");
			return ERROR_NONE;
	    }
		GC2355_HUAQUANDB("GC2355_HUAQUANCapture exit:\n");
	}

    return ERROR_NONE;
}	/* GC2355_HUAQUANCapture() */

UINT32 GC2355_HUAQUANGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    GC2355_HUAQUANDB("GC2355_HUAQUANGetResolution!!\n");
	pSensorResolution->SensorPreviewWidth	= GC2355_HUAQUAN_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= GC2355_HUAQUAN_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorFullWidth		= GC2355_HUAQUAN_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= GC2355_HUAQUAN_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorVideoWidth		= GC2355_HUAQUAN_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = GC2355_HUAQUAN_IMAGE_SENSOR_VIDEO_HEIGHT;
    return ERROR_NONE;
}   /* GC2355_HUAQUANGetResolution() */

UINT32 GC2355_HUAQUANGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	pSensorInfo->SensorPreviewResolutionX= GC2355_HUAQUAN_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY= GC2355_HUAQUAN_IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX= GC2355_HUAQUAN_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= GC2355_HUAQUAN_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&gc2355mipiraw_drv_lock);
	gc2355.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&gc2355mipiraw_drv_lock);

	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_B;
	
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
	pSensorInfo->MIPIsensorType = MIPI_OPHY_CSI2;

    pSensorInfo->CaptureDelayFrame = 4;
    pSensorInfo->PreviewDelayFrame = 3;
    pSensorInfo->VideoDelayFrame = 3;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = GC2355_HUAQUAN_PV_X_START;
            pSensorInfo->SensorGrabStartY = GC2355_HUAQUAN_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;//2
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = GC2355_HUAQUAN_VIDEO_X_START;
            pSensorInfo->SensorGrabStartY = GC2355_HUAQUAN_VIDEO_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;//2
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = GC2355_HUAQUAN_FULL_X_START;	//2*GC2355_HUAQUAN_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = GC2355_HUAQUAN_FULL_Y_START;	//2*GC2355_HUAQUAN_IMAGE_SENSOR_PV_STARTY;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = GC2355_HUAQUAN_PV_X_START;
            pSensorInfo->SensorGrabStartY = GC2355_HUAQUAN_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;//2
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }

    memcpy(pSensorConfigData, &GC2355_HUAQUANSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* GC2355_HUAQUANGetInfo() */


UINT32 GC2355_HUAQUANControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&gc2355mipiraw_drv_lock);
		GC2355_HUAQUANCurrentScenarioId = ScenarioId;
		spin_unlock(&gc2355mipiraw_drv_lock);
		GC2355_HUAQUANDB("GC2355_HUAQUANCurrentScenarioId=%d\n",GC2355_HUAQUANCurrentScenarioId);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            GC2355_HUAQUANPreview(pImageWindow, pSensorConfigData);
            break;
/*        
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			GC2355_HUAQUANVideo(pImageWindow, pSensorConfigData);
			break;
	*/		
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            GC2355_HUAQUANCapture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
} /* GC2355_HUAQUANControl() */


UINT32 GC2355_HUAQUANSetVideoMode(UINT16 u2FrameRate)
{
    kal_uint32 MIN_Frame_length =0,extralines=0;
    GC2355_HUAQUANDB("[GC2355_HUAQUANSetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&gc2355mipiraw_drv_lock);
	 gc2355_huaquan_VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&gc2355mipiraw_drv_lock);

	if(u2FrameRate==0)
	{   
		GC2355_HUAQUANDB("Disable Video Mode or dynimac fps\n");	
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.DummyPixels = 0;//define dummy pixels and lines
		gc2355.DummyLines = extralines ;
		spin_unlock(&gc2355mipiraw_drv_lock);
		//GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels ,gc2355.DummyLines);
		return KAL_TRUE;
	}
	
	if(u2FrameRate >30 || u2FrameRate <5)
	    GC2355_HUAQUANDB("error frame rate seting %d fps\n",u2FrameRate);

    if(gc2355.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(u2FrameRate==30)
    		{
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.DummyPixels = 0;//define dummy pixels and lines
		gc2355.DummyLines = 0 ;
		spin_unlock(&gc2355mipiraw_drv_lock);
		
		GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);
    		}
		else
		{
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.DummyPixels = 0;//define dummy pixels and lines
		MIN_Frame_length = (GC2355_HUAQUANMIPI_VIDEO_CLK)/(GC2355_HUAQUAN_VIDEO_PERIOD_PIXEL_NUMS + gc2355.DummyPixels)/u2FrameRate;
		gc2355.DummyLines = MIN_Frame_length - GC2355_HUAQUAN_VALID_LINE_NUMS-GC2355_HUAQUAN_DEFAULT_DUMMY_LINE_NUMS;
		spin_unlock(&gc2355mipiraw_drv_lock);
		GC2355_HUAQUANDB("GC2355_HUAQUANSetVideoMode MIN_Frame_length %d\n",MIN_Frame_length);
		
		GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);
    		}	
    }
	else if(gc2355.sensorMode == SENSOR_MODE_CAPTURE)
	{
		GC2355_HUAQUANDB("-------[GC2355_HUAQUANSetVideoMode]ZSD???---------\n");
		if(u2FrameRate==30)
    		{
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.DummyPixels = 0;//define dummy pixels and lines
		gc2355.DummyLines = 0 ;
		spin_unlock(&gc2355mipiraw_drv_lock);
		
		GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);
    		}
		else
			{
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.DummyPixels = 0;//define dummy pixels and lines
		MIN_Frame_length = (GC2355_HUAQUANMIPI_VIDEO_CLK)/(GC2355_HUAQUAN_VIDEO_PERIOD_PIXEL_NUMS + gc2355.DummyPixels)/u2FrameRate;
		gc2355.DummyLines = MIN_Frame_length - GC2355_HUAQUAN_VALID_LINE_NUMS-GC2355_HUAQUAN_DEFAULT_DUMMY_LINE_NUMS;
		spin_unlock(&gc2355mipiraw_drv_lock);
		
		GC2355_HUAQUAN_SetDummy(gc2355.DummyPixels,gc2355.DummyLines);
    		}	
	}

    return KAL_TRUE;
}

UINT32 GC2355_HUAQUANSetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	//return ERROR_NONE;
    //GC2355_HUAQUANDB("[GC2355_HUAQUANSetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
	if(bEnable) {   // enable auto flicker
		spin_lock(&gc2355mipiraw_drv_lock);
		gc2355.GC2355_HUAQUANAutoFlickerMode = KAL_TRUE;
		spin_unlock(&gc2355mipiraw_drv_lock);
    } else {
    	spin_lock(&gc2355mipiraw_drv_lock);
        gc2355.GC2355_HUAQUANAutoFlickerMode = KAL_FALSE;
		spin_unlock(&gc2355mipiraw_drv_lock);
        GC2355_HUAQUANDB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 GC2355_HUAQUANSetTestPatternMode(kal_bool bEnable)
{
    GC2355_HUAQUANDB("[GC2355_HUAQUANSetTestPatternMode] Test pattern enable:%d\n", bEnable);
    if(bEnable)
    {
        GC2355_HUAQUAN_write_cmos_sensor(0x8b,0xb2); //bit[4]: 1 enable test pattern, 0 disable test pattern
    }
	else
	{
	    GC2355_HUAQUAN_write_cmos_sensor(0x8b,0xa2);//bit[4]: 1 enable test pattern, 0 disable test pattern
	}
    return ERROR_NONE;
}


UINT32 GC2355_HUAQUANMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	GC2355_HUAQUANDB("GC2355_HUAQUANMIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = GC2355_HUAQUANMIPI_PREVIEW_CLK;
			lineLength = GC2355_HUAQUAN_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - GC2355_HUAQUAN_VALID_LINE_NUMS;
			gc2355.sensorMode = SENSOR_MODE_PREVIEW;
			GC2355_HUAQUAN_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = GC2355_HUAQUANMIPI_VIDEO_CLK; 
			lineLength = GC2355_HUAQUAN_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - GC2355_HUAQUAN_VALID_LINE_NUMS;
			gc2355.sensorMode = SENSOR_MODE_VIDEO;
			GC2355_HUAQUAN_SetDummy(0, dummyLine);			
			break;			
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = GC2355_HUAQUANMIPI_CAPTURE_CLK;
			lineLength = GC2355_HUAQUAN_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - GC2355_HUAQUAN_VALID_LINE_NUMS;
			gc2355.sensorMode = SENSOR_MODE_CAPTURE;
			GC2355_HUAQUAN_SetDummy(0, dummyLine);			
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			break;		
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 GC2355_HUAQUANMIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
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
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			 *pframeRate = 300;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}


UINT32 GC2355_HUAQUANFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                                                                UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    UINT32 SensorRegNumber;
    UINT32 i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= GC2355_HUAQUAN_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= GC2355_HUAQUAN_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
				*pFeatureReturnPara16++= GC2355_HUAQUAN_FeatureControl_PERIOD_PixelNum;
				*pFeatureReturnPara16= GC2355_HUAQUAN_FeatureControl_PERIOD_LineNum;
				*pFeatureParaLen=4;
				break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(GC2355_HUAQUANCurrentScenarioId)
			{
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = GC2355_HUAQUANMIPI_PREVIEW_CLK;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = GC2355_HUAQUANMIPI_VIDEO_CLK;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = GC2355_HUAQUANMIPI_CAPTURE_CLK;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = GC2355_HUAQUANMIPI_CAPTURE_CLK;
					*pFeatureParaLen=4;
					break;
			}
		      break;

        case SENSOR_FEATURE_SET_ESHUTTER:
            GC2355_HUAQUAN_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            GC2355_HUAQUAN_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            GC2355_HUAQUAN_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //GC2355_HUAQUAN_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            GC2355_HUAQUAN_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = GC2355_HUAQUAN_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&gc2355mipiraw_drv_lock);
                GC2355_HUAQUANSensorCCT[i].Addr=*pFeatureData32++;
                GC2355_HUAQUANSensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&gc2355mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=GC2355_HUAQUANSensorCCT[i].Addr;
                *pFeatureData32++=GC2355_HUAQUANSensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&gc2355mipiraw_drv_lock);
                GC2355_HUAQUANSensorReg[i].Addr=*pFeatureData32++;
                GC2355_HUAQUANSensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&gc2355mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=GC2355_HUAQUANSensorReg[i].Addr;
                *pFeatureData32++=GC2355_HUAQUANSensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=GC2355MIPI_SENSOR_ID_HUAQUAN;
                memcpy(pSensorDefaultData->SensorEngReg, GC2355_HUAQUANSensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, GC2355_HUAQUANSensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &GC2355_HUAQUANSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            GC2355_HUAQUAN_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            GC2355_HUAQUAN_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=GC2355_HUAQUAN_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            GC2355_HUAQUAN_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            GC2355_HUAQUAN_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            GC2355_HUAQUAN_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
	case SENSOR_FEATURE_GET_SENSOR_CURRENT_MID://cuirui add for MID
		*pFeatureReturnPara32=GC2355HUAQUANGetModuleID();
		*pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_INITIALIZE_AF:
            break;
        case SENSOR_FEATURE_CONSTANT_AF:
            break;
        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            GC2355_HUAQUANSetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            GC2355_HUAQUANGetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            GC2355_HUAQUANSetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            GC2355_HUAQUANSetTestPatternMode((BOOL)*pFeatureData16);
            break;
	    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
			*pFeatureReturnPara32=GC2355_HUAQUAN_TEST_PATTERN_CHECKSUM;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			GC2355_HUAQUANMIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			GC2355_HUAQUANMIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
        default:
            break;
    }
    return ERROR_NONE;
}	/* GC2355_HUAQUANFeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncGC2355_HUAQUAN=
{
    GC2355_HUAQUANOpen,
    GC2355_HUAQUANGetInfo,
    GC2355_HUAQUANGetResolution,
    GC2355_HUAQUANFeatureControl,
    GC2355_HUAQUANControl,
    GC2355_HUAQUANClose
};

UINT32 GC2355_HUAQUAN_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncGC2355_HUAQUAN;

    return ERROR_NONE;
}   /* SensorInit() */
