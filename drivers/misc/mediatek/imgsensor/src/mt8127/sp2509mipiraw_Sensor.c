
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 sp2509mipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
//#include <asm/system.h>
//#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "sp2509mipiraw_Sensor.h"

#ifdef SLT_DEVINFO_CMM_SP2509 	
#include  <linux/dev_info.h> 
static struct devinfo_struct *s_DEVINFO_ccm;   //suppose 10 max lcm device 
#endif

/****************************Modify Following Strings for Debug****************************/
#define PFX "SP2509_camera_sensor"

#define LOG_1 LOG_INF("SP2509,MIPI 1LANE\n")
#define LOG_2 LOG_INF("preview 1600*1200@30fps,864Mbps/lane; video 1600*1200@30fps,864Mbps/lane; capture 2M@30fps,864Mbps/lane\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args) //xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

/*****************************************************************************/
//	T CARD DEBUG
//	debug by sp_zze 2015-06-03
/*****************************************************************************/
//#define DEBUG_SENSOR_SP2509		//for T-card
#ifdef DEBUG_SENSOR_SP2509	//sp_zze
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>

#define SP2509_SD_FILE_PATH					"/mnt/sdcard/SP2509_sd"
#define	SP_I2C_WRITE						write_cmos_sensor
#define	SP_I2C_READ							read_cmos_sensor
#define SP_LOG								LOG_INF
#define SP_DELAY							msleep
#define U32									kal_uint32
#define U16									kal_uint16
#define U8									kal_uint8
#define DATA_BUFF_SIZE						(25*1024)//25k, must be larger than size of sd file

typedef struct
{
	U16 addr;
	U16 val;
} SP2509_init_regs_struct;

static kal_uint16 read_cmos_sensor(kal_uint32 addr);
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para);

static U8 sp_tolower(U8 c)
{
	if (c <= 'Z' && c >= 'A')
		return (c + 'a' - 'A');
	
	return c;
}
#define BASE_MAX	36
static const char digits[] = {"0123456789abcdefghijklmnopqrstuvwxyz"};
static U32 sp_strtoul(const char *str, char **endptr, U8 base)	//sp_zhangzhaoen
{
	const char *sc;
	const char *s1, *s2;
	U32 x = 0;
	
	for (sc = str; ; sc++) {	// skip space
		if (!(*sc == ' ' || *sc == '\t'))
			break;
	}

	if (base == 1 || base > BASE_MAX) {
		if (endptr)
			*endptr = (char *)str;		
		return 0;
	} else if (base) {
		if (base == 16 && *sc == '0' 
			&& (sc[1] == 'x' || sc[1] == 'X'))
			sc += 2;
	} else if (*sc != '0') {
		base = 10;
	} else if (sc[1] == 'x' || sc[1] == 'X') {
		base = 16;
		sc += 2;
	} else {
		base = 8;
	}
	
	for (s1 = sc; *sc == '0'; ++sc)
		;
	
	for (; ; ++sc) {
		s2 = memchr(digits, sp_tolower(*sc), base);
		if (s2 == NULL)
			break;

		x = x * base + (s2 - digits);
	}
	if (s1 == sc) {
		if (endptr)
			*endptr = (char *)str;		
		return 0;
	}

	// DEBUG: when overflow
	
	if (endptr)
		*endptr = (char *)sc;
	
	return (x);
}
static U32 SP2509_Initialize_from_T_Flash(char *file_path)
{
	U8 *curr_ptr = NULL;
	U8 *tmp = NULL;
	U32 file_size = 0;
	U32 regs_total = 0;
	U8 func_ind[4] = {0};	/* REG or DLY */
	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos = 0; 
	static U8 data_buff[DATA_BUFF_SIZE];
	SP2509_init_regs_struct SP2509_reg;
	
	fp = filp_open(file_path, O_RDONLY , 0); 
	if (IS_ERR(fp)) { 
		SP_LOG("create file error\n"); 
		return 0; 
	} 
	fs = get_fs(); 
	set_fs(KERNEL_DS); 
	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos); 
	filp_close(fp, NULL); 
	set_fs(fs);
	
	curr_ptr = data_buff;
	while (curr_ptr < (data_buff + file_size)) {
		while ((*curr_ptr == ' ') || (*curr_ptr == '\t'))/* Skip the Space & TAB */
			curr_ptr++;		
			
		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '*')) {
			curr_ptr += 2;	//skip '/' and '*'	added by sp_zze
			
			while (!(((*curr_ptr) == '*') && ((*(curr_ptr + 1)) == '/'))) {
				curr_ptr += 2;		/* Skip block comment code. */
			}
			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A))) {
				curr_ptr++;
				
				if (curr_ptr >= (data_buff + file_size))
					break;
			}
			curr_ptr += 2;						/* Skip the enter line */
			continue ;
		}
		
		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '/'))	{	/* Comment line, skip it. */
			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A))) {
				curr_ptr++;
				
				if (curr_ptr >= (data_buff + file_size))
					break;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}

		if (((*curr_ptr) == 0x0D) && ((*(curr_ptr + 1)) == 0x0A)) {
			curr_ptr += 2;
			continue ;
		}
		
		memcpy(func_ind, curr_ptr, 3);		
		if (!strcmp((const char *)func_ind, "REG")) {		/* REG */
			curr_ptr += 4;				/* Skip "REG{" */
			SP2509_reg.addr = sp_strtoul((const char *)curr_ptr, &tmp, 16);
			curr_ptr = tmp;
			curr_ptr ++;	/* Skip "," */
			SP2509_reg.val = sp_strtoul((const char *)curr_ptr, &tmp, 16);
			curr_ptr += 2;	/* Skip "};" */
			
			//init
			SP_I2C_WRITE(SP2509_reg.addr, SP2509_reg.val);
			regs_total++;
		} else if (!strcmp((const char *)func_ind, "DLY")) {	/* DLY */
			curr_ptr += 4;				/* Skip "DLY(" */
			SP2509_reg.val = sp_strtoul((const char *)curr_ptr, &tmp, 10);
			curr_ptr = tmp;

			SP_DELAY(SP2509_reg.val);
		}

		while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A))) {
			curr_ptr++;
			
			if (curr_ptr >= (data_buff + file_size))
				break;
		}
		curr_ptr += 2;
	}

	return regs_total;
}
static U8 read_from_extern_flash(char *file_path)
{
	struct file *fp; 

	fp = filp_open(file_path, O_RDONLY , 0); 
	if (IS_ERR(fp)) {   
		SP_LOG("open file error\n");
		return 0;
	} else {
		SP_LOG("open file ok\n");
		filp_close(fp, NULL); 
	}
	
	return 1;
}

#define SP2509_OTHER_DEBUG_PATH					"/mnt/sdcard/SP2509_debug"
static imgsensor_info_struct imgsensor_info;
static void SP2509_other_debug(char *file_path)	//sp_zze
{
	U8 *curr_ptr = NULL;
	U8 *tmp = NULL;
	U32 file_size = 0;
	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos = 0; 
	static U8 data_buff[1024];
	
	fp = filp_open(file_path, O_RDONLY , 0); 
	if (IS_ERR(fp)) { 
		SP_LOG("create file error\n"); 
		return; 
	} 
	fs = get_fs(); 
	set_fs(KERNEL_DS); 
	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos); 
	filp_close(fp, NULL); 
	set_fs(fs);
	
	curr_ptr = data_buff;
	
	imgsensor_info.ae_shut_delay_frame = sp_strtoul((const char *)curr_ptr, &tmp, 10);
	curr_ptr = tmp;
	curr_ptr ++;	/* Skip "," */
	imgsensor_info.ae_sensor_gain_delay_frame = sp_strtoul((const char *)curr_ptr, &tmp, 10);
	curr_ptr = tmp;
	curr_ptr ++;	/* Skip "," */
	imgsensor_info.ae_ispGain_delay_frame = sp_strtoul((const char *)curr_ptr, &tmp, 10);
	//sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
    //sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
    //sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	return;
}
#endif	//END of T CARD DEBUG

extern IMM_auxadc_GetOneChannelValue_Cali(int Channel, int *voltage);

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static imgsensor_info_struct imgsensor_info = {
    .sensor_id = SP2509MIPI_SENSOR_ID,

    //.checksum_value = 0xbaa74750,
	.checksum_value = 0xfaf63d31, // mod by liudongming 0x820e0cf,

    .pre = {
        .pclk = 24000000,            //record different mode's pclk
        .linelength = 947,            //record different mode's linelength
        .framelength = 1234,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1600,//2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1200,//1552,		//record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,//unit , ns
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,
    },
    .cap = {
        .pclk = 24000000,
        .linelength = 947,
        .framelength = 1234,
        .startx = 0,
        .starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .cap1 = {
        .pclk = 24000000,
        .linelength = 947,
        .framelength = 1234,
        .startx = 0,
        .starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .normal_video = {
         .pclk = 24000000,
        .linelength = 947,
        .framelength = 1234,
        .startx = 0,
        .starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .hs_video = {
         .pclk = 24000000,
        .linelength = 947,
        .framelength = 1234,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .slim_video = {
        .pclk = 24000000,
        .linelength = 947,
        .framelength = 1234,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .margin = 4,            //sensor framelength & shutter margin
    .min_shutter = 7,
    .max_frame_length = 0xffff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,    //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 10,      //support sensor mode num

    .cap_delay_frame = 2,       //enter capture delay frame num
    .pre_delay_frame = 2,         //enter preview delay frame num
    .video_delay_frame = 2,        //enter video delay frame num
    .hs_video_delay_frame = 2,
    .slim_video_delay_frame = 2,

    .isp_driving_current = ISP_DRIVING_8MA, //mclk driving current
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_1_LANE,
    .i2c_addr_table = {0x7a, 0x7b, 0xff},
    .i2c_speed = 200, // i2c read/write speed
};


static imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,             //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x46e,                   //current shutter   // Danbo ??
    .gain = 0x40,                      //current gain     // Danbo ??
    .dummy_pixel = 272,                   //current dummypixel
    .dummy_line = 32,                    //current dummyline
    .current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,      //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x7a,//record current sensor's i2c write id
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
 { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, // Preview 2112*1558
 { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, // capture 4206*3128
 { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, // video
 { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}, //hight speed video
 { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200}};// slim video


static kal_uint16 moduleid = 0x00;

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pu_send_cmd[1] = {(char)(addr & 0xFF)};
    //kdSetI2CSpeed(imgsensor_info.i2c_speed);  Add this func to set i2c speed by each sensor

    iReadRegI2C(pu_send_cmd, 1, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{

	char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};
	//kdSetI2CSpeed(imgsensor_info.i2c_speed);  Add this func to set i2c speed by each sensor
	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
	LOG_INF("cc write 0x%x  =%x \n" , addr,read_cmos_sensor(addr) );
}

static kal_uint32 return_sensor_id(void)
{
	write_cmos_sensor(0xfd, 0x00);
	return ((read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03));
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{

    kal_uint32 frame_length = imgsensor.frame_length;
    //unsigned long flags;
    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
	  imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length; 
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
    {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
}    /*    set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{  
        write_cmos_sensor(0xfd, 0x01); 
        write_cmos_sensor(0x03, (shutter >> 8) & 0xFF);
        write_cmos_sensor(0x04, shutter  & 0xFF); 
        write_cmos_sensor(0x01, 0x01); 
        LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

}   /*  write_shutter  */



/*************************************************************************
* FUNCTION
*   set_shutter
*
* DESCRIPTION
*   This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    if (shutter < 7)
          shutter = 7; 
	else if(shutter > 0xffff)
	   shutter = 0xffff;
	
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    
	write_shutter(shutter);

}    /*    set_shutter */




/*************************************************************************
* FUNCTION
*   set_gain
*
* DESCRIPTION
*   This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*   the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{

	 #if 1
	  kal_uint8  iReg;

	    if(gain >= BASEGAIN && gain <= 15*BASEGAIN)
   	 {   
   	    	 iReg = 0x10 * gain/BASEGAIN;        //change mtk gain base to aptina gain base
             //iReg+= ((gain%BASEGAIN) * BASEGAIN)/0x10;
						   	
   	    	 if(iReg<=0x10)
   	    	 {
   	    	    	 write_cmos_sensor(0xfd, 0x01);
   	    	    	 write_cmos_sensor(0x24, 0x10);//0x23
   	    	    	 write_cmos_sensor(0x01, 0x01);
   	    	 }
   	    	 else if(iReg>= 0xa0)//gpw
   	    	 {
   	    	    	 write_cmos_sensor(0xfd, 0x01);
   	    	    	 write_cmos_sensor(0x24,0xa0);
   	    	    	 write_cmos_sensor(0x01, 0x01); 
	        }        	
   	    	 else
   	    	 {
   	    	    	write_cmos_sensor(0xfd, 0x01);
   	    	    	write_cmos_sensor(0x24, (kal_uint8)iReg);
   	    	    	write_cmos_sensor(0x01, 0x01);	 
	       }	
   	 }	
	 return gain;
        
        #endif
        
}   /*  set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

}



static void set_mirror_flip(kal_uint8 image_mirror)
{

    /********************************************************
       *
       *   0x0101 Sensor mirror flip 
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
	//int static cnt = 0 ;
    write_cmos_sensor(0xfd, 0x01);
    //iTemp = read_cmos_sensor(0x3f) & 0x03;
    //iTemp&= ~0x03; //Clear the mirror and flip bits.

	//cnt = (cnt +1 ) % 4 ;
	//printk("%s :%d\n",__func__ ,cnt);
    switch (image_mirror) {
        case IMAGE_NORMAL:
			      write_cmos_sensor(0x3f, 0x00);    //Set normal
            break;
        case IMAGE_H_MIRROR:
            //write_cmos_sensor(0x3f, iTemp | 0x01); //Set mirror
            write_cmos_sensor(0x3f, 0x01);
            break;
        case IMAGE_V_MIRROR:
            //write_cmos_sensor(0x3f, iTemp | 0x02); //Set flip
            write_cmos_sensor(0x3f, 0x02);
            break;
        case IMAGE_HV_MIRROR:
            write_cmos_sensor(0x3f, 0x03); //Set mirror and flip
            break;
        default:
        	break ;
    }

}


/*************************************************************************
* FUNCTION
*   night_mode
*
* DESCRIPTION
*   This function night mode of sensor.
*
* PARAMETERS
*   bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}   /*  night_mode  */

static void sensor_init(void)
{
	LOG_INF("sensor_init start \n");
	//mdelay(1000);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x2f,0x04);
	write_cmos_sensor(0x34,0x00);
	write_cmos_sensor(0x35,0x00);
	write_cmos_sensor(0x30,0x1d);
	write_cmos_sensor(0x33,0x05);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0x44,0x00);
	write_cmos_sensor(0x2a,0x4c);
	write_cmos_sensor(0x2b,0x1e);
	write_cmos_sensor(0x2c,0x60);
	write_cmos_sensor(0x25,0x11);
	write_cmos_sensor(0x03,0x02);
	write_cmos_sensor(0x04,0xf7);
	write_cmos_sensor(0x09,0x00);
	write_cmos_sensor(0x0a,0x02);
	write_cmos_sensor(0x06,0x0a);
	write_cmos_sensor(0x24,0x60);
	write_cmos_sensor(0x01,0x01);
	write_cmos_sensor(0xfb,0x73);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0x16,0x04);
	write_cmos_sensor(0x1c,0x09);
	write_cmos_sensor(0x21,0x46);
	write_cmos_sensor(0x6c,0x00);
	write_cmos_sensor(0x6b,0x00);
	write_cmos_sensor(0x84,0x00);
	write_cmos_sensor(0x85,0x10);
	write_cmos_sensor(0x86,0x10);
	write_cmos_sensor(0x12,0x04);
	write_cmos_sensor(0x13,0x40);
	write_cmos_sensor(0x11,0x20);
	write_cmos_sensor(0x33,0x40);
	write_cmos_sensor(0xd0,0x03);
	write_cmos_sensor(0xd1,0x01);
	write_cmos_sensor(0xd2,0x00);
	write_cmos_sensor(0xd3,0x01);
	write_cmos_sensor(0xd4,0x20);
	write_cmos_sensor(0x51,0x14);
	write_cmos_sensor(0x52,0x10);
	write_cmos_sensor(0x55,0x30);
	write_cmos_sensor(0x58,0x10);
	write_cmos_sensor(0x71,0x10);
	write_cmos_sensor(0x6f,0x40);
	write_cmos_sensor(0x75,0x60);
	write_cmos_sensor(0x76,0x10);
	write_cmos_sensor(0x8a,0x22);
	write_cmos_sensor(0x8b,0x22);
	write_cmos_sensor(0x19,0x71);
	write_cmos_sensor(0x29,0x01);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0x9d,0xea);
	write_cmos_sensor(0xa0,0x05);
	write_cmos_sensor(0xa1,0x02);
	write_cmos_sensor(0xad,0x62);
	write_cmos_sensor(0xae,0x00);
	write_cmos_sensor(0xaf,0x85);
	write_cmos_sensor(0xb1,0x01);
	write_cmos_sensor(0xac,0x01);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0xfc,0x10);
	write_cmos_sensor(0xfe,0x10);
	write_cmos_sensor(0xf9,0x00);
	write_cmos_sensor(0xfa,0x00);
	write_cmos_sensor(0x8e,0x06);
	write_cmos_sensor(0x8f,0x40);
	write_cmos_sensor(0x90,0x04);
	write_cmos_sensor(0x91,0xb0);
	write_cmos_sensor(0x3f,0x00);
	write_cmos_sensor(0xac,0x01);
	write_cmos_sensor(0x45,0x01);
	write_cmos_sensor(0x46,0x00);
	write_cmos_sensor(0x47,0x6c);
	write_cmos_sensor(0x48,0x03);
	write_cmos_sensor(0x49,0x8b);
	write_cmos_sensor(0x4a,0x00);
	write_cmos_sensor(0x4b,0x07);
	write_cmos_sensor(0x4c,0x04);
	write_cmos_sensor(0x4d,0xb7);
	LOG_INF("sensor_init end \n" );
}    /*    MIPI_sensor_Init  */


static void preview_setting(void)   //PreviewSetting
{

	LOG_INF("preview_setting start\n" );

	LOG_INF("preview_setting end \n" );

}   /*  preview_setting  */

static void capture_setting(void)  
{

}

static void normal_video_setting(void )    // VideoFullSizeSetting
{

}
static void hs_video_setting(void)  // VideoHDSetting_60fps
{
	
}

static void slim_video_setting(void)  // VideoHDSetting
{
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{

    if(enable)
    {
        write_cmos_sensor(0xfd,0x01);
        write_cmos_sensor(0x0d,0x01);
    }
	else
	{
        write_cmos_sensor(0xfd,0x01);
        write_cmos_sensor(0x0d,0x00);
    }    
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

UINT32 SP2509GetModuleID()
{
	printk("SP2509GetModuleID\n");
	return moduleid;
}


/*************************************************************************
* FUNCTION
*   get_imgsensor_id
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
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id) 
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	int adc_val = 0;
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
	spin_lock(&imgsensor_drv_lock);
	imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
	spin_unlock(&imgsensor_drv_lock);
	do {
		*sensor_id = return_sensor_id();
		if (*sensor_id == imgsensor_info.sensor_id) {
		printk("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
		IMM_auxadc_GetOneChannelValue_Cali(12, &adc_val);
		if (adc_val < 100000) {	 /*0.1v = 100000uv , 0.3v :huaquan ; 0v : blx*/
			moduleid = 0x01;//blx
		}
		else {	 /*0.1v = 100000uv , 0.3v :huaquan ; 0v : blx*/
			moduleid = 0x05;//Huaquan
		}
		return ERROR_NONE;
		}
		printk("Read sensor id fail, write id:0x%x id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
		retry--;
	} while(retry > 0);
	i++;
	retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   open
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    //const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
    
	printk("open start \n" );
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0; 
   
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            //sensor_id = 0x2508;
            if (sensor_id == imgsensor_info.sensor_id) {                 
                break;
            }   
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }        
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;
    
    /* initail sequence write in  */
#ifdef DEBUG_SENSOR_SP2509
	if(read_from_extern_flash(SP2509_SD_FILE_PATH)) {
		SP_LOG("________________from t!\n");
		SP2509_Initialize_from_T_Flash(SP2509_SD_FILE_PATH);
	} else {
		preview_setting();
		sensor_init();	
	}
#else  
	/* initail sequence write in  */
	//preview_setting();

	sensor_init();
#endif	
    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
	  imgsensor.ihdr_en = KAL_FALSE;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);
	
	printk("open end \n" );

    return ERROR_NONE;
}   /*  open  */



/*************************************************************************
* FUNCTION
*   close
*
* DESCRIPTION
*   
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{

    /*No Need to implement this function*/ 
    
    return ERROR_NONE;
}   /*  close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength; 
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
#ifndef DEBUG_SENSOR_SP2509
    preview_setting();
#endif
	
	set_mirror_flip(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}   /*  preview   */

/*************************************************************************
* FUNCTION
*   capture
*
* DESCRIPTION
*   This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
#ifndef DEBUG_SENSOR_SP2509
    capture_setting();
#endif

    return ERROR_NONE;
}   /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;  
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    //imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
#ifndef DEBUG_SENSOR_SP2509
    normal_video_setting();
#endif

    return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength; 
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
#ifndef DEBUG_SENSOR_SP2509
    hs_video_setting();
#endif

    return ERROR_NONE;
}   /*  hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_TRUE;
    spin_unlock(&imgsensor_drv_lock);
#ifndef DEBUG_SENSOR_SP2509
    slim_video_setting();
#endif

    return ERROR_NONE;
}   /*  slim_video   */


static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
    
    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;       

    
    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight    = imgsensor_info.hs_video.grabwindow_height;
    
    sensor_resolution->SensorSlimVideoWidth  = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;

    return ERROR_NONE;
}   /*  get_resolution  */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{


#ifdef DEBUG_SENSOR_SP2509
	if(read_from_extern_flash(SP2509_OTHER_DEBUG_PATH)) {
		printk("sp_zze: other debug!\n");
		SP2509_other_debug(SP2509_OTHER_DEBUG_PATH);
	}
#endif

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;  
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame; 
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame; 
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
    
    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;   
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    
    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num; 
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */
    
    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;   // 0 is default 1x 
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;      
            
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            sensor_info->SensorGrabStartX = imgsensor_info.cap.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
                  
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc; 

            break;   
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            
            sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
       
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc; 

            break;    
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:         
            sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
                  
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc; 

            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
                  
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc; 

            break;
        default:            
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;      
            
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }
    
    return ERROR_NONE;
}   /*  get_info  */


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            capture(image_window, sensor_config_data);
            break;  
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            normal_video(image_window, sensor_config_data);  // VideoFullSizeSetting
            break;    
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);  // VideoHDSetting_120fps
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data); // VideoHDSetting
            break;
        default:
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}   /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker     
        imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate) 
{
    kal_uint32 frame_length;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);  
            //set_dummy();         
            break;          
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0; 
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);    
            //set_dummy();       
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            }
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock); 
            //set_dummy();          
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock); 
            //set_dummy();      
            break; 
        default:  //coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
			      imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			      imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			      imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);  
           
            break;
    } 
    return ERROR_NONE;
	
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
{

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *framerate = imgsensor_info.pre.max_framerate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *framerate = imgsensor_info.normal_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *framerate = imgsensor_info.cap.max_framerate;
            break;      
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *framerate = imgsensor_info.hs_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO: 
            *framerate = imgsensor_info.slim_video.max_framerate;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
    
    SENSOR_WINSIZE_INFO_STRUCT *wininfo;    
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
 
    switch (feature_id) {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	 
			*feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;         
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:       
            set_gain((UINT16) *feature_data);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;
		case SENSOR_FEATURE_GET_SENSOR_CURRENT_MID://cuirui add for MID
		*feature_return_para_32=SP2509GetModuleID();
		*feature_para_len=4;
		break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break; 
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32); 
            break; 
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
            break;
	#if 0
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
	#else // 32 bit  PZT 20160531 for CTS test
	 case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data_32, *(feature_data_32+1));
	#endif
	
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            //get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data_32, (MUINT32 *)(*(feature_data_32+1)));
            get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing             
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;                             
            break;              
        case SENSOR_FEATURE_SET_FRAMERATE:
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
            spin_unlock(&imgsensor_drv_lock);       
            break;
		   case SENSOR_FEATURE_SET_HDR:
			      spin_lock(&imgsensor_drv_lock);
			      imgsensor.ihdr_en = (BOOL)*feature_data;
			      spin_unlock(&imgsensor_drv_lock);		
			      break;
        case SENSOR_FEATURE_GET_CROP_INFO:
            wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
            switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;    
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
			}
		    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			      ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));	
			      break;
		   default:
			break;
	}
  
    return ERROR_NONE;
}   /*  feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 SP2509_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;
    return ERROR_NONE;
}   /*  SP2509_MIPI_RAW_SensorInit  */
