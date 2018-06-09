
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifneq ($(CONFIG_ARCH_MTK_PROJECT),)
ARCH_MTK_PROJECT := $(shell echo $(CONFIG_ARCH_MTK_PROJECT))
obj-y   += core/
obj-y   += dct/dct/
endif


ifeq ($(CONFIG_MTK_MPU6050G),y)
obj-y   +=  accelerometer/mpu6050g/
endif

ifeq ($(CONFIG_MTK_MPU6050GY),y)
obj-y   +=  gyroscope/mpu6050gy/
endif

ifeq ($(CONFIG_MTK_AKM09911),y)
obj-y	+=  magnetometer/akm09911/
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
###obj-y   +=  touchpanel/
obj-y   +=  vibrator/
#obj-y   +=  wmt/
ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
obj-y +=hdmi/
endif
