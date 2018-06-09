#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <linux/kernel.h>//for printk

#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"


//extern void ISP_MCLK1_EN(BOOL En);
/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         xlog_printk(ANDROID_LOG_ERR, PFX , fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...) \
                do {    \
                   xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg); \
                } while(0)
#else
#define PK_DBG(a,...)
#define PK_ERR(a,...)
#define PK_XLOG_INFO(fmt, args...)
#endif


u32 pinSetIdx = 0;//default main sensor

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4

#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3   
u32 pinSet[2][8] = {
			//for main sensor
			{
				GPIO_CAMERA_CMRST_PIN,
				GPIO_CAMERA_CMRST_PIN_M_GPIO,   /* mode */
				GPIO_OUT_ONE,                   /* ON state */
				GPIO_OUT_ZERO,                  /* OFF state */
				GPIO_CAMERA_CMPDN_PIN,
				GPIO_CAMERA_CMPDN_PIN_M_GPIO,
				GPIO_OUT_ONE,
				GPIO_OUT_ZERO,
			},
			//for sub sensor
			{
				GPIO_CAMERA_CMRST1_PIN,
				GPIO_CAMERA_CMRST1_PIN_M_GPIO,
				GPIO_OUT_ONE,
				GPIO_OUT_ZERO,
				GPIO_CAMERA_CMPDN1_PIN,
				GPIO_CAMERA_CMPDN1_PIN_M_GPIO,
				GPIO_OUT_ONE,
				GPIO_OUT_ZERO,
			},
                   };

/*static void MainCameraDigtalPowerCtrl(kal_bool on){
    if(mt_set_gpio_mode(GPIO_MAIN_CAMERA_12V_POWER_CTRL_PIN,0)){PK_DBG("[[CAMERA SENSOR] Set MAIN CAMERA_DIGITAL POWER_PIN ! \n");}
    if(mt_set_gpio_dir(GPIO_MAIN_CAMERA_12V_POWER_CTRL_PIN,GPIO_DIR_OUT)){PK_DBG("[[CAMERA SENSOR] Set CAMERA_POWER_PULL_PIN DISABLE ! \n");}
    if(mt_set_gpio_out(GPIO_MAIN_CAMERA_12V_POWER_CTRL_PIN,on)){PK_DBG("[[CAMERA SENSOR] Set CAMERA_POWER_PULL_PIN DISABLE ! \n");;}
}



#ifndef GPIO_MAIN_CAMERA_28V_POWER_CTRL_PIN 
#define GPIO_MAIN_CAMERA_28V_POWER_CTRL_PIN GPIO37
#endif 
static void MainCameraAnalogPowerCtrl(kal_bool on){
    if(mt_set_gpio_mode(GPIO_MAIN_CAMERA_28V_POWER_CTRL_PIN,0)){PK_DBG("[[CAMERA SENSOR] Set MAIN CAMERA_DIGITAL POWER_PIN ! \n");}
    if(mt_set_gpio_dir(GPIO_MAIN_CAMERA_28V_POWER_CTRL_PIN,GPIO_DIR_OUT)){PK_DBG("[[CAMERA SENSOR] Set CAMERA_POWER_PULL_PIN DISABLE ! \n");}
    if(mt_set_gpio_out(GPIO_MAIN_CAMERA_28V_POWER_CTRL_PIN,on)){PK_DBG("[[CAMERA SENSOR] Set CAMERA_POWER_PULL_PIN DISABLE ! \n");;}
}*/
static void Rst_PDN_Init(void)
{
    if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE]))
    {
        PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");
    }

    if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT))
    {
        PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");
    }

    if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE]))
    {
        PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");
    }

    if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT))
    {
        PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");
    }
}

static void disable_inactive_sensor(void)
{
    //disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]))
        {
            PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        } //low == reset sensor
        
        if(mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF]))
        {
            PK_DBG("[CAMERA LENS] set gpio failed!! \n");
        } //high == power down lens module
    }        
}

static int kd_poweron_sub_devices(MT65XX_POWER_VOLTAGE VOL_D2, MT65XX_POWER_VOLTAGE VOL_A, MT65XX_POWER_VOLTAGE VOL_D, MT65XX_POWER_VOLTAGE VOL_A2, char *mode_name)
{
    int ret = 0;

    if(VOL_D2 >= 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_D2, mode_name);

    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr0;
    }

    if(VOL_A > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_A,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr1;
    }

    /*if(VOL_D > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_D_SUB, VOL_D,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
        goto poweronerr2;
    }
    */

    if(VOL_A2 > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_A2,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
        goto poweronerr3;
    }

poweronerr3:
poweronerr2:
poweronerr1:
poweronerr0:
    return ret;
}

/*
hupeng 130116
For some IC, the VOL_D should before VOL_D2, such as s5k3h2
*/
static int kd_poweron_main_devices(MT65XX_POWER_VOLTAGE VOL_D2, MT65XX_POWER_VOLTAGE VOL_A, MT65XX_POWER_VOLTAGE VOL_D, MT65XX_POWER_VOLTAGE VOL_A2, char *mode_name,int VOL_D_first)
{
    int ret = 0;

    if(VOL_D_first)
    {
    if(VOL_D > 0)
            ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_D,mode_name);
    }
    else
    {
    if(VOL_D2 >= 0)
            ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_D2, mode_name);
    }
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr0;
    }

    if(VOL_A > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_A,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr1;
    }

    if(VOL_D_first)
    {
    if(VOL_D2 > 0)
            ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_D2,mode_name);
    }
    else
    {
    if(VOL_D >= 0)
            ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_D, mode_name);
    }
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
        goto poweronerr2;
    }

    if(VOL_A2 > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_A2,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
        goto poweronerr3;
    }

poweronerr3:
poweronerr2:
poweronerr1:
poweronerr0:
    return ret;
}


static int kd_powerdown_sub_devices(char *mode_name)
{
    int ret = 0;
    
/*  ret = hwPowerDown(CAMERA_POWER_VCAM_D_SUB, mode_name);

    if(TRUE != ret ) {
        PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
        //return -EIO;
        goto _kd_powerdown_sub_exit_;
    }
*/  
    ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
    if(TRUE != ret) {
        PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
        //return -EIO;
        goto _kd_powerdown_sub_exit_;
    }

    ret = hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name);
    if(TRUE != ret)
    {
        PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
        //return -EIO;
        goto _kd_powerdown_sub_exit_;
    }

_kd_powerdown_sub_exit_:
    return ret;
}

static int kd_powerdown_main_devices(char *mode_name)
{
    int ret = 0;

    ret = hwPowerDown(CAMERA_POWER_VCAM_D, mode_name);
    if(TRUE != ret) {
        PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
        //return -EIO;
        goto _kd_powerdown_main_exit_;
    }

    ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
    if(TRUE != ret) {
        PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
        //return -EIO;
        goto _kd_powerdown_main_exit_;
    }
    
    ret = hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name);
    if(TRUE != ret)
    {
        PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
        //return -EIO;
        goto _kd_powerdown_main_exit_;
    }

    ret = hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name);
    if(TRUE != ret )
    {
        PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
        //return -EIO;
        goto _kd_powerdown_main_exit_;
    }


_kd_powerdown_main_exit_:
    return ret;
}
#if 0

static int kd_ov5647_poweron( char *mode_name)
{
    int ret;
     PK_DBG("OV5647 kd_ov5647_poweron start..\n");

    ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr;
    }    
    mdelay(1);    
        
    ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr;
    }
    mdelay(1);

    //ret = hwPowerOn(CAMERA_POWER_VCAM_D_SUB, VOL_1500,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
        goto poweronerr;
    }

   // ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name);  //for at730 sub camera
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
        goto poweronerr;
    }    
    mdelay(10);// wait power to be stable  >5ms

        //disable inactive sensor
        if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_mode(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[1-pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_dir(pinSet[1-pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
            if(mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
            
        }        

        //enable active sensor
        //RST pin
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(10);
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(1);

            //PDN pin
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        }

    poweronerr:
         return ret;
}

static int kd_ov5647_powerdown( char *mode_name)
{
    int ret;
    PK_DBG("kdCISModulePower--off kd_ov5647_powerdown start,pinSetIdx:%d\n",pinSetIdx);
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
          }
    
    ret = kd_powerdown_sub_devices(mode_name);
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }

    PK_DBG("kdCISModulePower--off kd_ov5647_powerdown start success!\n\n\n");
    return ret;

}

/////////////////////////////////////////////////////
static int kd_gc2235mipiraw_poweron( char *mode_name)
{
    int ret;

    PK_DBG("[kd_gc2235_poweron] start,pinSetIdx:%d\n",pinSetIdx);
    //ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name);

        ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name);
        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
        ret = hwPowerOn(MT6323_POWER_LDO_VGP3, VOL_1800,mode_name);

    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;
}

static int kd_gc2235mipiraw_powerdown( char *mode_name)
{
    int ret;

    PK_DBG("[kd_gc2235_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    //ret = kd_powerdown_sub_devices(mode_name);
        ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);
        ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
        ret = hwPowerDown(MT6323_POWER_LDO_VGP3,mode_name);
       
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}
//////////////////////////////////
/////////////////////////////////////////////////////
static int kd_hi258_mipiraw_poweron( char *mode_name)
{
    int ret;

	PK_DBG("[kd_hi258_poweron] start,pinSetIdx:%d\n",pinSetIdx);
	//ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name);

	ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_2800, mode_name);
	mdelay(1);	
	ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
	mdelay(1);	
	ret = hwPowerOn(MT6323_POWER_LDO_VGP3, VOL_1800,mode_name);

	mdelay(5);// wait power to be stable  
	//disable_inactive_sensor();//disable inactive sensor
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	//PDN pin
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(15);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(10);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(5);
    }
poweronerr:
    return ret;
}

static int kd_hi258mipiraw_powerdown( char *mode_name)
{
	int ret;

	PK_DBG("[kd_gc2235_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	}
	mdelay(1);  
	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		//PDN pin
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	}
	mdelay(2);  
	//ret = kd_powerdown_sub_devices(mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_D,mode_name);
	mdelay(2);  
	ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
	mdelay(2);  
	ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);
	
poweronerr:
    return ret;
}
//////////////////////////////////
#endif

static int kd_hi544_poweron( char *mode_name)
{
    int ret;
    printk("kd_hi544_poweron start..\n");

    ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr;
    }

    ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr;
    }

    ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1200,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
        goto poweronerr;
    }

    ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
        goto poweronerr;
    }

    mdelay(10);// wait power to be stable  >5ms

        //PDN pin

        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(1);

	//mt_set_gpio_mode(GPIO119,GPIO_MODE_01);
	//mt_set_gpio_dir(GPIO119,GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO119,GPIO_OUT_ZERO); 
	//ISP_MCLK1_EN(TRUE);
	mdelay(2);

       // if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);

poweronerr:
    return ret;
}

static int kd_hi544_powerdown( char *mode_name)
{
    int ret;
	if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
		if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		}
	mdelay(10);

	//mt_set_gpio_mode(GPIO119,GPIO_MODE_01);
	//mt_set_gpio_dir(GPIO119,GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO119,GPIO_OUT_ZERO);

	//ISP_MCLK1_EN(FALSE);

	mdelay(1);
	if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	ret = kd_powerdown_main_devices(mode_name);
	
	/*
	if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {

	if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	}
	*/
    return ret;
}
static int kd_gc2355_poweron( char *mode_name)
{
    int ret;

	PK_DBG("[kd_gc2355_poweron]:----darren----start,pinSetIdx:%d\n",pinSetIdx);
	//   ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name,0);


	ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name);

	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
		goto poweronerr;
	}
	mdelay(5);
	#if 1
	ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
		goto poweronerr;
	}
	mdelay(5);
	#endif
	ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
		goto poweronerr;
	}


        mdelay(5);// wait power to be stable  
        disable_inactive_sensor();//disable inactive sensor
      
    //disable_inactive_sensor();//disable inactive sensor

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	//PDN pin
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ONE)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(10);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ZERO)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(10);
	PK_DBG("---darren_power:reset on\n");
	//if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	//mdelay(10);
	PK_DBG("---darren_power:reset off\n");
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(10);
	PK_DBG("---darren_power:reset on\n");
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(5);
    }
/*
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(10);
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
     */
poweronerr:
    return ret;
}


static int kd_gc2355_powerdown( char *mode_name)
{
	int ret;

	PK_DBG("[kd_gc2235_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		//PDN pin
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	}

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	}

	//ret = kd_powerdown_sub_devices(mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_D,mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);


	poweronerr:
	return ret;
}


static int kd_gc2035_poweron( char *mode_name)
{
    int ret;

	PK_DBG("[kd_gc2035_poweron]:----darren----start,pinSetIdx:%d\n",pinSetIdx);
	//   ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name,0);


	ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_2800,mode_name);

	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
		goto poweronerr;
	}
	mdelay(5);
	ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
		goto poweronerr;
	}
	mdelay(5);
	ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
		goto poweronerr;
	}

        mdelay(5);// wait power to be stable  
        disable_inactive_sensor();//disable inactive sensor
      
    //disable_inactive_sensor();//disable inactive sensor

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	//PDN pin
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ONE)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(10);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ZERO)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(10);
	PK_DBG("---darren_power:reset on\n");
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(10);
	PK_DBG("---darren_power:reset off\n");
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(10);
	PK_DBG("---darren_power:reset on\n");
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(5);
    }
/*
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(10);
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
     */
poweronerr:
    return ret;
}

static int kd_gc2035_powerdown( char *mode_name)
{
	int ret;

	PK_DBG("[kd_gc2235_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	}

	//ret = kd_powerdown_sub_devices(mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_D,mode_name);

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		//PDN pin
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	}
	poweronerr:
	return ret;
}

static int kd_hi257_poweron( char *mode_name)
{
	int ret;

	PK_DBG("[kd_hi257_poweron]:----darren----start,pinSetIdx:%d\n",pinSetIdx);
	//   ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name,0);


	ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
		goto poweronerr;
	}
	//mdelay(1);

	ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
		goto poweronerr;
	}
	//mdelay(1);

	ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name);
	if(ret != TRUE){
	        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
	        goto poweronerr;
	}

        mdelay(5);// wait power to be stable  

	//mt_set_gpio_mode(GPIO119,GPIO_MODE_01);
	//mt_set_gpio_dir(GPIO119,GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO119,GPIO_OUT_ZERO);
//	ISP_MCLK1_EN(TRUE);
	mdelay(1);
        disable_inactive_sensor();//disable inactive sensor
      
    //disable_inactive_sensor();//disable inactive sensor

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		//PDN pin
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ONE)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
		mdelay(5);
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ZERO)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
		mdelay(10);
		PK_DBG("---darren_power:reset on\n");
		//if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		//mdelay(10);
		PK_DBG("---darren_power:reset off\n");
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		mdelay(5);
		PK_DBG("---darren_power:reset on\n");
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		mdelay(5);
	}
/*
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(10);
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
     */
poweronerr:
    return ret;
}

static int kd_hi257_powerdown( char *mode_name)
{
	int ret;

	PK_DBG("[kd_hi257_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	}
	mdelay(2);	

	//mt_set_gpio_mode(GPIO119,GPIO_MODE_01);
	//mt_set_gpio_dir(GPIO119,GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO119,GPIO_OUT_ZERO);

	//ISP_MCLK1_EN(FALSE);

	mdelay(1);
	if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		//PDN pin
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	}
	mdelay(2);	
	//ret = kd_powerdown_sub_devices(mode_name);
	ret = hwPowerDown(CAMERA_POWER_VCAM_D,mode_name);
	mdelay(2);	
	ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
	mdelay(2);	
	ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);
	
poweronerr:
	return ret;
}
/****************************hi253yuv**********************************/ 
static int kd_hi253yuv_poweron( char *mode_name)
{
    int ret;
    //ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800, 0/*VOL_2800*/, mode_name);
    ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1800, 0/*VOL_2800*/, mode_name, 0);
 PK_DBG("kd_hi253yuv_poweron start..pinSetIdx=%d\n",pinSetIdx);

    /*ergate-016*/
    mdelay(5);// wait power to be stable  
    disable_inactive_sensor();//disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        //hupeng 1221,the PDN pin of gc2035 is low work
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);

        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;
}


static int kd_hi253yuv_powerdown( char *mode_name)
{
   int ret;

 PK_DBG("kd_hi253yuv_powerdown start..\n");

if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
    }
    //ret = kd_powerdown_sub_devices(mode_name);
	ret = kd_powerdown_main_devices(mode_name);

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        //hupeng 1221,the PDN pin of gc2035 is low work
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}

/****************************sp0a19yuv**********************************/ 
static int kd_sp0a19yuv_poweron( char *mode_name)
{
    int ret;

    printk("[kd_sp0a19yuv_poweron]:----darren----start,pinSetIdx:%d\n",pinSetIdx);
    
    ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name);

     if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr;
    }
    mdelay(5);
    ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr;
    }
    mdelay(5);
     ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name);
        if(ret != TRUE){
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
                goto poweronerr;
        }

    mdelay(5);// wait power to be stable  
    disable_inactive_sensor();//disable inactive sensor

        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ONE)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ZERO)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(10);
        PK_DBG("---darren_power:reset on\n");
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        PK_DBG("---darren_power:reset off\n");
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        PK_DBG("---darren_power:reset on\n");
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }

poweronerr:
    return ret;
}


static int kd_sp0a19yuv_powerdown( char *mode_name)
{
    int ret;

    printk("[kd_sp0a19yuv_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    ret = kd_powerdown_main_devices(mode_name);
    

    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}


/****************************gc0329**********************************/ 
static int kd_gc0312_poweron( char *mode_name)
{
    int ret;
    printk("statr to run kd_gc0329_poweron! %d\n\n",pinSetIdx);
        ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800,0/*VOL_2800*/, mode_name);
    
    /*ergate-008*/
    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
     
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;

}
static int kd_gc0312yuv_powerdown( char *mode_name)
{
    int ret;
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        }
    ret = kd_powerdown_sub_devices(mode_name);

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {

    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }

    return ret;
}
/****************************gc0329**********************************/ 
static int kd_gc0329_poweron( char *mode_name)
{
    int ret;
    printk("statr to run kd_gc0329_poweron! %d\n\n",pinSetIdx);
        ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800,0/*VOL_2800*/, mode_name);
    
    /*ergate-008*/
    mdelay(5);// wait power to be stable  
    disable_inactive_sensor();//disable inactive sensor

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
     
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;

}
static int kd_gc0329yuv_powerdown( char *mode_name)
{
    int ret;
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        }
    ret = kd_powerdown_sub_devices(mode_name);
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {

    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }

    return ret;
}

/****************************gc0328**********************************/ 
static int kd_gc0328_poweron( char *mode_name)
{
    int ret;
    printk("statr to run kd_gc0328_poweron!\n\n");
    ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800,0/*VOL_2800*/, mode_name);
    
    /*ergate-008*/
    mdelay(5);// wait power to be stable  
    disable_inactive_sensor();//disable inactive sensor

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
     
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;

}
static int kd_gc0328yuv_powerdown( char *mode_name)
{
    int ret;
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        }
    ret = kd_powerdown_sub_devices(mode_name);
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {

    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }

    return ret;
}

/****************************hi704yuv**********************************/ 
static int kd_hi704yuv_poweron( char *mode_name)
{
    int ret;
    ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800, 0/*VOL_2800*/, mode_name);
    PK_DBG("kd_hi704yuv_poweron start..pinSetIdx=%d\n",pinSetIdx);

    /*ergate-016*/
    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        //hupeng 1221,the PDN pin of gc2035 is low work
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){     PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){    PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        mdelay(1);

        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;
}


static int kd_hi704yuv_powerdown( char *mode_name)
{
    int ret;

    PK_DBG("kd_hi704yuv_powerdown start..\n");

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)) {
            PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        }
    }
    ret = kd_powerdown_sub_devices(mode_name);


    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}

/****************************ov8825mipi_raw**********************************/ 
static int kd_ov8825mipiraw_poweron( char *mode_name)
{
    int ret;
    ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1500, VOL_2800, mode_name,0);
 PK_DBG("kd_ov8825mipiraw_poweron start..\n");

    /*ergate-016*/
    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
  
    if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);

        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
    
     PK_DBG("kd_ov8825mipiraw_poweron success!\n\n\n");

poweronerr:
    return ret;
}


static int kd_ov8825mipiraw_powerdown( char *mode_name)
{
   int ret;

 PK_DBG("kd_ov8825mipiraw_powerdown start..\n");

if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    ret = kd_powerdown_main_devices(mode_name);
    

    
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}

/****************************ar0833mipi_raw**********************************/ 
static int kd_ar0833mipiraw_poweron( char *mode_name)
{
    int ret;
    ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1200, VOL_2800, mode_name,0);
 PK_DBG("kd_ar0833mipiraw_poweron start..\n");

    /*ergate-016*/
    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
  
    if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);

        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
    
     PK_DBG("kd_ar0833mipiraw_poweron success!\n\n\n");

poweronerr:
    return ret;
}


static int kd_ar0833mipiraw_powerdown( char *mode_name)
{
   int ret;

 PK_DBG("kd_ar0833mipiraw_powerdown start..\n");

if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    ret = kd_powerdown_main_devices(mode_name);
    

    
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}

/****************************s5k3h2yx_mipi_raw**********************************/ 
static int kd_s5k3h2yxmipiraw_poweron( char *mode_name)
{
    int ret;
    ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1200, VOL_2800, mode_name, 1);
 PK_DBG("kd_s5k3h2yxmipiraw start..\n");

    /*ergate-016*/
    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
  
    if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);

        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;
}


static int kd_s5k3h2yxmipiraw_powerdown( char *mode_name)
{
   int ret;

 PK_DBG("kd_s5k3h2yxmipiraw_powerdown start..\n");

    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    ret = kd_powerdown_main_devices(mode_name);
        
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}



static int kd_s5k4e1gamipi_poweron( char *mode_name)
{
        int ret;
        //add by zym for s5k4e1 enable af 
       // mt_set_gpio_out(220,1);
       PK_DBG("kd_s5k4e1gamipi_poweron start to power on just now\n\n\n");
        ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1500, VOL_2800, mode_name,0);

        // wait power to be stable 
        mdelay(5); 
        //enable active sensor
        //RST pin
    
       // disable_inactive_sensor();//disable inactive sensor
        if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
            //PDN pin
            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
            mdelay(5);
            //RST pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(10);
            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(1);
        }
           printk("kd_s5k4e1gamipi_poweron success!\n\n\n");

//_kdCISModulePowerOn_exit_:
    return ret;

}
static int kd_s5k4e1gamipi_powerdown( char *mode_name)
{
    int ret;
    PK_DBG("kdCISModulePower--off s5k4e1gamipi \n");
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
          }
    
    ret = kd_powerdown_main_devices(mode_name);
    
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
    return ret;
}

static int kd_s5k5cagx_yuv_poweron(char *mode_name)
{
    int ret;
    
    PK_DBG("[CAMERA SENSOR] kdCISModulePowerOn get in---S5K5cagX_YUV pinSetIdx=%d\n", pinSetIdx);

    //disable inactive sensor
    /*if(pinSetIdx == 0) {//disable sub
        pinSetIdxTmp = 1;
    }
    else{
        pinSetIdxTmp = 0;
    }
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdxTmp][IDX_PS_CMRST]) {
        if(mt_set_gpio_mode(pinSet[pinSetIdxTmp][IDX_PS_CMRST],pinSet[pinSetIdxTmp][IDX_PS_CMRST+IDX_PS_MODE])){
            PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");
        }
        if(mt_set_gpio_mode(pinSet[pinSetIdxTmp][IDX_PS_CMPDN],pinSet[pinSetIdxTmp][IDX_PS_CMPDN+IDX_PS_MODE])){
            PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");
        }
        if(mt_set_gpio_dir(pinSet[pinSetIdxTmp][IDX_PS_CMRST],GPIO_DIR_OUT)){
            PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");
        }
        if(mt_set_gpio_dir(pinSet[pinSetIdxTmp][IDX_PS_CMPDN],GPIO_DIR_OUT)){
            PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");
        }
        if(mt_set_gpio_out(pinSet[pinSetIdxTmp][IDX_PS_CMRST],pinSet[pinSetIdxTmp][IDX_PS_CMRST+IDX_PS_OFF])){
            PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        } //low == reset sensor
        if(mt_set_gpio_out(pinSet[pinSetIdxTmp][IDX_PS_CMPDN],pinSet[pinSetIdxTmp][IDX_PS_CMPDN+IDX_PS_OFF])){
            PK_DBG("[CAMERA LENS] set gpio failed!! \n");
        } //high == power down lens module
    }*/
    
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){
        PK_DBG("[CAMERA LENS] set gpio failed!! \n");
    }

    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){
        PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
    }
    msleep(1);
    ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1500, VOL_2800, mode_name, 0);
    msleep(5);

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){
            PK_DBG("[CAMERA LENS] set gpio failed!! \n");
        }
        msleep(3);
       
    //RST pin
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON]))
    {PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        }
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]))
        {PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
      }
        msleep(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON]))
            {PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        }
        msleep(1);
    }   
    //while(1){printk("kd_s5k5eayx_yuv power on recycle!\n\n");}        
    return ret;
}

static int kd_s5k5cagx_yuv_powerdown(char *mode_name)
{
    int ret;

        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){
            PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        } //low == reset sensor
       
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){
            PK_DBG("[CAMERA LENS] set gpio failed!! \n");
        } //high == power down lens module
    }
    
    ret = kd_powerdown_main_devices(mode_name);

    return ret;     
}

//////////////////////////////////////////////////
static int kd_t8ev3mipiraw_poweron( char *mode_name)  //fuzk
{
    int ret;

    PK_DBG("[kd_t8ev3_poweron]:----darren----start,pinSetIdx:%d\n",pinSetIdx);
 //   ret = kd_poweron_main_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name,0);

	
 ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name);
	if(ret != TRUE){
			PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
			goto poweronerr;
	}
	mdelay(5);

	 ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name);  //IOVDD

     if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr;
    }
        mdelay(5);
		
    ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr;
    }

        mdelay(5);// wait power to be stable  

		
	ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name);
	if(ret != TRUE){
		PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
			goto poweronerr;
	}
        //disable_inactive_sensor();//disable inactive sensor
	  
	//disable_inactive_sensor();//disable inactive sensor

	    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ZERO)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(5);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_OUT_ONE)){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
         mdelay(80);
	  PK_DBG("---darren_power:reset on\n");
	if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ONE)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	mdelay(2);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);	
    }
/*
	
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
     	mdelay(10);
	if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
     */
poweronerr:
    return ret;
}

static int kd_t8ev3mipiraw_powerdown( char *mode_name)
{
    int ret;
	//PK_DBG("kd_t8ev3_powerdown start..\n");
 printk("kd_t8ev3_powerdown startt..\n");
	if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
	if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    ret = kd_powerdown_main_devices(mode_name);
		
    if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////


static int kd_ov5648_poweron( char *mode_name)
{
    int ret;
    printk("kd_ov5648_poweron start..\n");

    //ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1500, 0/*VOL_2800*/, mode_name);
        ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr;
    }

        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr;
    }

        ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
        goto poweronerr;
    }

        ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
        goto poweronerr;
    }

    mdelay(10);// wait power to be stable  >5ms

        //PDN pin
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
     
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);

poweronerr:
    return ret;
}

static int kd_ov5648_powerdown( char *mode_name)
{
    int ret;
	if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
		if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
		}
	ret = kd_powerdown_main_devices(mode_name);
	
	if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {

	if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
	}

    return ret;
}

int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On, char* mode_name)
{

	printk("%s(), sensor idx = %d, name = %s\n", __func__, SensorIdx, currSensorName);

	if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx){
		if (currSensorName && 
			((0 == strcmp(SENSOR_DRVNAME_OV8825_MIPI_RAW,currSensorName))
			||(0 == strcmp(SENSOR_DRVNAME_S5K4E1GA_MIPI_RAW,currSensorName))
			||(0 == strcmp(SENSOR_DRVNAME_AR0833_MIPI_RAW,currSensorName))
			||(0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW,currSensorName))
			//||(0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName))
			//||(0 == strcmp(SENSOR_DRVNAME_HI257_YUV,currSensorName))
			//||(0 == strcmp(SENSOR_DRVNAME_GC0328_YUV,currSensorName))
			//||(0 == strcmp(SENSOR_DRVNAME_HI704_YUV,currSensorName))
			||(0 == strcmp(SENSOR_DRVNAME_S5K5CAGX_YUV, currSensorName))
			||(0 == strcmp(SENSOR_DRVNAME_SP0A19_YUV, currSensorName))
			//||( 0 == strcmp(SENSOR_DRVNAME_T8EV3_MIPI_RAW, currSensorName))
			|| (0 == strcmp(SENSOR_DRVNAME_OV5648_MIPI_RAW,currSensorName))
			|| (0 == strcmp(SENSOR_DRVNAME_HI544_MIPI_RAW,currSensorName))
			//|| to add other main camera here
			))
		pinSetIdx = 0;
		else
		{
			PK_DBG("kdCISModulePowerOn main get in---  sensorIdx not compare with sensro ++currSensorName=%s\n",currSensorName);
				goto _kdCISModulePowerOn_exit_;
		}
	}
    else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
    if (currSensorName && 
       ( //(0 == strcmp(SENSOR_DRVNAME_HI253_YUV,currSensorName))
		(0 == strcmp(SENSOR_DRVNAME_GC0312_YUV,currSensorName))
	       //|| (0 == strcmp(SENSOR_DRVNAME_GC2035_YUV,currSensorName))
	       //|| (0 == strcmp(SENSOR_DRVNAME_HI257_YUV,currSensorName))
          //|| (0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName))
        //  || (0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW,currSensorName))
        //|| to add other sub camera here
       ))
            pinSetIdx = 1;
    else
    {
        PK_DBG("kdCISModulePowerOn sub get in ---  sensorIdx not compare with sensro ++currSensorName=%s\n",currSensorName);
        goto _kdCISModulePowerOn_exit_;
    }
    }
    else if (DUAL_CAMERA_MAIN_2_SENSOR == SensorIdx) {
       // pinSetIdx = 2;
    }
    //power ON
    PK_DBG("kdCISModulePowerOn:SensorIdx:%d,pinSetIdx:%d",SensorIdx,pinSetIdx);
    Rst_PDN_Init();

    //power ON
    if (On) 
    {
    PK_DBG("kdCISModulePowerOn -on:currSensorName=%s\n",currSensorName);
    if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV8825_MIPI_RAW,currSensorName)))
    {   
             PK_DBG("is ov8825 on \n"); 
         if(TRUE != kd_ov8825mipiraw_poweron(mode_name))
          goto _kdCISModulePowerOn_exit_;
    }
    else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_AR0833_MIPI_RAW,currSensorName)))
    {   
             PK_DBG("is ar0833 on\n");  
         if(TRUE != kd_ar0833mipiraw_poweron(mode_name))
          goto _kdCISModulePowerOn_exit_;
    }
/*  else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K3H2YX_MIPI_RAW,currSensorName)))
    {   
             PK_DBG("is s5k3h2 on\n");  
         if(TRUE != kd_s5k3h2yxmipiraw_poweron(mode_name))
          goto _kdCISModulePowerOn_exit_;
    }*/
        else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K4E1GA_MIPI_RAW,currSensorName)))           
        {
        PK_DBG("is s5k4e1ga on \n");
            if(TRUE != kd_s5k4e1gamipi_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        } 
        else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW,currSensorName)))          
        {
         PK_DBG("is gc2355 on\n");
            if(TRUE != kd_gc2355_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }  
        else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2035_YUV,currSensorName)))          
        {
         PK_DBG("is gc2035 on\n");
            if(TRUE != kd_gc2035_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }  
       /*   else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI_RAW,currSensorName)))
        {
         PK_DBG("is gc2235 on\n");
            if(TRUE != kd_gc2235mipiraw_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }  
        
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI258_YUV,currSensorName)))		  
		{
			PK_DBG("is hi258 on\n");
			if(TRUE != kd_hi258_mipiraw_poweron(mode_name))
			 goto _kdCISModulePowerOn_exit_;
		}  
        
	else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI257_YUV,currSensorName)))         
	{
		PK_DBG("is hi257 on\n");
		if(TRUE != kd_hi257_poweron(mode_name))
			goto _kdCISModulePowerOn_exit_;
	}  */
	else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI544_MIPI_RAW,currSensorName)))         
	{
		PK_DBG("is hi544 on\n");
		if(TRUE != kd_hi544_poweron(mode_name))
			goto _kdCISModulePowerOn_exit_;
	}  
	else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName)))          
	{
		PK_DBG("is gc0329 on\n");
		if(TRUE != kd_gc0329_poweron(mode_name))
			goto _kdCISModulePowerOn_exit_;
	} 
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0312_YUV,currSensorName)))          
        {
         PK_DBG("is gc0312 on\n");
            if(TRUE != kd_gc0312_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
        /*else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0328_YUV,currSensorName)))          
        {
         PK_DBG("is gc0328 on\n");
            if(TRUE != kd_gc0328_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }   */
        else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI704_YUV,currSensorName)))
        {
         PK_DBG("is hi704 on\n");
            if(TRUE != kd_hi704yuv_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
        else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K5CAGX_YUV,currSensorName)))           
        {
         PK_DBG("is s5k5ca on\n");
            if(TRUE != kd_s5k5cagx_yuv_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        } 
        else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI253_YUV,currSensorName)))
        {
         PK_DBG("is hi253 on\n");
            if(TRUE != kd_hi253yuv_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
        else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_SP0A19_YUV,currSensorName)))
        {
         PK_DBG("is SP0A19 on\n");
            if(TRUE != kd_sp0a19yuv_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
                    /* HQ at730 modified for ov5647_mipi*/        
/*
       else if(currSensorName && ((0 == strcmp(SENSOR_DRVNAME_OV5647_RAW,currSensorName))
        ||(0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW,currSensorName)))) //SENSOR_DRVNAME_OV5647MIPI_RAW
        {
              PK_DBG("is ov5647 on\n");
            if(TRUE != kd_ov5647_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        } */
		/*else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_T8EV3_MIPI_RAW,currSensorName))) //fuzk
		{
			PK_DBG("is t8ev3 on\n");
			if(TRUE != kd_t8ev3mipiraw_poweron(mode_name))
				goto _kdCISModulePowerOn_exit_;
		}*/
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5648_MIPI_RAW,currSensorName)))			
		{
			PK_DBG("is ov5648mipi on\n");
			if(TRUE != kd_ov5648_poweron(mode_name))
				goto _kdCISModulePowerOn_exit_;
		}
        else
    {
        PK_DBG("kdCISModulePowerOn get in---  other , please add the power on code!!!!!!\n");
        goto _kdCISModulePowerOn_exit_;
        }
  }
  else {//power OFF

    PK_DBG("kdCISModulePowerOn -off:currSensorName=%s\n",currSensorName);

    if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV8825_MIPI_RAW,currSensorName)))
    {
        PK_DBG("is ov8825 down\n");
         if(TRUE != kd_ov8825mipiraw_powerdown(mode_name))
          goto _kdCISModulePowerOn_exit_;   
        
    }
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_AR0833_MIPI_RAW,currSensorName)))
    {
        PK_DBG("is ar0833 down\n");
         if(TRUE != kd_ar0833mipiraw_powerdown(mode_name))
          goto _kdCISModulePowerOn_exit_;   
    }
/*  else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K3H2YX_MIPI_RAW,currSensorName)))
    {
        PK_DBG("is s5k3h2 down\n");
         if(TRUE != kd_s5k3h2yxmipiraw_powerdown(mode_name))
          goto _kdCISModulePowerOn_exit_;   
        
    }
    */
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K4E1GA_MIPI_RAW,currSensorName)))           
        {
         PK_DBG("is s5k4e1ga down\n");
            if(TRUE != kd_s5k4e1gamipi_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName)))          
        {
         PK_DBG("is gc0329 down\n");
            if(TRUE != kd_gc0329yuv_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        } 
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0312_YUV,currSensorName)))          
        {
         PK_DBG("is gc0312 down\n");
            if(TRUE != kd_gc0312yuv_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
   /* else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0328_YUV,currSensorName)))          
        {
         PK_DBG("is gc0328 down\n");
            if(TRUE != kd_gc0328yuv_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        } 
                  else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI_RAW,currSensorName)))
       {
         PK_DBG("is gc2235 down\n");
            if(TRUE != kd_gc2235mipiraw_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
       }    */
    else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI704_YUV,currSensorName)))
        {
         PK_DBG("is hi704 down\n");
            if(TRUE != kd_hi704yuv_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }   
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2355_MIPI_RAW,currSensorName)))          
        {
         PK_DBG("is gc2035 down\n");
            if(TRUE != kd_gc2355_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }     
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2035_YUV,currSensorName)))          
        {
         PK_DBG("is gc2035 down\n");
            if(TRUE != kd_gc2035_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }     
    else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K5CAGX_YUV, currSensorName)))
    {
        PK_DBG("is s5k5eayx_yuv down\n");
        if(TRUE != kd_s5k5cagx_yuv_powerdown(mode_name))
            goto _kdCISModulePowerOn_exit_;
    }
    else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI253_YUV,currSensorName)))
        {
         PK_DBG("is hi253 down\n");
            if(TRUE != kd_hi253yuv_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
	/*
	else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI257_YUV,currSensorName)))
    {
     PK_DBG("is hi257 down\n");
        if(TRUE != kd_hi257_powerdown(mode_name))
             goto _kdCISModulePowerOn_exit_;
    }
		
    	else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI258_YUV,currSensorName)))
	{
		PK_DBG("is hi258 down\n");
		if(TRUE != kd_hi258mipiraw_powerdown(mode_name))
		 goto _kdCISModulePowerOn_exit_;
	}*/
	else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_HI544_MIPI_RAW,currSensorName)))
	{
		PK_DBG("is hi544 down\n");
		if(TRUE != kd_hi544_powerdown(mode_name))
		 goto _kdCISModulePowerOn_exit_;
	}
        else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_SP0A19_YUV,currSensorName)))
        {
         PK_DBG("is SP0A19 down\n");
            if(TRUE != kd_sp0a19yuv_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }
/*
    else if(currSensorName && ((0 == strcmp(SENSOR_DRVNAME_OV5647_RAW,currSensorName))
        ||(0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW,currSensorName)))) //SENSOR_DRVNAME_OV5647MIPI_RAW
        {
              PK_DBG("is ov5647 down\n");
            if(TRUE != kd_ov5647_powerdown(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }       */
		/*else  if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_T8EV3_MIPI_RAW,currSensorName))) //fuzk
		{
			PK_DBG("is t8ev3 down\n");
			if(TRUE != kd_t8ev3mipiraw_powerdown(mode_name))
				goto _kdCISModulePowerOn_exit_;
		}*/
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5648_MIPI_RAW,currSensorName)))			
		{
			PK_DBG("is ov5648_mipi_raw down\n");
			if(TRUE != kd_ov5648_powerdown(mode_name))
				goto _kdCISModulePowerOn_exit_;
		} 
		else
		{
			PK_DBG("kdCISModulePowerDown get in---  other , please add the power down code!!!!!!\n");
				goto _kdCISModulePowerOn_exit_;
		}
  }
    return 0;

_kdCISModulePowerOn_exit_:
    return -EIO;
}

EXPORT_SYMBOL(kdCISModulePowerOn);


//!--
//




