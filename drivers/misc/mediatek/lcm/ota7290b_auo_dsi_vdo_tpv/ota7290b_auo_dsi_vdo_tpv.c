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

#define FRAME_WIDTH					(600)
#define FRAME_HEIGHT					(1024)

#define REGFLAG_DELAY					0XFE
#define REGFLAG_END_OF_TABLE				0xFF

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
#define read_reg_v2(cmd, buffer, buffer_size)			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

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

static struct LCM_setting_table lcm_initialization_setting[] =
{
	{0xB0,  1 ,  {0x5A}},
	{0xB1,  1 ,  {0x00}},
	{0x89,  1 ,  {0x01}},
	{0x91,  1 ,  {0x17}},

	{0xB1,  1 ,  {0x03}},
	{0x2C,  1 ,  {0x28}},
	{REGFLAG_DELAY, 10,  {}},

	{0x00,  1 ,  {0xAB}},
	{0x01,  1 ,  {0x15}},
	{0x02,  1 ,  {0x00}},
	{0x03,  1 ,  {0x00}},
	{0x04,  1 ,  {0x00}},
	{0x05,  1 ,  {0x00}},
	{0x06,  1 ,  {0x00}},
	{0x07,  1 ,  {0x00}},
	{0x08,  1 ,  {0x00}},
	{0x09,  1 ,  {0xBF}},
	{0x0A,  1 ,  {0x1F}},
	{0x0B,  1 ,  {0x77}},
	{0x0C,  1 ,  {0x00}},
	{0x0D,  1 ,  {0x00}},
	{0x0E,  1 ,  {0xC4}},
	{0x0F,  1 ,  {0x0D}},
	{0x10,  1 ,  {0x05}},
	{0x11,  1 ,  {0x00}},
	{0x12,  1 ,  {0x70}},
	{0x13,  1 ,  {0x01}},
	{0x14,  1 ,  {0xE3}},
	{0x15,  1 ,  {0xFF}},
	{0x16,  1 ,  {0x3D}},
	{0x17,  1 ,  {0x0E}},
	{0x18,  1 ,  {0x01}},
	{0x19,  1 ,  {0x00}},
	{0x1A,  1 ,  {0x00}},
	{0x1B,  1 ,  {0xFC}},
	{0x1C,  1 ,  {0x0B}},
	{0x1D,  1 ,  {0xA0}},
	{0x1E,  1 ,  {0x03}},
	{0x1F,  1 ,  {0x04}},
	{0x20,  1 ,  {0x0C}},
	{0x21,  1 ,  {0x00}},
	{0x22,  1 ,  {0x04}},
	{0x23,  1 ,  {0x81}},
	{0x24,  1 ,  {0x1F}},
	{0x25,  1 ,  {0x10}},
	{0x26,  1 ,  {0x9B}},
	{0x2D,  1 ,  {0x01}},
	{0x2E,  1 ,  {0x84}},
	{0x2F,  1 ,  {0x00}},
	{0x30,  1 ,  {0x02}},
	{0x31,  1 ,  {0x08}},
	{0x32,  1 ,  {0x01}},
	{0x33,  1 ,  {0x1C}},
	{0x34,  1 ,  {0x70}},
	{0x35,  1 ,  {0xFF}},
	{0x36,  1 ,  {0xFF}},
	{0x37,  1 ,  {0xFF}},
	{0x38,  1 ,  {0xFF}},
	{0x39,  1 ,  {0xFF}},
	{0x3A,  1 ,  {0x05}},
	{0x3B,  1 ,  {0x00}},
	{0x3C,  1 ,  {0x00}},
	{0x3D,  1 ,  {0x00}},
	{0x3E,  1 ,  {0x0F}},
	{0x3F,  1 ,  {0x8C}},
	{0x40,  1 ,  {0x28}},
	{0x41,  1 ,  {0xFC}},
	{0x42,  1 ,  {0x01}},
	{0x43,  1 ,  {0x40}},
	{0x44,  1 ,  {0x05}},
	{0x45,  1 ,  {0xE8}},
	{0x46,  1 ,  {0x16}},
	{0x47,  1 ,  {0x00}},
	{0x48,  1 ,  {0x00}},
	{0x49,  1 ,  {0x88}},
	{0x4A,  1 ,  {0x08}},
	{0x4B,  1 ,  {0x05}},
	{0x4C,  1 ,  {0x03}},
	{0x4D,  1 ,  {0xD0}},
	{0x4E,  1 ,  {0x13}},
	{0x4F,  1 ,  {0xFF}},
	{0x50,  1 ,  {0x0A}},
	{0x51,  1 ,  {0x53}},
	{0x52,  1 ,  {0x26}},
	{0x53,  1 ,  {0x22}},
	{0x54,  1 ,  {0x09}},
	{0x55,  1 ,  {0x22}},
	{0x56,  1 ,  {0x00}},
	{0x57,  1 ,  {0x1C}},
	{0x58,  1 ,  {0x03}},
	{0x59,  1 ,  {0x3F}},
	{0x5A,  1 ,  {0x28}},
	{0x5B,  1 ,  {0x01}},
	{0x5C,  1 ,  {0xCC}},
	{0x5D,  1 ,  {0x21}},
	{0x5E,  1 ,  {0x04}},
	{0x5F,  1 ,  {0xA1}},
	{0x60,  1 ,  {0x14}},
	{0x61,  1 ,  {0x63}},
	{0x62,  1 ,  {0xCE}},
	{0x63,  1 ,  {0x41}},
	{0x64,  1 ,  {0xC8}},
	{0x65,  1 ,  {0x75}},
	{0x66,  1 ,  {0x08}},
	{0x67,  1 ,  {0x21}},
	{0x68,  1 ,  {0x88}},
	{0x69,  1 ,  {0x10}},
	{0x6A,  1 ,  {0x42}},
	{0x6B,  1 ,  {0x20}},
	{0x6C,  1 ,  {0x6B}},
	{0x6D,  1 ,  {0xB5}},
	{0x6E,  1 ,  {0xF6}},
	{0x6F,  1 ,  {0x5E}},
	{0x70,  1 ,  {0x8C}},
	{0x71,  1 ,  {0x5C}},
	{0x72,  1 ,  {0x87}},
	{0x73,  1 ,  {0x10}},
	{0x74,  1 ,  {0x02}},
	{0x75,  1 ,  {0x09}},
	{0x76,  1 ,  {0x00}},
	{0x77,  1 ,  {0x00}},
	{0x78,  1 ,  {0x0F}},
	{0x79,  1 ,  {0xE0}},
	{0x7A,  1 ,  {0x01}},
	{0x7B,  1 ,  {0xFF}},
	{0x7C,  1 ,  {0xFF}},
	{0x7D,  1 ,  {0x09}},
	{0x7E,  1 ,  {0x42}},
	{0x7F,  1 ,  {0xFE}},
	{REGFLAG_DELAY, 10,  {}},

	{0xB1,  1 ,  {0x02}},
	{REGFLAG_DELAY, 10,  {}},

	{0x00,  1 ,  {0xFF}},
	{0x01,  1 ,  {0x01}},
	{0x02,  1 ,  {0x00}},
	{0x03,  1 ,  {0x00}},
	{0x04,  1 ,  {0x00}},
	{0x05,  1 ,  {0x00}},
	{0x06,  1 ,  {0x00}},
	{0x07,  1 ,  {0x00}},
	{0x08,  1 ,  {0xC0}},
	{0x09,  1 ,  {0x00}},
	{0x0A,  1 ,  {0x00}},
	{0x0B,  1 ,  {0x14}},
	{0x0C,  1 ,  {0xE6}},
	{0x0D,  1 ,  {0x0D}},
	{0x0F,  1 ,  {0x00}},
	{0x10,  1 ,  {0x72}},
	{0x11,  1 ,  {0xC8}},
	{0x12,  1 ,  {0x1B}},
	{0x13,  1 ,  {0x44}},
	{0x14,  1 ,  {0x5A}},
	{0x15,  1 ,  {0x99}},
	{0x16,  1 ,  {0xB7}},
	{0x17,  1 ,  {0xA1}},
	{0x18,  1 ,  {0x8B}},
	{0x19,  1 ,  {0xBB}},
	{0x1A,  1 ,  {0xAB}},
	{0x1B,  1 ,  {0x0F}},
	{0x1C,  1 ,  {0xFF}},
	{0x1D,  1 ,  {0xFF}},
	{0x1E,  1 ,  {0xFF}},
	{0x1F,  1 ,  {0xFF}},
	{0x20,  1 ,  {0xFF}},
	{0x21,  1 ,  {0xFF}},
	{0x22,  1 ,  {0xFF}},
	{0x23,  1 ,  {0xFF}},
	{0x24,  1 ,  {0xFF}},
	{0x25,  1 ,  {0xFF}},
	{0x26,  1 ,  {0xFF}},
	{0x27,  1 ,  {0x1F}},
	{0x2C,  1 ,  {0xFF}},
	{0x2D,  1 ,  {0x07}},
	{0x33,  1 ,  {0x05}},
	{0x35,  1 ,  {0x7F}},
	{0x36,  1 ,  {0x04}},
	{0x38,  1 ,  {0x7F}},
	{0x3A,  1 ,  {0x80}},
	{0x3B,  1 ,  {0x01}},
	{0x3C,  1 ,  {0xC0}},
	{0x3D,  1 ,  {0x32}},
	{0x3E,  1 ,  {0x00}},
	{0x3F,  1 ,  {0x58}},
	{0x40,  1 ,  {0x06}},
	{0x41,  1 ,  {0x00}},
	{0x42,  1 ,  {0xCB}},
	{0x43,  1 ,  {0x00}},
	{0x44,  1 ,  {0x60}},
	{0x45,  1 ,  {0x09}},
	{0x46,  1 ,  {0x00}},
	{0x47,  1 ,  {0x00}},
	{0x48,  1 ,  {0xBB}},
	{0x49,  1 ,  {0xD2}},
	{0x4A,  1 ,  {0x01}},
	{0x4B,  1 ,  {0x00}},
	{0x4C,  1 ,  {0x10}},
	{0x4D,  1 ,  {0xC0}},
	{0x4E,  1 ,  {0x0F}},
	{0x4F,  1 ,  {0xF1}},
	{0x50,  1 ,  {0x78}},
	{0x51,  1 ,  {0x3A}},
	{0x52,  1 ,  {0x34}},
	{0x53,  1 ,  {0x59}},
	{0x54,  1 ,  {0x00}},
	{0x55,  1 ,  {0x03}},
	{0x56,  1 ,  {0x0C}},
	{0x57,  1 ,  {0x00}},
	{0x58,  1 ,  {0x00}},
	{0x59,  1 ,  {0xF0}},
	{0x5A,  1 ,  {0x7C}},
	{0x5B,  1 ,  {0x3E}},
	{0x5C,  1 ,  {0x9F}},
	{0x5D,  1 ,  {0x00}},
	{0x5E,  1 ,  {0x00}},
	{0x5F,  1 ,  {0x00}},
	{0x60,  1 ,  {0x60}},
	{0x61,  1 ,  {0x65}},
	{0x62,  1 ,  {0xB5}},
	{0x63,  1 ,  {0xB2}},
	{0x64,  1 ,  {0x02}},
	{0x65,  1 ,  {0x00}},
	{0x66,  1 ,  {0x00}},
	{0x67,  1 ,  {0x00}},
	{0x68,  1 ,  {0x98}},
	{0x69,  1 ,  {0x80}},
	{0x6A,  1 ,  {0x40}},
	{0x6B,  1 ,  {0x3C}},
	{0x6C,  1 ,  {0x3D}},
	{0x6D,  1 ,  {0x3D}},
	{0x6E,  1 ,  {0x3D}},
	{0x6F,  1 ,  {0x3D}},
	{0x70,  1 ,  {0x9F}},
	{0x71,  1 ,  {0xCF}},
	{0x72,  1 ,  {0x27}},
	{0x73,  1 ,  {0x00}},
	{0x74,  1 ,  {0x00}},
	{0x75,  1 ,  {0x00}},
	{0x76,  1 ,  {0x00}},
	{0x77,  1 ,  {0x00}},
	{0x78,  1 ,  {0x00}},
	{0x79,  1 ,  {0x00}},
	{0x7A,  1 ,  {0xAC}},
	{0x7B,  1 ,  {0xAC}},
	{0x7C,  1 ,  {0xAC}},
	{0x7D,  1 ,  {0xAC}},
	{0x7E,  1 ,  {0xAC}},
	{0x7F,  1 ,  {0x56}},
	{0x0B,  1 ,  {0x04}},
	{REGFLAG_DELAY, 10,  {}},

	{0xB1,  1 ,  {0x03}},
	{0x2C,  1 ,  {0x2C}},
	{REGFLAG_DELAY, 10,  {}},

	{0xB1,  1 ,  {0x00}},
	{0x89,  1 ,  {0x03}},
	{REGFLAG_END_OF_TABLE, 0x00,  {}}
};

static struct LCM_setting_table lcm_suspend_setting[] =
{
	{0x28,  0 , { }},
	{REGFLAG_DELAY, 20, { }},
	{0x10, 0 , { }},
	{REGFLAG_DELAY, 120, { }},
	{REGFLAG_END_OF_TABLE, 0x00, { }}
};

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode   = BURST_VDO_MODE;    //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE;//SYNC_EVENT_VDO_MODE;

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
	params->dsi.vertical_backporch				= 3;
	params->dsi.vertical_frontporch				= 2;
	params->dsi.vertical_active_line			= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active			= 24;
	params->dsi.horizontal_backporch			= 36;
	params->dsi.horizontal_frontporch			= 20;
	params->dsi.horizontal_active_pixel			= FRAME_WIDTH;

	// Bit rate calculation
	//1 Every lane speed
	params->dsi.PLL_CLOCK = 134;
	params->dsi.ssc_disable = 1;		//1:disable ssc , 0:enable ssc
	//params->dsi.cont_clock = 1; 	        // continuous clock
}

static void lcd_power_en(unsigned char enabled)
{
#ifndef BUILD_LK
	printk("[OTA7290B][K] %s : %s\n", __func__, enabled ? "on" : "off");
#else
	printf("[OTA7290B][LK] %s : %s\n", __func__, enabled ? "on" : "off");
#endif
	if (enabled) {
		mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	} else {
		mt_set_gpio_mode(GPIO_LCD_PWR_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_LCD_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
	}
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[OTA7290B][LK] %s\n", __func__);
#else
	printk("[OTA7290B][K] %s\n", __func__);
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

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

#ifdef BUILD_LK
	printf("[OTA7290B]lcm_init func:LK Austin ota7290b TPV lcm init ok!\n");
#else
	printk("[OTA7290B]lcm_init func:Kernel Austin ota7290b TPV lcm init ok!\n");
#endif
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend_power(void)
{
#ifndef BUILD_LK
	printk("[OTA7290B][K] %s\n", __func__);
#else
	printf("[OTA7290B][LK] %s\n", __func__);
#endif
	lcd_power_en(0);
	MDELAY(20);

	mt_set_gpio_mode(GPIO_LCD_RST, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);

	upmu_set_rg_vgp1_en(0x0);
	MDELAY(10);
	printk("[OTA7290B]lcm_suspend func:Kernel Austin ota7290b TPV lcm suspend ok!\n");
}

static void lcm_resume(void)
{
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	printk("[OTA7290B]lcm_init func:Kernel Austin ota7290b TPV lcm resume ok!\n");
}

static void lcm_resume_power(void)
{
#ifdef BUILD_LK
	printf("[OTA7290B][LK] %s\n", __func__);
#else
	printk("[OTA7290B][K] %s\n", __func__);
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

LCM_DRIVER ota7290b_auo_dsi_vdo_tpv_lcm_drv =
{
	.name			= "ota7290b_auo_dsi_vdo_tpv",
	.set_util_funcs 	= lcm_set_util_funcs,
	.get_params     	= lcm_get_params,
	.init           	= lcm_init,
	.suspend        	= lcm_suspend,
	.suspend_power  	= lcm_suspend_power,
	.resume         	= lcm_resume,
	.resume_power		= lcm_resume_power,
	.compare_id             = lcm_compare_id,
};

