include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_init.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o

ifeq ($(CONFIG_AUSTIN_PROJECT), y)
	obj-y   +=  goodix_tool.o
	obj-y   +=  gt9xx_driver.o
	obj-y   +=  gt9xx_update.o
else
	obj-y   +=  focaltech_core.o
	obj-y   +=  focaltech_ex_fun.o
	obj-y   +=  focaltech_ctl.o
	obj-y   +=  focaltech_flash.o
	obj-y   +=  gsl_ts_driver.o
	obj-y   +=  ssd60xx_tp.o
endif

