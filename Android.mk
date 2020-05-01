# Android makefile for the WLAN Module

MAJOR_VERSION := $(shell echo $(PLATFORM_VERSION) | cut -f1 -d.)

# This makefile is only meant for Android < 10, afterwards it is built differently
ifneq ($(shell test $(MAJOR_VERSION) -ge 10 && echo true), true)
# Build/Package only in case of supported target
ifneq ($(filter imx6 imx7 imx8,$(TARGET_BOARD_PLATFORM)),)

LOCAL_PATH := $(call my-dir)
LOCAL_PATH_BACKUP := $(ANDROID_BUILD_TOP)/$(LOCAL_PATH)

ifeq ($(shell test $(MAJOR_VERSION) -ge 7 && echo true), true)
ifeq ($(TARGET_ARCH),arm64)
CROSS_COMPILE := $(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
else
CROSS_COMPILE := $(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-
endif
else
ifeq ($(shell test $(MAJOR_VERSION) -ge 6 && echo true), true)
CROSS_COMPILE := $(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
else
CROSS_COMPILE := $(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-
endif
endif

MAKE_OPTIONS := ARCH=$(TARGET_ARCH)
MAKE_OPTIONS += CROSS_COMPILE=$(CROSS_COMPILE)
MAKE_OPTIONS += WLAN_ROOT=$(LOCAL_PATH_BACKUP)
MAKE_OPTIONS += MODNAME=wlan
MAKE_OPTIONS += WLAN_OPEN_SOURCE=1
MAKE_OPTIONS += CONFIG_CLD_HL_SDIO_CORE=y
MAKE_OPTIONS += CONFIG_QCA_WIFI_ISOC=0
MAKE_OPTIONS += CONFIG_QCA_WIFI_2_0=1
MAKE_OPTIONS += CONFIG_QCA_CLD_WLAN=m

# Kernel path has changed starting with Oreo
ifeq ($(shell test $(MAJOR_VERSION) -ge 8 && echo true), true)
KERNEL_SRC := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
else
KERNEL_SRC := $(ANDROID_BUILD_TOP)/kernel_imx
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := qcacld_wlan.ko
# Install in /vendor starting with Oreo
ifeq ($(shell test $(MAJOR_VERSION) -ge 8 && echo true), true)
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_PATH  := $(TARGET_OUT_VENDOR)/lib/modules/
else
LOCAL_MODULE_PATH  := $(TARGET_OUT)/lib/modules/
endif
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS  := optional
include $(BUILD_PREBUILT)

QCACLD_INTERMEDIATES := $(TARGET_OUT_INTERMEDIATES)/$(LOCAL_MODULE_CLASS)/$(LOCAL_MODULE)_intermediates

# Override the default build target in order to issue our own custom command.
# Note that the module name is wlan.ko by default, we then change it to
# qcacld_wlan.ko in order to be more explicit.
$(LOCAL_BUILT_MODULE): bootimage
	$(MAKE) -C $(KERNEL_SRC) M=$(LOCAL_PATH_BACKUP) $(MAKE_OPTIONS) modules
	$(hide) $(CROSS_COMPILE)strip --strip-debug $(LOCAL_PATH_BACKUP)/wlan.ko
	$(hide) mkdir -p $(QCACLD_INTERMEDIATES)
	$(hide) $(ACP) $(LOCAL_PATH_BACKUP)/wlan.ko $(QCACLD_INTERMEDIATES)/qcacld_wlan.ko

endif
endif
