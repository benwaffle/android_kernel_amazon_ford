 /* MediaTek Inc. (C) 2010. All rights reserved. */ 

#ifndef _SP0A19_SENSOR_H
#define _SP0A19_SENSOR_H

#define SP0A19_DEBUG
#define SP0A19_DRIVER_TRACE
//#define SP0A19_TEST_PATTEM

#define SP0A19_FACTORY_START_ADDR 0
#define SP0A19_ENGINEER_START_ADDR 10
 
typedef enum SP0A19_group_enum
{
  SP0A19_PRE_GAIN = 0,
  SP0A19_CMMCLK_CURRENT,
  SP0A19_FRAME_RATE_LIMITATION,
  SP0A19_REGISTER_EDITOR,
  SP0A19_GROUP_TOTAL_NUMS
} SP0A19_FACTORY_GROUP_ENUM;

typedef enum SP0A19_register_index
{
  SP0A19_SENSOR_BASEGAIN = SP0A19_FACTORY_START_ADDR,
  SP0A19_PRE_GAIN_R_INDEX,
  SP0A19_PRE_GAIN_Gr_INDEX,
  SP0A19_PRE_GAIN_Gb_INDEX,
  SP0A19_PRE_GAIN_B_INDEX,
  SP0A19_FACTORY_END_ADDR
} SP0A19_FACTORY_REGISTER_INDEX;

typedef enum SP0A19_engineer_index
{
  SP0A19_CMMCLK_CURRENT_INDEX = SP0A19_ENGINEER_START_ADDR,
  SP0A19_ENGINEER_END
} SP0A19_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[SP0A19_ENGINEER_END];
  SENSOR_REG_STRUCT cct[SP0A19_FACTORY_END_ADDR];
} sensor_data_struct;

/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define SP0A19_PREVIEW_CLK                   12000000
#define SP0A19_CAPTURE_CLK                   12000000

#define SP0A19_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW8_B //SENSOR_OUTPUT_FORMAT_RAW_Gb //SENSOR_OUTPUT_FORMAT_RAW_R

#define SP0A19_MIN_ANALOG_GAIN				1	/* 1x */
#define SP0A19_MAX_ANALOG_GAIN				10	/* 2.8x */


/* FRAME RATE UNIT */
#define SP0A19_FPS(x)                          (10 * (x))

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */

#define SP0A19_FULL_PERIOD_PIXEL_NUMS          871
#define SP0A19_FULL_PERIOD_LINE_NUMS           510

#define SP0A19_VIDEO_PERIOD_PIXEL_NUMS          871
#define SP0A19_VIDEO_PERIOD_LINE_NUMS           510

#define SP0A19_PV_PERIOD_PIXEL_NUMS            871
#define SP0A19_PV_PERIOD_LINE_NUMS             510

/* SENSOR START/END POSITION */
#define SP0A19_FULL_X_START                    0   
#define SP0A19_FULL_Y_START                    0  
#define SP0A19_IMAGE_SENSOR_FULL_WIDTH         (640) 
#define SP0A19_IMAGE_SENSOR_FULL_HEIGHT        (480) 

#define SP0A19_VIDEO_X_START                      0
#define SP0A19_VIDEO_Y_START                      0
#define SP0A19_IMAGE_SENSOR_VIDEO_WIDTH           (640) 
#define SP0A19_IMAGE_SENSOR_VIDEO_HEIGHT          (480) 

#define SP0A19_PV_X_START                      0
#define SP0A19_PV_Y_START                      0
#define SP0A19_IMAGE_SENSOR_PV_WIDTH           (640) 
#define SP0A19_IMAGE_SENSOR_PV_HEIGHT          (480)

/* SENSOR READ/WRITE ID */
#define SP0A19_WRITE_ID (0x42)
#define SP0A19_READ_ID  (0x43)

/* SENSOR ID */
//#define SP0A19_SENSOR_ID						(0x2355)

/* SENSOR PRIVATE STRUCT */
typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE
} SP0A19_SENSOR_MODE;

typedef enum{
	SP0A19_IMAGE_NORMAL = 0,
	SP0A19_IMAGE_H_MIRROR,
	SP0A19_IMAGE_V_MIRROR,
	SP0A19_IMAGE_HV_MIRROR
}SP0A19_IMAGE_MIRROR;

typedef struct SP0A19_sensor_STRUCT
{
	MSDK_SENSOR_CONFIG_STRUCT cfg_data;
	sensor_data_struct eng; /* engineer mode */
	MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
	SP0A19_SENSOR_MODE sensorMode;
	SP0A19_IMAGE_MIRROR Mirror;
	kal_bool pv_mode;
	kal_bool video_mode;
	kal_bool NightMode;
	kal_uint16 normal_fps; /* video normal mode max fps */
	kal_uint16 night_fps; /* video night mode max fps */
	kal_uint16 FixedFps;
	kal_uint16 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint16 frame_height;
	kal_uint16 frame_height_BackUp;
	kal_uint16 line_length;  
	kal_uint16 Prv_line_length;
} SP0A19_sensor_struct;

typedef enum SP0A19_GainMode_Index
{
	SP0A19_Analogic_Gain = 0,
	SP0A19_Digital_Gain
}SP0A19_GainMode_Index;
//export functions
UINT32 SP0A19Open(void);
UINT32 SP0A19Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 SP0A19FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 SP0A19GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 SP0A19GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 SP0A19Close(void);

#define Sleep(ms) mdelay(ms)

#endif 
