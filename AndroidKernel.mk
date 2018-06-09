#
# Copyright (C) 2009-2011 The Android-x86 Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#

ifeq ($(LINUX_KERNEL_VERSION),kernel-3.10)
ifneq ($(strip $(MTK_EMULATOR_SUPPORT)),yes)
ifneq ($(strip $(MTK_PROJECT_NAME)),)

ifneq ($(wildcard $(call my-dir)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)),)

KERNEL_DIR := $(call my-dir)
ROOTDIR := $(abspath $(TOP))

ifneq ($(filter /% ~%,$(OUT_DIR)),)
KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
else
KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_OUT_ABS := $(ROOTDIR)/$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
endif

ifeq ($(TARGET_ARCH), arm64)
  ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/Image.gz-dtb
  else
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/Image.gz
  endif
else
  ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage-dtb
  else
    TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage
  endif
endif
TARGET_PREBUILT_KERNEL_BIN := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage.bin

TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
KERNEL_MODULES_SYMBOLS_OUT := $(PRODUCT_OUT)/symbols/system/lib/modules

ifneq ($(strip $(TARGET_NO_KERNEL)),true)
  INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
else
  INSTALLED_KERNEL_TARGET :=
endif

ifeq ($(KERNEL_CROSS_COMPILE),)
ifeq ($(TARGET_ARCH),arm)
KERNEL_CROSS_COMPILE := $(abspath $(TOPDIR)prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-eabi-$(TARGET_GCC_VERSION)/bin/arm-eabi-)
endif
ifeq ($(TARGET_ARCH),arm64)
KERNEL_CROSS_COMPILE := $(abspath $(TARGET_TOOLS_PREFIX))
endif
endif

define mv-modules
mv `find $(1)/lib -type f -name *.ko` $(1)
endef

define clean-module-folder
rm -rf $(1)/lib
endef

$(KERNEL_OUT):
	mkdir -p $@

$(KERNEL_MODULES_OUT):
	mkdir -p $@

.PHONY: kernel kernel-defconfig kernel-menuconfig kernel-modules clean-kernel
kernel-menuconfig: | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT_ABS) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) menuconfig

kernel-savedefconfig: | $(KERNEL_OUT)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_DIR)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)

$(TARGET_PREBUILT_KERNEL): kernel
	@echo Done kernel


USE_TRAPZ := true
$(info TRAPZ_GENERATED_KERNEL_HEADER = $(TRAPZ_GENERATED_KERNEL_HEADER)) # see buld/core/binary.mk

$(TARGET_KERNEL_CONFIG) kernel-defconfig: $(KERNEL_DIR)/arch/$(TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG) | $(KERNEL_OUT) $(TRAPZ_GENERATED_KERNEL_HEADER)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT_ABS) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) $(KERNEL_DEFCONFIG)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT_ABS) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) oldconfig

	cat $(KERNEL_DIR)/arch/arm/configs/trapz.config >> $@

$(KERNEL_HEADERS_INSTALL): $(TARGET_KERNEL_CONFIG) | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT_ABS) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) headers_install

kernel: $(TARGET_KERNEL_CONFIG) $(KERNEL_HEADERS_INSTALL) | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_OUT_ABS) ARCH=$(TARGET_ARCH) MTK_TARGET_PROJECT=${MTK_TARGET_PROJECT} CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(ROOTDIR) 

$(INSTALLED_KERNEL_TARGET): kernel

ifeq ($(strip $(MTK_HEADER_SUPPORT)),yes)
$(TARGET_PREBUILT_KERNEL_BIN): $(TARGET_PREBUILT_KERNEL) | $(HOST_OUT_EXECUTABLES)/mkimage
	$(HOST_OUT_EXECUTABLES)/mkimage $< KERNEL 0xffffffff > $@ 	

kernel-modules: kernel | $(KERNEL_MODULES_OUT)
	$(MAKE) $(KERNEL_MAKEFLAGS) modules
	$(MAKE) $(KERNEL_MAKEFLAGS) INSTALL_MOD_PATH=$(ROOTDIR)/$(KERNEL_MODULES_SYMBOLS_OUT) modules_install
	$(call mv-modules,$(ROOTDIR)/$(KERNEL_MODULES_SYMBOLS_OUT))
	$(call clean-module-folder,$(ROOTDIR)/$(KERNEL_MODULES_SYMBOLS_OUT))
	$(MAKE) $(KERNEL_MAKEFLAGS) INSTALL_MOD_PATH=$(ROOTDIR)/$(KERNEL_MODULES_OUT) INSTALL_MOD_STRIP=1 modules_install
	$(call mv-modules,$(ROOTDIR)/$(KERNEL_MODULES_OUT))
	$(call clean-module-folder,$(ROOTDIR)/$(KERNEL_MODULES_OUT))

$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL_BIN) | $(ACP)
	$(copy-file-to-target)
else
$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(copy-file-to-target)
endif

droidcore all_modules systemimage: kernel-modules

clean-kernel:
	@rm -rf $(KERNEL_OUT)
	@rm -rf $(KERNEL_MODULES_OUT)

endif
endif
endif
endif # Ifeq ($(LINUX_KERNEL_VERSION),kernel-3.10)

