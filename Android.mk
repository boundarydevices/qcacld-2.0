# Android makefile for the WLAN Module

# Build/Package only in case of supported target
ifneq ($(filter imx6 imx7,$(TARGET_BOARD_PLATFORM)),)

LOCAL_PATH := $(call my-dir)
LOCAL_PATH_BACKUP := $(ANDROID_BUILD_TOP)/$(LOCAL_PATH)
COMPAT_MODULE_PATH := $(LOCAL_PATH_BACKUP)/compat/compat.ko
CFG80211_MODULE_PATH := $(LOCAL_PATH_BACKUP)/net/wireless/cfg80211.ko
WLAN_MODULE_PATH := $(LOCAL_PATH_BACKUP)/drivers/net/wireless/qcacld-2.0/wlan.ko

include $(CLEAR_VARS)
LOCAL_MODULE       := qcacld_wlan.ko
LOCAL_MODULE_PATH  := $(TARGET_OUT)/lib/modules/
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS  := optional
include $(BUILD_PREBUILT)

CROSS_COMPILE=$(ANDROID_BUILD_TOP)/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-

KERNEL_SRC=$(ANDROID_BUILD_TOP)/kernel_imx

QCACLD_INTERMEDIATES := $(TARGET_OUT_INTERMEDIATES)/$(LOCAL_MODULE_CLASS)/$(LOCAL_MODULE)_intermediates

MAKE_OPTIONS := ARCH=arm
MAKE_OPTIONS += CROSS_COMPILE=$(CROSS_COMPILE)
MAKE_OPTIONS += KLIB_BUILD=$(KERNEL_SRC)
MAKE_OPTIONS += KLIB=$(QCACLD_INTERMEDIATES)

# Override the default build target in order to issue our own custom command.
# Note that the module name is wlan.ko by default, we then change it to
# qcacld_wlan.ko in order to be more explicit.
$(LOCAL_BUILT_MODULE): $(TARGET_PREBUILT_KERNEL)
	$(MAKE) $(MAKE_OPTIONS) -C $(LOCAL_PATH_BACKUP) defconfig-bdsdmac
	$(MAKE) $(MAKE_OPTIONS) -C $(LOCAL_PATH_BACKUP)
	$(hide) $(CROSS_COMPILE)strip --strip-debug $(WLAN_MODULE_PATH)
	$(hide) mkdir -p $(QCACLD_INTERMEDIATES)
	$(hide) $(ACP) $(WLAN_MODULE_PATH) $(QCACLD_INTERMEDIATES)/qcacld_wlan.ko
	$(hide) mkdir -p $(TARGET_OUT)/lib/modules/
	$(hide) $(ACP) $(COMPAT_MODULE_PATH) $(CFG80211_MODULE_PATH) $(TARGET_OUT)/lib/modules/

endif
