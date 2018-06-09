#ifndef BUILD_LK
	#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#else
	#include <mach/mt_gpio.h>
	#include <mach/mt_pm_ldo.h>
	#include <mach/upmu_common.h>
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  						(600)
#define FRAME_HEIGHT 						(1024)

#define REGFLAG_DELAY             				0XFE
#define REGFLAG_END_OF_TABLE      				0xFF

#define GPIO_LCD_PWR_EN                                         (GPIO44 | 0x80000000)
#define GPIO_LCD_3V3_EN                                         (GPIO83 | 0x80000000)
#define GPIO_LCD_RST						(GPIO89 | 0x80000000)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)      					(lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)	        			(lcm_util.set_gpio_out((n), (v)))
#define UDELAY(n) 						(lcm_util.udelay(n))
#define MDELAY(n) 						(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)						lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)			lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)						lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   


#define   LCM_DSI_CMD_MODE					0

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
struct LCM_setting_table 
{
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static void TC358768_DCS_write_1A_1P(unsigned char cmd, unsigned char para)
{
	unsigned int data_array[16];
	data_array[0] =(0x00001500 | (para<<24) | (cmd<<16));
	dsi_set_cmdq(data_array, 1, 1);
}

static void TC358768_DCS_write_1A_0P(unsigned char cmd)
{
	unsigned int data_array[16];
	data_array[0]=(0x00000500 | (cmd<<16));
	dsi_set_cmdq(data_array, 1, 1);
}

static void init_lcm_registers(void)
{
	TC358768_DCS_write_1A_1P(0x01,0x00);
	MDELAY(20);
	TC358768_DCS_write_1A_1P(0x83,0x00);
	TC358768_DCS_write_1A_1P(0x84,0x00);
	TC358768_DCS_write_1A_1P(0x85,0x04);
	TC358768_DCS_write_1A_1P(0x86,0x08);
	TC358768_DCS_write_1A_1P(0x8C,0x8E);
	TC358768_DCS_write_1A_1P(0xC5,0x2B);
	TC358768_DCS_write_1A_1P(0xC7,0x2B);
	TC358768_DCS_write_1A_1P(0x83,0xAA);
	TC358768_DCS_write_1A_1P(0x84,0x11);
	TC358768_DCS_write_1A_1P(0xA9,0x4B);
	TC358768_DCS_write_1A_1P(0x83,0x00);
	TC358768_DCS_write_1A_1P(0x84,0x00);
	TC358768_DCS_write_1A_1P(0xFD,0x5B);
	TC358768_DCS_write_1A_1P(0xFA,0x14);

	TC358768_DCS_write_1A_1P(0x83,0xAA);
	TC358768_DCS_write_1A_1P(0x84,0x11);
	TC358768_DCS_write_1A_1P(0xC0,0x1F);
	TC358768_DCS_write_1A_1P(0xC1,0x22);
	TC358768_DCS_write_1A_1P(0xC2,0x2E);
	TC358768_DCS_write_1A_1P(0xC3,0x3B);
	TC358768_DCS_write_1A_1P(0xC4,0x44);
	TC358768_DCS_write_1A_1P(0xC5,0x4D);
	TC358768_DCS_write_1A_1P(0xC6,0x54);
	TC358768_DCS_write_1A_1P(0xC7,0x5B);
	TC358768_DCS_write_1A_1P(0xC8,0x61);
	TC358768_DCS_write_1A_1P(0xC9,0xD0);
	TC358768_DCS_write_1A_1P(0xCA,0xD2);
	TC358768_DCS_write_1A_1P(0xCB,0xEE);
	TC358768_DCS_write_1A_1P(0xCC,0xFB);
	TC358768_DCS_write_1A_1P(0xCD,0x0B);
	TC358768_DCS_write_1A_1P(0xCE,0x0F);
	TC358768_DCS_write_1A_1P(0xCF,0x11);
	TC358768_DCS_write_1A_1P(0xD0,0x13);
	TC358768_DCS_write_1A_1P(0xD1,0x21);
	TC358768_DCS_write_1A_1P(0xD2,0x33);
	TC358768_DCS_write_1A_1P(0xD3,0x4D);
	TC358768_DCS_write_1A_1P(0xD4,0x53);
	TC358768_DCS_write_1A_1P(0xD5,0xB3);
	TC358768_DCS_write_1A_1P(0xD6,0xB9);
	TC358768_DCS_write_1A_1P(0xD7,0xBF);
	TC358768_DCS_write_1A_1P(0xD8,0xC5);
	TC358768_DCS_write_1A_1P(0xD9,0xCE);
	TC358768_DCS_write_1A_1P(0xDA,0xD8);
	TC358768_DCS_write_1A_1P(0xDB,0xE3);
	TC358768_DCS_write_1A_1P(0xDC,0xF1);
	TC358768_DCS_write_1A_1P(0xDD,0xFF);
	TC358768_DCS_write_1A_1P(0xDE,0xF0);
	TC358768_DCS_write_1A_1P(0xDF,0x2F);
	TC358768_DCS_write_1A_1P(0xE0,0x20);
	TC358768_DCS_write_1A_1P(0xE1,0x23);
	TC358768_DCS_write_1A_1P(0xE2,0x30);
	TC358768_DCS_write_1A_1P(0xE3,0x3E);
	TC358768_DCS_write_1A_1P(0xE4,0x46);
	TC358768_DCS_write_1A_1P(0xE5,0x4E);
	TC358768_DCS_write_1A_1P(0xE6,0x57);
	TC358768_DCS_write_1A_1P(0xE7,0x5E);
	TC358768_DCS_write_1A_1P(0xE8,0x64);
	TC358768_DCS_write_1A_1P(0xE9,0xD7);
	TC358768_DCS_write_1A_1P(0xEA,0xDA);
	TC358768_DCS_write_1A_1P(0xEB,0xF8);
	TC358768_DCS_write_1A_1P(0xEC,0x07);
	TC358768_DCS_write_1A_1P(0xED,0x16);
	TC358768_DCS_write_1A_1P(0xEE,0x1B);
	TC358768_DCS_write_1A_1P(0xEF,0x1D);
	TC358768_DCS_write_1A_1P(0xF0,0x23);
	TC358768_DCS_write_1A_1P(0xF1,0x32);
	TC358768_DCS_write_1A_1P(0xF2,0x44);
	TC358768_DCS_write_1A_1P(0xF3,0x61);
	TC358768_DCS_write_1A_1P(0xF4,0x67);
	TC358768_DCS_write_1A_1P(0xF5,0xBB);
	TC358768_DCS_write_1A_1P(0xF6,0xC2);
	TC358768_DCS_write_1A_1P(0xF7,0xCB);
	TC358768_DCS_write_1A_1P(0xF8,0xD1);
	TC358768_DCS_write_1A_1P(0xF9,0xDA);
	TC358768_DCS_write_1A_1P(0xFA,0xE4);
	TC358768_DCS_write_1A_1P(0xFB,0xEE);
	TC358768_DCS_write_1A_1P(0xFC,0xFD);
	TC358768_DCS_write_1A_1P(0xFD,0xFF);
	TC358768_DCS_write_1A_1P(0xFE,0xF8);
	TC358768_DCS_write_1A_1P(0xFF,0x2F);

	TC358768_DCS_write_1A_1P(0x83,0xBB);
	TC358768_DCS_write_1A_1P(0x84,0x22);
	TC358768_DCS_write_1A_1P(0xC0,0x1F);
	TC358768_DCS_write_1A_1P(0xC1,0x22);
	TC358768_DCS_write_1A_1P(0xC2,0x2E);
	TC358768_DCS_write_1A_1P(0xC3,0x3B);
	TC358768_DCS_write_1A_1P(0xC4,0x44);
	TC358768_DCS_write_1A_1P(0xC5,0x4D);
	TC358768_DCS_write_1A_1P(0xC6,0x54);
	TC358768_DCS_write_1A_1P(0xC7,0x5B);
	TC358768_DCS_write_1A_1P(0xC8,0x61);
	TC358768_DCS_write_1A_1P(0xC9,0xD0);
	TC358768_DCS_write_1A_1P(0xCA,0xD2);
	TC358768_DCS_write_1A_1P(0xCB,0xEE);
	TC358768_DCS_write_1A_1P(0xCC,0xFB);
	TC358768_DCS_write_1A_1P(0xCD,0x0B);
	TC358768_DCS_write_1A_1P(0xCE,0x0F);
	TC358768_DCS_write_1A_1P(0xCF,0x11);
	TC358768_DCS_write_1A_1P(0xD0,0x13);
	TC358768_DCS_write_1A_1P(0xD1,0x21);
	TC358768_DCS_write_1A_1P(0xD2,0x33);
	TC358768_DCS_write_1A_1P(0xD3,0x4D);
	TC358768_DCS_write_1A_1P(0xD4,0x53);
	TC358768_DCS_write_1A_1P(0xD5,0xB3);
	TC358768_DCS_write_1A_1P(0xD6,0xB9);
	TC358768_DCS_write_1A_1P(0xD7,0xBF);
	TC358768_DCS_write_1A_1P(0xD8,0xC5);
	TC358768_DCS_write_1A_1P(0xD9,0xCE);
	TC358768_DCS_write_1A_1P(0xDA,0xD8);
	TC358768_DCS_write_1A_1P(0xDB,0xE3);
	TC358768_DCS_write_1A_1P(0xDC,0xF1);
	TC358768_DCS_write_1A_1P(0xDD,0xFF);
	TC358768_DCS_write_1A_1P(0xDE,0xF0);
	TC358768_DCS_write_1A_1P(0xDF,0x2F);
	TC358768_DCS_write_1A_1P(0xE0,0x20);
	TC358768_DCS_write_1A_1P(0xE1,0x23);
	TC358768_DCS_write_1A_1P(0xE2,0x30);
	TC358768_DCS_write_1A_1P(0xE3,0x3E);
	TC358768_DCS_write_1A_1P(0xE4,0x46);
	TC358768_DCS_write_1A_1P(0xE5,0x4E);
	TC358768_DCS_write_1A_1P(0xE6,0x57);
	TC358768_DCS_write_1A_1P(0xE7,0x5E);
	TC358768_DCS_write_1A_1P(0xE8,0x64);
	TC358768_DCS_write_1A_1P(0xE9,0xD7);
	TC358768_DCS_write_1A_1P(0xEA,0xDA);
	TC358768_DCS_write_1A_1P(0xEB,0xF8);
	TC358768_DCS_write_1A_1P(0xEC,0x07);
	TC358768_DCS_write_1A_1P(0xED,0x16);
	TC358768_DCS_write_1A_1P(0xEE,0x1B);
	TC358768_DCS_write_1A_1P(0xEF,0x1D);
	TC358768_DCS_write_1A_1P(0xF0,0x23);
	TC358768_DCS_write_1A_1P(0xF1,0x32);
	TC358768_DCS_write_1A_1P(0xF2,0x44);
	TC358768_DCS_write_1A_1P(0xF3,0x61);
	TC358768_DCS_write_1A_1P(0xF4,0x67);
	TC358768_DCS_write_1A_1P(0xF5,0xBB);
	TC358768_DCS_write_1A_1P(0xF6,0xC2);
	TC358768_DCS_write_1A_1P(0xF7,0xCB);
	TC358768_DCS_write_1A_1P(0xF8,0xD1);
	TC358768_DCS_write_1A_1P(0xF9,0xDA);
	TC358768_DCS_write_1A_1P(0xFA,0xE4);
	TC358768_DCS_write_1A_1P(0xFB,0xEE);
	TC358768_DCS_write_1A_1P(0xFC,0xFD);
	TC358768_DCS_write_1A_1P(0xFD,0xFF);
	TC358768_DCS_write_1A_1P(0xFE,0xF8);
	TC358768_DCS_write_1A_1P(0xFF,0x2F);

	TC358768_DCS_write_1A_1P(0x83,0xCC);
	TC358768_DCS_write_1A_1P(0x84,0x33);
	TC358768_DCS_write_1A_1P(0xC0,0x1F);
	TC358768_DCS_write_1A_1P(0xC1,0x22);
	TC358768_DCS_write_1A_1P(0xC2,0x2E);
	TC358768_DCS_write_1A_1P(0xC3,0x3B);
	TC358768_DCS_write_1A_1P(0xC4,0x44);
	TC358768_DCS_write_1A_1P(0xC5,0x4D);
	TC358768_DCS_write_1A_1P(0xC6,0x54);
	TC358768_DCS_write_1A_1P(0xC7,0x5B);
	TC358768_DCS_write_1A_1P(0xC8,0x61);
	TC358768_DCS_write_1A_1P(0xC9,0xD0);
	TC358768_DCS_write_1A_1P(0xCA,0xD2);
	TC358768_DCS_write_1A_1P(0xCB,0xEE);
	TC358768_DCS_write_1A_1P(0xCC,0xFB);
	TC358768_DCS_write_1A_1P(0xCD,0x0B);
	TC358768_DCS_write_1A_1P(0xCE,0x0F);
	TC358768_DCS_write_1A_1P(0xCF,0x11);
	TC358768_DCS_write_1A_1P(0xD0,0x13);
	TC358768_DCS_write_1A_1P(0xD1,0x21);
	TC358768_DCS_write_1A_1P(0xD2,0x33);
	TC358768_DCS_write_1A_1P(0xD3,0x4D);
	TC358768_DCS_write_1A_1P(0xD4,0x53);
	TC358768_DCS_write_1A_1P(0xD5,0xB3);
	TC358768_DCS_write_1A_1P(0xD6,0xB9);
	TC358768_DCS_write_1A_1P(0xD7,0xBF);
	TC358768_DCS_write_1A_1P(0xD8,0xC5);
	TC358768_DCS_write_1A_1P(0xD9,0xCE);
	TC358768_DCS_write_1A_1P(0xDA,0xD8);
	TC358768_DCS_write_1A_1P(0xDB,0xE3);
	TC358768_DCS_write_1A_1P(0xDC,0xF1);
	TC358768_DCS_write_1A_1P(0xDD,0xFF);
	TC358768_DCS_write_1A_1P(0xDE,0xF0);
	TC358768_DCS_write_1A_1P(0xDF,0x2F);
	TC358768_DCS_write_1A_1P(0xE0,0x20);
	TC358768_DCS_write_1A_1P(0xE1,0x23);
	TC358768_DCS_write_1A_1P(0xE2,0x30);
	TC358768_DCS_write_1A_1P(0xE3,0x3E);
	TC358768_DCS_write_1A_1P(0xE4,0x46);
	TC358768_DCS_write_1A_1P(0xE5,0x4E);
	TC358768_DCS_write_1A_1P(0xE6,0x57);
	TC358768_DCS_write_1A_1P(0xE7,0x5E);
	TC358768_DCS_write_1A_1P(0xE8,0x64);
	TC358768_DCS_write_1A_1P(0xE9,0xD7);
	TC358768_DCS_write_1A_1P(0xEA,0xDA);
	TC358768_DCS_write_1A_1P(0xEB,0xF8);
	TC358768_DCS_write_1A_1P(0xEC,0x07);
	TC358768_DCS_write_1A_1P(0xED,0x16);
	TC358768_DCS_write_1A_1P(0xEE,0x1B);
	TC358768_DCS_write_1A_1P(0xEF,0x1D);
	TC358768_DCS_write_1A_1P(0xF0,0x23);
	TC358768_DCS_write_1A_1P(0xF1,0x32);
	TC358768_DCS_write_1A_1P(0xF2,0x44);
	TC358768_DCS_write_1A_1P(0xF3,0x61);
	TC358768_DCS_write_1A_1P(0xF4,0x67);
	TC358768_DCS_write_1A_1P(0xF5,0xBB);
	TC358768_DCS_write_1A_1P(0xF6,0xC2);
	TC358768_DCS_write_1A_1P(0xF7,0xCB);
	TC358768_DCS_write_1A_1P(0xF8,0xD1);
	TC358768_DCS_write_1A_1P(0xF9,0xDA);
	TC358768_DCS_write_1A_1P(0xFA,0xE4);
	TC358768_DCS_write_1A_1P(0xFB,0xEE);
	TC358768_DCS_write_1A_1P(0xFC,0xFD);
	TC358768_DCS_write_1A_1P(0xFD,0xFF);
	TC358768_DCS_write_1A_1P(0xFE,0xF8);
	TC358768_DCS_write_1A_1P(0xFF,0x2F);

	TC358768_DCS_write_1A_1P(0x83,0x00);
	TC358768_DCS_write_1A_1P(0x84,0x00);
}

static struct LCM_setting_table lcm_suspend_setting[] =
{
	{0x28,  0 , {}},
	{REGFLAG_DELAY, 10, {}},
	{0x10,  0 , {}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
	
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
       	}
    }	
}

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DSI;

    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dsi.mode   = SYNC_EVENT_VDO_MODE;    //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE;//SYNC_EVENT_VDO_MODE;

    // DSI
    /* Command mode setting */
    //1 Three lane or Four lane
    params->dsi.LANE_NUM			= LCM_FOUR_LANE;
    //The following defined the fomat for data coming from LCD engine.
    params->dsi.data_format.color_order 	= LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     	= LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      	= LCM_DSI_FORMAT_RGB888;

    // Highly depends on LCD driver capability.
    // Not support in MT6573
    params->dsi.packet_size=256;

    // Video mode setting		
    params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
    params->dsi.word_count=FRAME_WIDTH*3;	

    params->dsi.vertical_sync_active			= 2;
    params->dsi.vertical_backporch			= 24;
    params->dsi.vertical_frontporch			= 35;
    params->dsi.vertical_active_line			= FRAME_HEIGHT; 

    params->dsi.horizontal_sync_active			= 2;
    params->dsi.horizontal_backporch			= 59;
    params->dsi.horizontal_frontporch			= 80;
    params->dsi.horizontal_active_pixel			= FRAME_WIDTH;

    // Bit rate calculation
    //1 Every lane speed
    params->dsi.PLL_CLOCK = 156;                       //468;//495;//440;
    // continuous clock
    params->dsi.cont_clock = 1;
    params->dsi.ssc_disable = 1;
}

static void lcd_power_en(unsigned char enabled)
{
#ifndef BUILD_LK
	printk("[NT51021][K] %s : %s\n", __func__, enabled ? "on" : "off");
#else
	printf("[NT51021][LK] %s : %s\n", __func__, enabled ? "on" : "off");
#endif

	if (enabled)
	{
		mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	}
	else
	{
		mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);	
	}
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[NT51021][LK] %s\n", __func__);
#else
	printk("[NT51021][K] %s\n", __func__);
#endif
	upmu_set_rg_vgp1_vosel(0x7);
	upmu_set_rg_vgp1_en(0x1);
	MDELAY(20);

	mt_set_gpio_mode(GPIO_LCD_RST, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST , GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(20);

	lcd_power_en(1);
	MDELAY(10);

	init_lcm_registers();
#ifdef BUILD_LK
	printf("[NT51021]lcm_init func:LK Austin nt51021 1st lcm init ok!\n");
#else
	printk("[NT51021]lcm_init func:Kernel Austin nt51021 1st lcm init ok!\n");
#endif
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];
	data_array[0] =0x00100500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
}

static void lcm_suspend_power(void)
{
#ifndef BUILD_LK
	printk("[NT51021][K] %s\n", __func__);
#else
	printf("[NT51021][LK] %s\n", __func__);
#endif
	lcd_power_en(0);
	MDELAY(20);

	mt_set_gpio_mode(GPIO_LCD_RST, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);

	upmu_set_rg_vgp1_en(0x0);
	MDELAY(10);
	printk("[NT51021]lcm_suspend func:Kernel Austin nt51021 1st lcm suspend!\n");
}

static void lcm_resume(void)
{
	init_lcm_registers();
	printk("[NT51021]lcm_init func:Kernel Austin nt51021 1st lcm resume ok!\n");
}

static void lcm_resume_power(void)
{
#ifdef BUILD_LK
	printf("[NT51021][LK] %s\n", __func__);
#else
	printk("[NT51021][K] %s\n", __func__);
#endif
	upmu_set_rg_vgp1_vosel(0x7);
	upmu_set_rg_vgp1_en(0x1);
	MDELAY(20);

	mt_set_gpio_mode(GPIO_LCD_RST, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST , GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(20);

	lcd_power_en(1);
	MDELAY(10);
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}

LCM_DRIVER nt51021_inx_dsi_vdo_kd_lcm_drv = 
{
    	.name			= "nt51021_inx_dsi_vdo_kd",
	.set_util_funcs 	= lcm_set_util_funcs,
	.get_params     	= lcm_get_params,
	.init           	= lcm_init,
	.suspend        	= lcm_suspend,
	.suspend_power  	= lcm_suspend_power,
	.resume         	= lcm_resume,
	.resume_power		= lcm_resume_power,
	.compare_id             = lcm_compare_id,
};

