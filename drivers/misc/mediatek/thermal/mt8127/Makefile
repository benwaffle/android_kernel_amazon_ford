include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/thermal

obj-$(CONFIG_THERMAL) := mtk_thermal_platform.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o

obj-$(CONFIG_THERMAL) += mtk_cpufreq_cooling.o
obj-$(CONFIG_THERMAL) += mtk_hotplug_cooling.o
obj-$(CONFIG_THERMAL) += mtk_backlight_cooling.o
obj-$(CONFIG_THERMAL) += mtk_charger_cooling.o
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o

ifeq ($(CONFIG_MTK_PMIC_MT6397),y)
    obj-$(CONFIG_THERMAL) += mtk_ts_pmic6397.o
else
    obj-$(CONFIG_THERMAL) += mtk_ts_pmic6323.o
endif

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif
