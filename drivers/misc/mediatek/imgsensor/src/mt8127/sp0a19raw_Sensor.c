/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   RAW
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   Leo Lee
 *
 *============================================================================
 *	HISTORY
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 04 10  2013
 * First release MT6589 SP0A19 driver Version 1.0
 *
 *------------------------------------------------------------------------------
 *============================================================================
 ****************************************************************************/

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

#include "sp0a19raw_Sensor.h"

#define SP0A19_DEBUG
#ifdef SP0A19_DEBUG
#define SENSORDB(fmt, arg...) printk( "[SP0A19Raw] "  fmt, ##arg)
#else
#define SENSORDB(x,...)
#endif

#if defined(MT6577)||defined(MT6589)
static DEFINE_SPINLOCK(sp0a19_drv_lock);
#endif

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);


static SP0A19_sensor_struct SP0A19_sensor =
{
	.eng_info =
	{
		.SensorId = 128,
		.SensorType = CMOS_SENSOR,
		.SensorOutputDataFormat = SP0A19_COLOR_FORMAT,
	},
	.Mirror = SP0A19_IMAGE_H_MIRROR,
	.shutter = 0x20,
	.gain = 0x20,
	.pclk = SP0A19_PREVIEW_CLK,
	.frame_height = SP0A19_PV_PERIOD_LINE_NUMS,
	.line_length = SP0A19_PV_PERIOD_PIXEL_NUMS,
};

static kal_uint16 moduleid = 0x00;

/*************************************************************************
* FUNCTION
*    write_cmos_sensor
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
static void write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	kal_uint8 out_buff[2];

	out_buff[0] = addr;
	out_buff[1] = para;

	iWriteRegI2C((u8*)out_buff , (u16)sizeof(out_buff), SP0A19_WRITE_ID);
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
static kal_uint8 read_cmos_sensor(kal_uint8 addr)
{
	kal_uint8 in_buff[1] = {0xFF};
	kal_uint8 out_buff[1];

	out_buff[0] = addr;

	if (0 != iReadRegI2C((u8*)out_buff , (u16) sizeof(out_buff), (u8*)in_buff, (u16) sizeof(in_buff), SP0A19_WRITE_ID)) {
	SENSORDB("ERROR: read_cmos_sensor \n");
	}
	return in_buff[0];
}

/*************************************************************************
* FUNCTION
*	SP0A19_SetShutter
*
* DESCRIPTION
*	This function set e-shutter of SP0A19 to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SP0A19_set_shutter(kal_uint16 shutter)
{


#ifdef SP0A19_DRIVER_TRACE
	SENSORDB("SP0A19_set_shutter iShutter = %d \n",shutter);
#endif
    unsigned long flags;
	if (shutter < 7)
		shutter = 7;
	else if(shutter > 0x1fff)
		shutter = 0x1fff;
#if defined(MT6577)||defined(MT6589)
	spin_lock_irqsave(&sp0a19_drv_lock, flags);
#endif
	SP0A19_sensor.shutter = shutter;
#if defined(MT6577)||defined(MT6589)
	spin_unlock_irqrestore(&sp0a19_drv_lock, flags);
#endif
	//Update Shutter
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x04, shutter  & 0xFF);

	SENSORDB("Exit! shutter =%d\n", shutter);

}   /*  Set_SP0A19_Shutter */

/*************************************************************************
* FUNCTION
*	SP0A19_SetGain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
#define ANALOG_GAIN_1 64  // 1.00x
#define ANALOG_GAIN_2 88  // 1.375x
#define ANALOG_GAIN_3 122  // 1.90x
#define ANALOG_GAIN_4 168  // 2.625x
#define ANALOG_GAIN_5 239  // 3.738x
#define ANALOG_GAIN_6 330  // 5.163x
#define ANALOG_GAIN_7 470  // 7.350x

kal_uint16 SP0A19_SetGain(kal_uint16 gain)
{
#ifdef SP0A19_DRIVER_TRACE
	SENSORDB("SP0A19_SetGain iGain = %d BASEGAIN = %d\n",gain,BASEGAIN);
#endif
	#if 1
	kal_uint8  iReg;

	if(gain >= BASEGAIN && gain <= 15*BASEGAIN)
	{
		iReg = 0x10 * gain/BASEGAIN;	//change mtk gain base to aptina gain base
	//iReg+= ((gain%BASEGAIN) * BASEGAIN)/0x10;

		if(iReg<=0x10)
		{
			write_cmos_sensor(0xfd, 0x00);
			write_cmos_sensor(0x24, 0x10);//0x23
			SENSORDB("SP0A09MIPI_SetGain = 16");
		}
		else if(iReg>= 0xa0)//gpw
		{
			write_cmos_sensor(0xfd, 0x00);
			write_cmos_sensor(0x24,0xa0);
			SENSORDB("SP0A09MIPI_SetGain = 160");
	}
		else
		{
			write_cmos_sensor(0xfd, 0x00);
			write_cmos_sensor(0x24, (kal_uint8)iReg);
			SENSORDB("SP0A09MIPI_SetGain = %d",iReg);
	}
	}
	else
		SENSORDB("error gain setting");
	return gain;

	#endif
}
/*************************************************************************
* FUNCTION
*	SP0A19_NightMode
*
* DESCRIPTION
*	This function night mode of SP0A19.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SP0A19_night_mode(kal_bool enable)
{
/*No Need to implement this function*/
#if 0
	const kal_uint16 dummy_pixel = SP0A19_sensor.line_length - SP0A19_PV_PERIOD_PIXEL_NUMS;
	const kal_uint16 pv_min_fps =  enable ? SP0A19_sensor.night_fps : SP0A19_sensor.normal_fps;
	kal_uint16 dummy_line = SP0A19_sensor.frame_height - SP0A19_PV_PERIOD_LINE_NUMS;
	kal_uint16 max_exposure_lines;

	printk("[SP0A19_night_mode]enable=%d",enable);
	if (!SP0A19_sensor.video_mode) return;
	max_exposure_lines = SP0A19_sensor.pclk * SP0A19_FPS(1) / (pv_min_fps * SP0A19_sensor.line_length);
	if (max_exposure_lines > SP0A19_sensor.frame_height) /* fix max frame rate, AE table will fix min frame rate */
//	{
//	dummy_line = max_exposure_lines - SP0A19_PV_PERIOD_LINE_NUMS;
//	}
#endif
}   /*  SP0A19_NightMode    */


/* write camera_para to sensor register */
static void SP0A19_camera_para_to_sensor(void)
{
  kal_uint32 i;
#ifdef SP0A19_DRIVER_TRACE
	SENSORDB("SP0A19_camera_para_to_sensor\n");
#endif
  for (i = 0; 0xFFFFFFFF != SP0A19_sensor.eng.reg[i].Addr; i++)
  {
    write_cmos_sensor(SP0A19_sensor.eng.reg[i].Addr, SP0A19_sensor.eng.reg[i].Para);
  }
  for (i = SP0A19_FACTORY_START_ADDR; 0xFFFFFFFF != SP0A19_sensor.eng.reg[i].Addr; i++)
  {
    write_cmos_sensor(SP0A19_sensor.eng.reg[i].Addr, SP0A19_sensor.eng.reg[i].Para);
  }
  SP0A19_SetGain(SP0A19_sensor.gain); /* update gain */
}

/* update camera_para from sensor register */
static void SP0A19_sensor_to_camera_para(void)
{
  kal_uint32 i,temp_data;
#ifdef SP0A19_DRIVER_TRACE
   SENSORDB("SP0A19_sensor_to_camera_para\n");
#endif
  for (i = 0; 0xFFFFFFFF != SP0A19_sensor.eng.reg[i].Addr; i++)
  {
	temp_data = read_cmos_sensor(SP0A19_sensor.eng.reg[i].Addr);
#if defined(MT6577)||defined(MT6589)
	spin_lock(&sp0a19_drv_lock);
#endif
    SP0A19_sensor.eng.reg[i].Para = temp_data;
#if defined(MT6577)||defined(MT6589)
	spin_unlock(&sp0a19_drv_lock);
#endif
  }
  for (i = SP0A19_FACTORY_START_ADDR; 0xFFFFFFFF != SP0A19_sensor.eng.reg[i].Addr; i++)
  {
	temp_data = read_cmos_sensor(SP0A19_sensor.eng.reg[i].Addr);
#if defined(MT6577)||defined(MT6589)
	spin_lock(&sp0a19_drv_lock);
#endif
    SP0A19_sensor.eng.reg[i].Para = temp_data;
#if defined(MT6577)||defined(MT6589)
	spin_unlock(&sp0a19_drv_lock);
#endif
  }
}

/* ------------------------ Engineer mode ------------------------ */
inline static void SP0A19_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{
#ifdef SP0A19_DRIVER_TRACE
   SENSORDB("SP0A19_get_sensor_group_count\n");
#endif
  *sensor_count_ptr = SP0A19_GROUP_TOTAL_NUMS;
}

inline static void SP0A19_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{
#ifdef SP0A19_DRIVER_TRACE
   SENSORDB("SP0A19_get_sensor_group_info\n");
#endif
  switch (para->GroupIdx)
  {
  case SP0A19_PRE_GAIN:
    sprintf(para->GroupNamePtr, "CCT");
    para->ItemCount = 5;
    break;
  case SP0A19_CMMCLK_CURRENT:
    sprintf(para->GroupNamePtr, "CMMCLK Current");
    para->ItemCount = 1;
    break;
  case SP0A19_FRAME_RATE_LIMITATION:
    sprintf(para->GroupNamePtr, "Frame Rate Limitation");
    para->ItemCount = 2;
    break;
  case SP0A19_REGISTER_EDITOR:
    sprintf(para->GroupNamePtr, "Register Editor");
    para->ItemCount = 2;
    break;
  default:
    ASSERT(0);
  }
}

inline static void SP0A19_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

  const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
  const static kal_char *editer_item_name[] = {"REG addr", "REG value"};

#ifdef SP0A19_DRIVER_TRACE
	SENSORDB("SP0A19_get_sensor_item_info\n");
#endif
  switch (para->GroupIdx)
  {
  case SP0A19_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case SP0A19_SENSOR_BASEGAIN:
    case SP0A19_PRE_GAIN_R_INDEX:
    case SP0A19_PRE_GAIN_Gr_INDEX:
    case SP0A19_PRE_GAIN_Gb_INDEX:
    case SP0A19_PRE_GAIN_B_INDEX:
      break;
    default:
      ASSERT(0);
    }
    sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - SP0A19_SENSOR_BASEGAIN]);
    para->ItemValue = SP0A19_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
    para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
    para->Min = SP0A19_MIN_ANALOG_GAIN * 1000;
    para->Max = SP0A19_MAX_ANALOG_GAIN * 1000;
    break;
  case SP0A19_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
      switch (SP0A19_sensor.eng.reg[SP0A19_CMMCLK_CURRENT_INDEX].Para)
      {
      case ISP_DRIVING_2MA:
	para->ItemValue = 2;
	break;
      case ISP_DRIVING_4MA:
	para->ItemValue = 4;
	break;
      case ISP_DRIVING_6MA:
	para->ItemValue = 6;
	break;
      case ISP_DRIVING_8MA:
	para->ItemValue = 8;
	break;
      default:
	ASSERT(0);
      }
      para->IsTrueFalse = para->IsReadOnly = KAL_FALSE;
      para->IsNeedRestart = KAL_TRUE;
      para->Min = 2;
      para->Max = 8;
      break;
    default:
      ASSERT(0);
    }
    break;
  case SP0A19_FRAME_RATE_LIMITATION:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Max Exposure Lines");
      para->ItemValue = 5998;
      break;
    case 1:
      sprintf(para->ItemNamePtr, "Min Frame Rate");
      para->ItemValue = 5;
      break;
    default:
      ASSERT(0);
    }
    para->IsTrueFalse = para->IsNeedRestart = KAL_FALSE;
    para->IsReadOnly = KAL_TRUE;
    para->Min = para->Max = 0;
    break;
  case SP0A19_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
    case 0:
    case 1:
      sprintf(para->ItemNamePtr, editer_item_name[para->ItemIdx]);
      para->ItemValue = 0;
      para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
      para->Min = 0;
      para->Max = (para->ItemIdx == 0 ? 0xFFFF : 0xFF);
      break;
    default:
      ASSERT(0);
    }
    break;
  default:
    ASSERT(0);
  }
}

inline static kal_bool SP0A19_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
  kal_uint16 temp_para;
#ifdef SP0A19_DRIVER_TRACE
   SENSORDB("SP0A19_set_sensor_item_info\n");
#endif
  switch (para->GroupIdx)
  {
  case SP0A19_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case SP0A19_SENSOR_BASEGAIN:
    case SP0A19_PRE_GAIN_R_INDEX:
    case SP0A19_PRE_GAIN_Gr_INDEX:
    case SP0A19_PRE_GAIN_Gb_INDEX:
    case SP0A19_PRE_GAIN_B_INDEX:
#if defined(MT6577)||defined(MT6589)
		spin_lock(&sp0a19_drv_lock);
#endif
      SP0A19_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
#if defined(MT6577)||defined(MT6589)
	spin_unlock(&sp0a19_drv_lock);
#endif
      SP0A19_SetGain(SP0A19_sensor.gain); /* update gain */
      break;
    default:
      ASSERT(0);
    }
    break;
  case SP0A19_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      switch (para->ItemValue)
      {
      case 2:
	temp_para = ISP_DRIVING_2MA;
	break;
      case 3:
      case 4:
	temp_para = ISP_DRIVING_4MA;
	break;
      case 5:
      case 6:
	temp_para = ISP_DRIVING_6MA;
	break;
      default:
	temp_para = ISP_DRIVING_8MA;
	break;
      }
      //SP0A19_set_isp_driving_current(temp_para);
#if defined(MT6577)||defined(MT6589)
      spin_lock(&sp0a19_drv_lock);
#endif
      SP0A19_sensor.eng.reg[SP0A19_CMMCLK_CURRENT_INDEX].Para = temp_para;
#if defined(MT6577)||defined(MT6589)
	spin_unlock(&sp0a19_drv_lock);
#endif
      break;
    default:
      ASSERT(0);
    }
    break;
  case SP0A19_FRAME_RATE_LIMITATION:
    ASSERT(0);
    break;
  case SP0A19_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
      static kal_uint32 fac_sensor_reg;
    case 0:
      if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
      fac_sensor_reg = para->ItemValue;
      break;
    case 1:
      if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
      write_cmos_sensor(fac_sensor_reg, para->ItemValue);
      break;
    default:
      ASSERT(0);
    }
    break;
  default:
    ASSERT(0);
  }
  return KAL_TRUE;
}

void SP0A19_SetMirrorFlip(SP0A19_IMAGE_MIRROR Mirror)
{
}

static void SP0A19_Sensor_Init(void)
{
	SENSORDB("sensor_init start \n");
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x32,0x00);
	write_cmos_sensor(0x06,0x00);
	write_cmos_sensor(0x09,0x00);
	write_cmos_sensor(0x0a,0x14);
	write_cmos_sensor(0x0f,0x2f);
	write_cmos_sensor(0x10,0x2e);
	write_cmos_sensor(0x11,0x00);
	write_cmos_sensor(0x12,0x18);
	write_cmos_sensor(0x13,0x2f);
	write_cmos_sensor(0x14,0x00);
	write_cmos_sensor(0x15,0x3f);
	write_cmos_sensor(0x16,0x00);
	write_cmos_sensor(0x17,0x18);
	write_cmos_sensor(0x25,0x40);
	write_cmos_sensor(0x1a,0x0b);
	write_cmos_sensor(0x1b,0x0c);
	write_cmos_sensor(0x1e,0x0b);
	write_cmos_sensor(0x20,0x3f);
	write_cmos_sensor(0x21,0x13);
	write_cmos_sensor(0x22,0x19);
	write_cmos_sensor(0x26,0x1a);
	write_cmos_sensor(0x27,0xab);
	write_cmos_sensor(0x28,0xfd);
	write_cmos_sensor(0x30,0x00);
	write_cmos_sensor(0x31,0x00);
	write_cmos_sensor(0xfb,0x33);
	write_cmos_sensor(0x1f,0x08);
	write_cmos_sensor(0x34,0x00);
	write_cmos_sensor(0x33,0x00);
	write_cmos_sensor(0x5f,0x51);
	write_cmos_sensor(0x35,0x20);
	write_cmos_sensor(0x1C,0x28);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x48,0x00);
	write_cmos_sensor(0x49,0x01);
	write_cmos_sensor(0x4a,0xe0);
	write_cmos_sensor(0x4c,0x00);
	write_cmos_sensor(0x4d,0x02);
	write_cmos_sensor(0x4e,0x80);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0xf4,0x0f);

	SENSORDB("sensor_init end \n" );
}   /*  SP0A19_Sensor_Init  */

/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*	SP0A19Open
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

UINT32 SP0A19Open(void)
{
    kal_uint32 rc = 0;
    int  retry = 0;
    kal_uint16 sensorgain;
    // check if sensor ID correct
    retry = 3;
    kal_uint16 sensor_id = 0;
	mt_set_gpio_mode(GPIO_CAMERA_CMPDN1_PIN,GPIO_CAMERA_CMPDN1_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CAMERA_CMPDN1_PIN,GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAMERA_CMPDN1_PIN,GPIO_OUT_ZERO);
	msleep(10);

    do {
	// check if sensor ID correct
		write_cmos_sensor(0xfd, 0x00);
		sensor_id = read_cmos_sensor(0x02);
		SENSORDB("sp0a19raw Read Sensor ID   = 0x%04x\n", sensor_id);
		if (sensor_id == SP0A19_SENSOR_ID)
		{
//		camera_pdn1_reverse = 1;
		break;
		}
		retry--;
    } while (retry > 0);
    if (sensor_id != SP0A19_SENSOR_ID) {
	sensor_id = 0xFFFFFFFF;
		SENSORDB("sp0a19raw Read Sensor ID fail id  = 0x%04x\n", sensor_id);
	return ERROR_SENSOR_CONNECT_FAIL;
    }

	/* initail sequence write in  */
	SP0A19_Sensor_Init();

//	SP0A19_SetMirrorFlip(SP0A19_sensor.Mirror);

	return ERROR_NONE;
}   /* SP0A19Open  */

UINT32 SP0A19GetModuleID()
{
	printk("SP0A19GetModuleID\n");
	return moduleid;
}

/*************************************************************************
* FUNCTION
*   SP0A19GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID
*
* PARAMETERS
*   *sensorID : return the sensor ID
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 SP0A19GetSensorID(UINT32 *sensorID)
{
	// check if sensor ID correct
    kal_uint16 sensor_id=0;
    int retry=3;

     SENSORDB("SP0A19GetSensorID \n");

	// check if sensor ID correct
	do {

		write_cmos_sensor(0xfd,0x00);
		sensor_id=read_cmos_sensor(0x02);
		if (sensor_id == SP0A19_SENSOR_ID) {
		moduleid = 0x03;
		SENSORDB("sp0a19raw Read Sensor ID   = 0x%04x\n", sensor_id);

		break;
	}
	SENSORDB("Read Sensor ID Fail = 0x%x\n", sensor_id);

	retry--;
	}while (retry > 0);

	if (sensor_id != SP0A19_SENSOR_ID) {

		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

      *sensorID = sensor_id;
   return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	SP0A19Close
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
UINT32 SP0A19Close(void)
{
#ifdef SP0A19_DRIVER_TRACE
   SENSORDB("SP0A19Close\n");
#endif
  //kdCISModulePowerOn(SensorIdx,currSensorName,0,mode_name);
//	DRV_I2CClose(SP0A19hDrvI2C);
	return ERROR_NONE;
}   /* SP0A19Close */

/*************************************************************************
* FUNCTION
* SP0A19Preview
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
UINT32 SP0A19Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
#if defined(MT6577)||defined(MT6589)
	spin_lock(&sp0a19_drv_lock);
#endif
	SP0A19_sensor.pv_mode = KAL_TRUE;

	//SP0A19_set_mirror(sensor_config_data->SensorImageMirror);
	switch (sensor_config_data->SensorOperationMode)
	{
	case MSDK_SENSOR_OPERATION_MODE_VIDEO:
		SP0A19_sensor.video_mode = KAL_TRUE;
	default: /* ISP_PREVIEW_MODE */
		SP0A19_sensor.video_mode = KAL_FALSE;
	}
	SP0A19_sensor.line_length = SP0A19_PV_PERIOD_PIXEL_NUMS;
	SP0A19_sensor.frame_height = SP0A19_PV_PERIOD_LINE_NUMS;
	SP0A19_sensor.pclk = SP0A19_PREVIEW_CLK;
#if defined(MT6577)||defined(MT6589)
	spin_unlock(&sp0a19_drv_lock);
#endif
	//SP0A19_Write_Shutter(SP0A19_sensor.shutter);
	return ERROR_NONE;
}   /*  SP0A19Preview   */

/*************************************************************************
* FUNCTION
*	SP0A19Capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 SP0A19Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 shutter = (kal_uint32)SP0A19_sensor.shutter;
#if defined(MT6577)||defined(MT6589)
	spin_lock(&sp0a19_drv_lock);
#endif
	SP0A19_sensor.video_mode = KAL_FALSE;
	SP0A19_sensor.pv_mode = KAL_FALSE;
#if defined(MT6577)||defined(MT6589)
	spin_unlock(&sp0a19_drv_lock);
#endif
	return ERROR_NONE;
}   /* SP0A19_Capture() */

UINT32 SP0A19GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	pSensorResolution->SensorFullWidth=SP0A19_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight=SP0A19_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorPreviewWidth=SP0A19_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight=SP0A19_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorVideoWidth	= SP0A19_IMAGE_SENSOR_VIDEO_WIDTH;
	pSensorResolution->SensorVideoHeight    = SP0A19_IMAGE_SENSOR_VIDEO_HEIGHT;
	return ERROR_NONE;
}	/* SP0A19GetResolution() */

UINT32 SP0A19GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
					MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
					MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	pSensorInfo->SensorPreviewResolutionX=SP0A19_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY=SP0A19_IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX=SP0A19_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY=SP0A19_IMAGE_SENSOR_FULL_HEIGHT;

	pSensorInfo->SensorCameraPreviewFrameRate=30;
	pSensorInfo->SensorVideoFrameRate=30;
	pSensorInfo->SensorStillCaptureFrameRate=10;
	pSensorInfo->SensorWebCamCaptureFrameRate=15;
	pSensorInfo->SensorResetActiveHigh=TRUE; //low active
	pSensorInfo->SensorResetDelayCount=5;
	pSensorInfo->SensorOutputDataFormat=SP0A19_COLOR_FORMAT;
	pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_HIGH;
	pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_PARALLEL;

	pSensorInfo->CaptureDelayFrame = 2;
	pSensorInfo->PreviewDelayFrame = 1;
	pSensorInfo->VideoDelayFrame = 1;

	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;
	pSensorInfo->AEShutDelayFrame =0;		/* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 0;   /* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 1;

	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
			pSensorInfo->SensorGrabStartX = SP0A19_PV_X_START;
			pSensorInfo->SensorGrabStartY = SP0A19_PV_Y_START;

		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount= 3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
			pSensorInfo->SensorGrabStartX = SP0A19_FULL_X_START;
			pSensorInfo->SensorGrabStartY = SP0A19_FULL_Y_START;
		break;
		default:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
			pSensorInfo->SensorGrabStartX = SP0A19_PV_X_START;
			pSensorInfo->SensorGrabStartY = SP0A19_PV_Y_START;
		break;
	}
	memcpy(pSensorConfigData, &SP0A19_sensor.cfg_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
  return ERROR_NONE;
}	/* SP0A19GetInfo() */


UINT32 SP0A19Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
					MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

#ifdef SP0A19_DRIVER_TRACE
	SENSORDB("SP0A19Control ScenarioId = %d \n",ScenarioId);
#endif
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			SP0A19Preview(pImageWindow, pSensorConfigData);
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			SP0A19Capture(pImageWindow, pSensorConfigData);
		break;
	default:
	return ERROR_INVALID_SCENARIO_ID;
	}
	return TRUE;
}	/* SP0A19Control() */



UINT32 SP0A19SetVideoMode(UINT16 u2FrameRate)
{};

UINT32 SP0A19FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
							UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
	//UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
	//UINT32 SP0A19SensorRegNumber;
	//UINT32 i;
	//PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
	//MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	//MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
	//MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
	//MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

	switch (FeatureId)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pFeatureReturnPara16++=SP0A19_IMAGE_SENSOR_FULL_WIDTH;
			*pFeatureReturnPara16=SP0A19_IMAGE_SENSOR_FULL_HEIGHT;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_PERIOD:	/* 3 */
			*pFeatureReturnPara16++=SP0A19_sensor.line_length;
			*pFeatureReturnPara16=SP0A19_sensor.frame_height;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:  /* 3 */
			*pFeatureReturnPara32 = SP0A19_sensor.pclk;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_SET_ESHUTTER:	/* 4 */
			SP0A19_set_shutter(*pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			SP0A19_night_mode((BOOL) *pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_GAIN:	/* 6 */
			SP0A19_SetGain((UINT16) *pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
		case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
		case SENSOR_FEATURE_GET_REGISTER:
			pSensorRegData->RegData = read_cmos_sensor(pSensorRegData->RegAddr);
		break;
		case SENSOR_FEATURE_SET_CCT_REGISTER:
			memcpy(&SP0A19_sensor.eng.cct, pFeaturePara, sizeof(SP0A19_sensor.eng.cct));
			break;
		break;
		case SENSOR_FEATURE_GET_CCT_REGISTER:	/* 12 */
			if (*pFeatureParaLen >= sizeof(SP0A19_sensor.eng.cct) + sizeof(kal_uint32))
			{
			*((kal_uint32 *)pFeaturePara++) = sizeof(SP0A19_sensor.eng.cct);
			memcpy(pFeaturePara, &SP0A19_sensor.eng.cct, sizeof(SP0A19_sensor.eng.cct));
			}
			break;
		case SENSOR_FEATURE_SET_ENG_REGISTER:
			memcpy(&SP0A19_sensor.eng.reg, pFeaturePara, sizeof(SP0A19_sensor.eng.reg));
			break;
		case SENSOR_FEATURE_GET_ENG_REGISTER:	/* 14 */
			if (*pFeatureParaLen >= sizeof(SP0A19_sensor.eng.reg) + sizeof(kal_uint32))
			{
			*((kal_uint32 *)pFeaturePara++) = sizeof(SP0A19_sensor.eng.reg);
			memcpy(pFeaturePara, &SP0A19_sensor.eng.reg, sizeof(SP0A19_sensor.eng.reg));
			}
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = SP0A19_SENSOR_ID;
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &SP0A19_sensor.eng.reg, sizeof(SP0A19_sensor.eng.reg));
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &SP0A19_sensor.eng.cct, sizeof(SP0A19_sensor.eng.cct));
			*pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
			break;
		case SENSOR_FEATURE_GET_CONFIG_PARA:
			memcpy(pFeaturePara, &SP0A19_sensor.cfg_data, sizeof(SP0A19_sensor.cfg_data));
			*pFeatureParaLen = sizeof(SP0A19_sensor.cfg_data);
			break;
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		SP0A19_camera_para_to_sensor();
			break;
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
			SP0A19_sensor_to_camera_para();
			break;
		case SENSOR_FEATURE_GET_GROUP_COUNT:
			SP0A19_get_sensor_group_count((kal_uint32 *)pFeaturePara);
			*pFeatureParaLen = 4;
			break;
		case SENSOR_FEATURE_GET_GROUP_INFO:
			SP0A19_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *)pFeaturePara);
			*pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
			break;
		case SENSOR_FEATURE_GET_ITEM_INFO:
			SP0A19_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
			*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
			break;
		case SENSOR_FEATURE_SET_ITEM_INFO:
			SP0A19_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
			*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
			break;
		case SENSOR_FEATURE_GET_ENG_INFO:
			memcpy(pFeaturePara, &SP0A19_sensor.eng_info, sizeof(SP0A19_sensor.eng_info));
			*pFeatureParaLen = sizeof(SP0A19_sensor.eng_info);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_SENSOR_CURRENT_MID://cuirui add for MID
			*pFeatureReturnPara32=SP0A19GetModuleID();
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			//SP0A19SetVideoMode(*pFeatureData16);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			SP0A19GetSensorID(pFeatureReturnPara32);
			break;
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			break;
		default:
			break;
	}
	return ERROR_NONE;
}	/* SP0A19FeatureControl() */
SENSOR_FUNCTION_STRUCT	SensorFuncSP0A19=
{
	SP0A19Open,
	SP0A19GetInfo,
	SP0A19GetResolution,
	SP0A19FeatureControl,
	SP0A19Control,
	SP0A19Close
};

UINT32 SP0A19_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&SensorFuncSP0A19;

	return ERROR_NONE;
}	/* SensorInit() */
