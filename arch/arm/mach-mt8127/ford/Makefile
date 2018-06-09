
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifneq ($(CONFIG_ARCH_MTK_PROJECT),)
ARCH_MTK_PROJECT := $(shell echo $(CONFIG_ARCH_MTK_PROJECT))
obj-y   += core/
ifeq ($(CONFIG_AUSTIN_PROJECT),y)
	obj-y   += dct/dct_austin/
else
	obj-y   += dct/dct/
endif
endif


ifeq ($(CONFIG_CUSTOM_KERNEL_ACCELEROMETER),y)
obj-y   +=  accelerometer/
endif

ifeq ($(CONFIG_MTK_KXTJ2_1009),y)
obj-y   +=  accelerometer/KXTJ2_1009/
endif

ifeq ($(CONFIG_MTK_BMA253),y)
obj-y   +=  accelerometer/BMA253/
endif

ifeq ($(CONFIG_MTK_MC3410),y)
obj-y   +=  accelerometer/MC3410-NEW/
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_GYROSCOPE),y)
obj-y   +=  gyroscope/
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_MAGNETOMETER),y)
obj-y	+=  magnetometer/
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_ALSPS),y)
obj-y   +=  alsps/
endif

obj-y   +=  cam_cal/
obj-y   +=  camera/camera/
obj-y   +=  eeprom/
###obj-y   +=  flashlight/
obj-y   +=  flashlight/constant_flashlight/
obj-y   +=  headset/
###zbj-y   +=  imgsensor/
obj-y   +=  keypad/
obj-y   +=  leds/
obj-y   +=  lens/
obj-y   +=  sound/
###obj-y   +=  ssw/
obj-y   +=  touchpanel/
obj-y   +=  vibrator/
#obj-y   +=  wmt/
ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
obj-y +=hdmi/
endif
