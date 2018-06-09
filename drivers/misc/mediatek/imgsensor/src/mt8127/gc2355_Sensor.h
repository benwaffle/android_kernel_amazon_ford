 /* MediaTek Inc. (C) 2010. All rights reserved. */ 

#ifndef _GC2355_SENSOR_H
#define _GC2355_SENSOR_H

#define GC2355_DEBUG
#define GC2355_DRIVER_TRACE
//#define GC2355_TEST_PATTEM

#define GC2355_FACTORY_START_ADDR 0
#define GC2355_ENGINEER_START_ADDR 10
 
typedef enum GC2355_group_enum
{
  GC2355_PRE_GAIN = 0,
  GC2355_CMMCLK_CURRENT,
  GC2355_FRAME_RATE_LIMITATION,
  GC2355_REGISTER_EDITOR,
  GC2355_GROUP_TOTAL_NUMS
} GC2355_FACTORY_GROUP_ENUM;

typedef enum GC2355_register_index
{
  GC2355_SENSOR_BASEGAIN = GC2355_FACTORY_START_ADDR,
  GC2355_PRE_GAIN_R_INDEX,
  GC2355_PRE_GAIN_Gr_INDEX,
  GC2355_PRE_GAIN_Gb_INDEX,
  GC2355_PRE_GAIN_B_INDEX,
  GC2355_FACTORY_END_ADDR
} GC2355_FACTORY_REGISTER_INDEX;

typedef enum GC2355_engineer_index
{
  GC2355_CMMCLK_CURRENT_INDEX = GC2355_ENGINEER_START_ADDR,
  GC2355_ENGINEER_END
} GC2355_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[GC2355_ENGINEER_END];
  SENSOR_REG_STRUCT cct[GC2355_FACTORY_END_ADDR];
} sensor_data_struct;

/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define GC2355_PREVIEW_CLK                   42000000
#define GC2355_CAPTURE_CLK                   42000000

#define GC2355_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_B //SENSOR_OUTPUT_FORMAT_RAW_Gb //SENSOR_OUTPUT_FORMAT_RAW_R

#define GC2355_MIN_ANALOG_GAIN				1	/* 1x */
#define GC2355_MAX_ANALOG_GAIN				4	/* 2.8x */


/* FRAME RATE UNIT */
#define GC2355_FPS(x)                          (10 * (x))

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */

#define GC2355_FULL_PERIOD_PIXEL_NUMS          1126
#define GC2355_FULL_PERIOD_LINE_NUMS           1242

#define GC2355_VIDEO_PERIOD_PIXEL_NUMS          1126
#define GC2355_VIDEO_PERIOD_LINE_NUMS           1242

#define GC2355_PV_PERIOD_PIXEL_NUMS            1126
#define GC2355_PV_PERIOD_LINE_NUMS             1242

/* SENSOR START/END POSITION */
#define GC2355_FULL_X_START                    0   
#define GC2355_FULL_Y_START                    0  
#define GC2355_IMAGE_SENSOR_FULL_WIDTH         (1600 - 8) 
#define GC2355_IMAGE_SENSOR_FULL_HEIGHT        (1200 - 6) 

#define GC2355_VIDEO_X_START                      0
#define GC2355_VIDEO_Y_START                      0
#define GC2355_IMAGE_SENSOR_VIDEO_WIDTH           (1600) 
#define GC2355_IMAGE_SENSOR_VIDEO_HEIGHT          (1200) 

#define GC2355_PV_X_START                      0
#define GC2355_PV_Y_START                      0
#define GC2355_IMAGE_SENSOR_PV_WIDTH           (1600 - 8) 
#define GC2355_IMAGE_SENSOR_PV_HEIGHT          (1200 - 6)

/* SENSOR READ/WRITE ID */
#define GC2355_WRITE_ID (0x78)
#define GC2355_READ_ID  (0x79)

/* SENSOR ID */
//#define GC2355_SENSOR_ID						(0x2355)

/* SENSOR PRIVATE STRUCT */
typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE
} GC2355_SENSOR_MODE;

typedef enum{
	GC2355_IMAGE_NORMAL = 0,
	GC2355_IMAGE_H_MIRROR,
	GC2355_IMAGE_V_MIRROR,
	GC2355_IMAGE_HV_MIRROR
}GC2355_IMAGE_MIRROR;

typedef struct GC2355_sensor_STRUCT
{
	MSDK_SENSOR_CONFIG_STRUCT cfg_data;
	sensor_data_struct eng; /* engineer mode */
	MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
	GC2355_SENSOR_MODE sensorMode;
	GC2355_IMAGE_MIRROR Mirror;
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
} GC2355_sensor_struct;

typedef enum GC2355_GainMode_Index
{
	GC2355_Analogic_Gain = 0,
	GC2355_Digital_Gain
}GC2355_GainMode_Index;
//export functions
UINT32 GC2355Open(void);
UINT32 GC2355Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 GC2355FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 GC2355GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 GC2355GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 GC2355Close(void);

#define Sleep(ms) mdelay(ms)

#endif 
