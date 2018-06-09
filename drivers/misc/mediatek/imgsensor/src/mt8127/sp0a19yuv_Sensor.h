/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*/

#ifndef __SP0A19_SENSOR_H
#define __SP0A19_SENSOR_H


#define VGA_PERIOD_PIXEL_NUMS						(640-8)//784//694
#define VGA_PERIOD_LINE_NUMS						(480-6)//510//488

#define IMAGE_SENSOR_VGA_GRAB_PIXELS			0
#define IMAGE_SENSOR_VGA_GRAB_LINES			1

#define IMAGE_SENSOR_VGA_WIDTH					(640-8)
#define IMAGE_SENSOR_VGA_HEIGHT					(480-6)

#define IMAGE_SENSOR_PV_WIDTH					(640-8)
#define IMAGE_SENSOR_PV_HEIGHT					(480-6)

#define IMAGE_SENSOR_FULL_WIDTH					(640-8)
#define IMAGE_SENSOR_FULL_HEIGHT					(480-6)

#define SP0A19_WRITE_ID							0x42
#define SP0A19_READ_ID								0x43
#define SP0A19_SENSOR_ID                                              0xa6
typedef enum
{
	SP0A19_RGB_Gamma_m1 = 0,
	SP0A19_RGB_Gamma_m2,
	SP0A19_RGB_Gamma_m3,
	SP0A19_RGB_Gamma_m4,
	SP0A19_RGB_Gamma_m5,
	SP0A19_RGB_Gamma_m6,
	SP0A19_RGB_Gamma_night
}SP0A19_GAMMA_TAG;

typedef enum
{
	CHT_806C_2 = 1,
	CHT_808C_2,
	LY_982A_H114,
	XY_046A,
	XY_0620,
	XY_078V,
	YG1001A_F
}SP0A19_LENS_TAG;

UINT32 SP0A19Open(void);
UINT32 SP0A19Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 SP0A19FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 SP0A19GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 SP0A19GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 SP0A19Close(void);

#endif /* __SENSOR_H */

