# We can build either as part of a standalone Kernel build or as
# an external module.  Determine which mechanism is being used
ifeq ($(MODNAME),)
	KERNEL_BUILD := 1
else
	KERNEL_BUILD := 0
endif

# This branch builds for SDIO devices by default
ifeq ($(CONFIG_CLD_HL_SDIO_CORE),)
        CONFIG_CLD_HL_SDIO_CORE := y
endif

ifeq ($(CONFIG_CLD_HL_SDIO_CORE), y)
	CONFIG_QCA_WIFI_SDIO := 1
endif

ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
	CONFIG_ROME_IF = sdio
endif

ifndef CONFIG_ROME_IF
	#use pci as default interface
	CONFIG_ROME_IF = pci
endif

ifeq ($(KERNEL_BUILD),1)
	# These are provided in external module based builds
	# Need to explicitly define for Kernel-based builds
	MODNAME := wlan
	WLAN_ROOT := drivers/staging/qcacld-2.0
	WLAN_OPEN_SOURCE := 1
	CONFIG_QCA_WIFI_2_0 := 1
	CONFIG_QCA_WIFI_ISOC := 0
endif

ifeq ($(KERNEL_BUILD), 0)
	# These are configurable via Kconfig for kernel-based builds
	# Need to explicitly configure for Android-based builds

	#Flag to enable BlueTooth AMP feature
	CONFIG_PRIMA_WLAN_BTAMP := n

	#Flag to enable Legacy Fast Roaming(LFR)
	CONFIG_PRIMA_WLAN_LFR := y

	#JB kernel has PMKSA patches, hence enabling this flag
	CONFIG_PRIMA_WLAN_OKC := y

	# JB kernel has CPU enablement patches, so enable
	ifeq ($(CONFIG_ROME_IF),pci)
		CONFIG_PRIMA_WLAN_11AC_HIGH_TP := y
	endif
	ifeq ($(CONFIG_ROME_IF),usb)
		CONFIG_PRIMA_WLAN_11AC_HIGH_TP := n
	endif
	ifeq ($(CONFIG_ROME_IF),sdio)
		CONFIG_PRIMA_WLAN_11AC_HIGH_TP := n
	endif
	#Flag to enable TDLS feature
	CONFIG_QCOM_TDLS := y

	#Flag to enable Fast Transition (11r) feature
	CONFIG_QCOM_VOWIFI_11R := y

        ifneq ($(CONFIG_QCA_CLD_WLAN),)
                ifeq ($(CONFIG_CNSS),y)
        #Flag to enable Protected Managment Frames (11w) feature
                CONFIG_WLAN_FEATURE_11W := y
        #Flag to enable LTE CoEx feature
                CONFIG_QCOM_LTE_COEX := y
        #Flag to enable LPSS feature
                CONFIG_WLAN_FEATURE_LPSS := y
                endif
        endif

	#Flag to enable new Linux Regulatory implementation
	CONFIG_ENABLE_LINUX_REG := y

        #Flag to enable Protected Managment Frames (11w) feature
        ifeq ($(CONFIG_ROME_IF),usb)
                CONFIG_WLAN_FEATURE_11W := y
        endif
        ifeq ($(CONFIG_ROME_IF),sdio)
                CONFIG_WLAN_FEATURE_11W := y
        endif

	#Flag to enable NAN
	CONFIG_FEATURE_NAN := y

        #Flag to enable Linux QCMBR feature as default feature
        ifeq ($(CONFIG_LINUX_QCMBR),)
                CONFIG_LINUX_QCMBR :=y
        endif

	#Flag to enable TSF feature
	ifeq ($(CONFIG_ROME_IF),sdio)
		CONFIG_WLAN_SYNC_TSF := y
	endif

	#Flag to enable UDP response offload feature
	ifeq ($(CONFIG_ROME_IF),sdio)
		CONFIG_WLAN_UDP_RESPONSE_OFFLOAD := y
	endif

	#Flag to enable WoW Pulse GPIO Pin feature
	ifeq ($(CONFIG_ROME_IF),sdio)
		CONFIG_WLAN_WOW_PULSE := y
	endif
endif

ifeq ($(CONFIG_X86), y)
CONFIG_NON_QC_PLATFORM := y
endif

# To enable ESE upload, dependent config
# CONFIG_QCOM_ESE must be enabled.
CONFIG_QCOM_ESE := n
CONFIG_QCOM_ESE_UPLOAD := n

# Feature flags which are not (currently) configurable via Kconfig

#Whether to build debug version
BUILD_DEBUG_VERSION ?= 0

#Enable this flag to build driver in diag version
BUILD_DIAG_VERSION := 1

#Do we panic on bug?  default is to warn
PANIC_ON_BUG := 1

#Re-enable wifi on WDI timeout
RE_ENABLE_WIFI_ON_WDI_TIMEOUT := 0

ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
#Enable OS specific ADF abstraction
CONFIG_ADF_SUPPORT := 1

#Enable OL debug and wmi unified functions
CONFIG_ATH_PERF_PWR_OFFLOAD := 1

#Disable packet log
CONFIG_REMOVE_PKT_LOG := 0

#Enable 11AC TX
ifeq ($(CONFIG_ROME_IF),pci)
	CONFIG_ATH_11AC_TXCOMPACT := 1
endif
ifeq ($(CONFIG_ROME_IF),usb)
	CONFIG_ATH_11AC_TXCOMPACT := 0
endif
ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
CONFIG_ATH_11AC_TXCOMPACT := 0
endif

#Enable per vdev Tx desc pool
ifeq ($(CONFIG_ROME_IF),pci)
	CONFIG_PER_VDEV_TX_DESC_POOL := 0
endif
ifeq ($(CONFIG_ROME_IF),usb)
	CONFIG_PER_VDEV_TX_DESC_POOL := 1
endif
ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
	CONFIG_PER_VDEV_TX_DESC_POOL := 0
endif


#Enable OS specific IRQ abstraction
CONFIG_ATH_SUPPORT_SHARED_IRQ := 1

#Enable message based HIF instead of RAW access in BMI
ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
CONFIG_HIF_MESSAGE_BASED := 0
else
CONFIG_HIF_MESSAGE_BASED := 1
endif

#Enable PCI specific APIS (dma, etc)
ifeq ($(CONFIG_ROME_IF),pci)
	CONFIG_HIF_PCI := 1
endif
#Enable USB specific APIS
ifeq ($(CONFIG_ROME_IF),usb)
	CONFIG_HIF_USB := 1
endif

#Enable pci read/write config functions
ifeq ($(CONFIG_ROME_IF),pci)
	CONFIG_ATH_PCI := 1
endif
ifeq ($(CONFIG_ROME_IF),usb)
#CONFIG_ATH_PCI := 1
endif

#Enable IBSS support on CLD
CONFIG_QCA_IBSS_SUPPORT := 1

#Enable MDNS Offload
ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
CONFIG_MDNS_OFFLOAD_SUPPORT := 1
endif

#Enable power management suspend/resume functionality to PCI
CONFIG_ATH_BUS_PM := 1

#Enable dword alignment for IP header
CONFIG_IP_HDR_ALIGNMENT := 0

#Enable FLOWMAC module support
CONFIG_ATH_SUPPORT_FLOWMAC_MODULE := 0

#Enable spectral support
CONFIG_ATH_SUPPORT_SPECTRAL := 0

#Enable HOST statistics support
CONFIG_SUPPORT_HOST_STATISTICS := 0

#Enable WDI Event support
CONFIG_WDI_EVENT_ENABLE := 1

#Endianess selection
CONFIG_LITTLE_ENDIAN := 1

#Enable TX reclaim support
CONFIG_TX_CREDIT_RECLAIM_SUPPORT := 0

#Enable FTM support
CONFIG_QCA_WIFI_FTM := 1

#Enable Checksum Offload
CONFIG_CHECKSUM_OFFLOAD := 1

#Enable GTK offload
CONFIG_GTK_OFFLOAD := 1

#Enable EXT WOW
ifeq ($(CONFIG_ROME_IF),pci)
	CONFIG_EXT_WOW := 1
endif

#Set this to 1 to catch erroneous Target accesses during debug.
CONFIG_ATH_PCIE_ACCESS_DEBUG := 0
endif

#Enable IPA offload
ifeq ($(CONFIG_IPA), y)
CONFIG_IPA_OFFLOAD := 1
CONFIG_IPA_UC_OFFLOAD := 1
endif

#Enable Signed firmware support for split binary format
CONFIG_QCA_SIGNED_SPLIT_BINARY_SUPPORT := 0

#Enable single firmware binary format
CONFIG_QCA_SINGLE_BINARY_SUPPORT := 0

#Enable collecting target RAM dump after kernel panic
CONFIG_TARGET_RAMDUMP_AFTER_KERNEL_PANIC := 1

#Flag to enable Stats Ext implementation
CONFIG_FEATURE_STATS_EXT := 1


ifeq ($(CONFIG_CFG80211),y)
HAVE_CFG80211 := 1
else
ifeq ($(CONFIG_CFG80211),m)
HAVE_CFG80211 := 1
else
HAVE_CFG80211 := 0
endif
endif

ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
############ COMMON ############
COMMON_DIR :=	CORE/SERVICES/COMMON
COMMON_INC :=	-I$(WLAN_ROOT)/$(COMMON_DIR)

############ ADF ##############
ADF_DIR	:=	$(COMMON_DIR)/adf
ADF_INC :=	-I$(WLAN_ROOT)/$(ADF_DIR) \
		-I$(WLAN_ROOT)/$(ADF_DIR)/linux \
		-I$(WLAN_ROOT)/$(COMMON_DIR)/asf

ADF_OBJS :=     $(ADF_DIR)/adf_nbuf.o \
                $(ADF_DIR)/adf_os_lock.o \
                $(ADF_DIR)/adf_os_mem.o \
                $(ADF_DIR)/linux/adf_os_defer_pvt.o \
                $(ADF_DIR)/linux/adf_os_lock_pvt.o
endif

############ BAP ############
BAP_DIR :=	CORE/BAP
BAP_INC_DIR :=	$(BAP_DIR)/inc
BAP_SRC_DIR :=	$(BAP_DIR)/src

BAP_INC := 	-I$(WLAN_ROOT)/$(BAP_INC_DIR) \
		-I$(WLAN_ROOT)/$(BAP_SRC_DIR)

BAP_OBJS := 	$(BAP_SRC_DIR)/bapApiData.o \
		$(BAP_SRC_DIR)/bapApiDebug.o \
		$(BAP_SRC_DIR)/bapApiExt.o \
		$(BAP_SRC_DIR)/bapApiHCBB.o \
		$(BAP_SRC_DIR)/bapApiInfo.o \
		$(BAP_SRC_DIR)/bapApiLinkCntl.o \
		$(BAP_SRC_DIR)/bapApiLinkSupervision.o \
		$(BAP_SRC_DIR)/bapApiStatus.o \
		$(BAP_SRC_DIR)/bapApiTimer.o \
		$(BAP_SRC_DIR)/bapModule.o \
		$(BAP_SRC_DIR)/bapRsn8021xAuthFsm.o \
		$(BAP_SRC_DIR)/bapRsn8021xPrf.o \
		$(BAP_SRC_DIR)/bapRsn8021xSuppRsnFsm.o \
		$(BAP_SRC_DIR)/bapRsnAsfPacket.o \
		$(BAP_SRC_DIR)/bapRsnSsmAesKeyWrap.o \
		$(BAP_SRC_DIR)/bapRsnSsmEapol.o \
		$(BAP_SRC_DIR)/bapRsnSsmReplayCtr.o \
		$(BAP_SRC_DIR)/bapRsnTxRx.o \
		$(BAP_SRC_DIR)/btampFsm.o \
		$(BAP_SRC_DIR)/btampHCI.o

ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
############ HIF ############
HIF_DIR := CORE/SERVICES/HIF
HIF_DIR_OBJS :=  $(HIF_DIR)/ath_procfs.o

HIF_COMMON_DIR := CORE/SERVICES/HIF/common
HIF_COMMON_OBJS := $(HIF_COMMON_DIR)/hif_bmi_reg_access.o \
                   $(HIF_COMMON_DIR)/hif_diag_reg_access.o

HIF_SDIO_DIR := CORE/SERVICES/HIF/sdio
HIF_SDIO_OBJS := $(HIF_SDIO_DIR)/hif_sdio_send.o \
                 $(HIF_SDIO_DIR)/hif_sdio_dev.o \
                 $(HIF_SDIO_DIR)/hif_sdio.o \
                 $(HIF_SDIO_DIR)/hif_sdio_recv.o \
                 $(HIF_SDIO_DIR)/regtable.o \

HIF_SDIO_LINUX_DIR := $(HIF_SDIO_DIR)/linux
HIF_SDIO_LINUX_OBJS := $(HIF_SDIO_LINUX_DIR)/if_ath_sdio.o


HIF_SDIO_NATIVE_DIR := $(HIF_SDIO_LINUX_DIR)/native_sdio
HIF_SDIO_NATIVE_INC_DIR := $(HIF_SDIO_NATIVE_DIR)/include
HIF_SDIO_NATIVE_SRC_DIR := $(HIF_SDIO_NATIVE_DIR)/src

HIF_SDIO_NATIVE_OBJS := $(HIF_SDIO_NATIVE_SRC_DIR)/hif.o \
                        $(HIF_SDIO_NATIVE_SRC_DIR)/hif_scatter.o

HIF_INC := -I$(WLAN_ROOT)/$(HIF_COMMON_DIR) \
           -I$(WLAN_ROOT)/$(HIF_SDIO_DIR) \
           -I$(WLAN_ROOT)/$(HIF_SDIO_LINUX_DIR) \
           -I$(WLAN_ROOT)/$(HIF_SDIO_NATIVE_INC_DIR) \
           -I$(WLAN_ROOT)/$(HIF_SDIO_NATIVE_SRC_DIR)

HIF_OBJS := $(HIF_DIR_OBJS)\
			$(HIF_COMMON_OBJS)\
            $(HIF_SDIO_OBJS)\
            $(HIF_SDIO_LINUX_OBJS)\
            $(HIF_SDIO_NATIVE_OBJS)
else
############ DXE ############
DXE_DIR :=	CORE/DXE
DXE_INC_DIR :=	$(DXE_DIR)/inc
DXE_SRC_DIR :=	$(DXE_DIR)/src

DXE_INC := 	-I$(WLAN_ROOT)/$(DXE_INC_DIR) \
		-I$(WLAN_ROOT)/$(DXE_SRC_DIR)

HIF_DXE_DIR :=  CORE/SERVICES/HIF/DXE
HIF_DXE_INC :=  -I$(WLAN_ROOT)/$(HIF_DXE_DIR)

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
DXE_OBJS =      $(DXE_SRC_DIR)/wlan_qct_dxe.o \
                $(DXE_SRC_DIR)/wlan_qct_dxe_cfg_i.o
else
ifeq ($(CONFIG_QCA_WIFI_ISOC), 1)
HIF_DXE_INC :=  -I$(WLAN_ROOT)/$(HIF_DXE_DIR) \
		-I$(WLAN_ROOT)/$(HIF_DXE_DIR)/linux

HIF_DXE_OBJS:=  $(HIF_DXE_DIR)/hif_dxe.o \
                $(HIF_DXE_DIR)/hif_dxe_config.o \
                $(HIF_DXE_DIR)/linux/hif_dxe_os.o \
                $(HIF_DXE_DIR)/dmux_dxe.o \
                $(DXE_DIR)/htt_dxe_tx.o \
                $(DXE_DIR)/htt_dxe_fw_stats.o \
                $(DXE_DIR)/htt_dxe_h2t.o \
                $(DXE_DIR)/htt_dxe_t2h.o \
                $(DXE_DIR)/htt_dxe.o \
                $(DXE_DIR)/htt_dxe_rx.o

DXE_INC += $(HIF_DXE_INC)
DXE_OBJS := $(HIF_DXE_OBJS)
endif
endif
endif

############ HDD ############
HDD_DIR :=	CORE/HDD
HDD_INC_DIR :=	$(HDD_DIR)/inc
HDD_SRC_DIR :=	$(HDD_DIR)/src

HDD_INC := 	-I$(WLAN_ROOT)/$(HDD_INC_DIR) \
		-I$(WLAN_ROOT)/$(HDD_SRC_DIR)

HDD_OBJS := 	$(HDD_SRC_DIR)/bap_hdd_main.o \
		$(HDD_SRC_DIR)/wlan_hdd_assoc.o \
		$(HDD_SRC_DIR)/wlan_hdd_cfg.o \
		$(HDD_SRC_DIR)/wlan_hdd_debugfs.o \
		$(HDD_SRC_DIR)/wlan_hdd_dev_pwr.o \
		$(HDD_SRC_DIR)/wlan_hdd_dp_utils.o \
		$(HDD_SRC_DIR)/wlan_hdd_early_suspend.o \
		$(HDD_SRC_DIR)/wlan_hdd_ftm.o \
		$(HDD_SRC_DIR)/wlan_hdd_hostapd.o \
		$(HDD_SRC_DIR)/wlan_hdd_main.o \
		$(HDD_SRC_DIR)/wlan_hdd_mib.o \
		$(HDD_SRC_DIR)/wlan_hdd_oemdata.o \
		$(HDD_SRC_DIR)/wlan_hdd_scan.o \
		$(HDD_SRC_DIR)/wlan_hdd_softap_tx_rx.o \
		$(HDD_SRC_DIR)/wlan_hdd_tx_rx.o \
		$(HDD_SRC_DIR)/wlan_hdd_trace.o \
		$(HDD_SRC_DIR)/wlan_hdd_wext.o \
		$(HDD_SRC_DIR)/wlan_hdd_wmm.o \
		$(HDD_SRC_DIR)/wlan_hdd_wowl.o

ifeq ($(CONFIG_IPA_OFFLOAD), 1)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_ipa.o
endif

ifeq ($(CONFIG_MDNS_OFFLOAD_SUPPORT), 1)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_mdns_offload.o
endif

ifeq ($(HAVE_CFG80211),1)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_cfg80211.o \
		$(HDD_SRC_DIR)/wlan_hdd_p2p.o
endif

ifeq ($(CONFIG_QCOM_TDLS),y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_tdls.o
endif

ifeq ($(CONFIG_WLAN_SYNC_TSF),y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_tsf.o
endif

############ EPPING ############
EPPING_DIR :=	CORE/EPPING
EPPING_INC_DIR :=	$(EPPING_DIR)/inc
EPPING_SRC_DIR :=	$(EPPING_DIR)/src

EPPING_INC := 	-I$(WLAN_ROOT)/$(EPPING_INC_DIR)

EPPING_OBJS := $(EPPING_SRC_DIR)/epping_main.o \
		$(EPPING_SRC_DIR)/epping_txrx.o \
		$(EPPING_SRC_DIR)/epping_tx.o \
		$(EPPING_SRC_DIR)/epping_rx.o \
		$(EPPING_SRC_DIR)/epping_helper.o \


############ MAC ############
MAC_DIR :=	CORE/MAC
MAC_INC_DIR :=	$(MAC_DIR)/inc
MAC_SRC_DIR :=	$(MAC_DIR)/src

MAC_INC := 	-I$(WLAN_ROOT)/$(MAC_INC_DIR) \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/dph \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/include \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/include \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/lim

MAC_CFG_OBJS := $(MAC_SRC_DIR)/cfg/cfgApi.o \
		$(MAC_SRC_DIR)/cfg/cfgDebug.o \
		$(MAC_SRC_DIR)/cfg/cfgParamName.o \
		$(MAC_SRC_DIR)/cfg/cfgProcMsg.o \
		$(MAC_SRC_DIR)/cfg/cfgSendMsg.o

MAC_DPH_OBJS :=	$(MAC_SRC_DIR)/dph/dphHashTable.o

MAC_LIM_OBJS := $(MAC_SRC_DIR)/pe/lim/limAIDmgmt.o \
		$(MAC_SRC_DIR)/pe/lim/limAdmitControl.o \
		$(MAC_SRC_DIR)/pe/lim/limApi.o \
		$(MAC_SRC_DIR)/pe/lim/limAssocUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limDebug.o \
		$(MAC_SRC_DIR)/pe/lim/limFT.o \
		$(MAC_SRC_DIR)/pe/lim/limIbssPeerMgmt.o \
		$(MAC_SRC_DIR)/pe/lim/limLinkMonitoringAlgo.o \
		$(MAC_SRC_DIR)/pe/lim/limLogDump.o \
		$(MAC_SRC_DIR)/pe/lim/limP2P.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessActionFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessAssocReqFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessAssocRspFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessAuthFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessBeaconFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessCfgUpdates.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessDeauthFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessDisassocFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessLmmMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessMessageQueue.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessMlmReqMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessMlmRspMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessProbeReqFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessProbeRspFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessSmeReqMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limPropExtsUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limRoamingAlgo.o \
		$(MAC_SRC_DIR)/pe/lim/limScanResultUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSecurityUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSendManagementFrames.o \
		$(MAC_SRC_DIR)/pe/lim/limSendMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limSendSmeRspMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limSerDesUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSession.o \
		$(MAC_SRC_DIR)/pe/lim/limSessionUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSmeReqUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limStaHashApi.o \
		$(MAC_SRC_DIR)/pe/lim/limTimerUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limTrace.o \
		$(MAC_SRC_DIR)/pe/lim/limUtils.o

ifeq ($(CONFIG_QCOM_ESE),y)
ifneq ($(CONFIG_QCOM_ESE_UPLOAD),y)
MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/limProcessEseFrame.o
endif
endif

ifeq ($(CONFIG_QCOM_TDLS),y)
MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/limProcessTdls.o
endif

MAC_PMM_OBJS := $(MAC_SRC_DIR)/pe/pmm/pmmAP.o \
		$(MAC_SRC_DIR)/pe/pmm/pmmApi.o \
		$(MAC_SRC_DIR)/pe/pmm/pmmDebug.o

MAC_SCH_OBJS := $(MAC_SRC_DIR)/pe/sch/schApi.o \
		$(MAC_SRC_DIR)/pe/sch/schBeaconGen.o \
		$(MAC_SRC_DIR)/pe/sch/schBeaconProcess.o \
		$(MAC_SRC_DIR)/pe/sch/schDebug.o \
		$(MAC_SRC_DIR)/pe/sch/schMessage.o

MAC_RRM_OBJS :=	$(MAC_SRC_DIR)/pe/rrm/rrmApi.o

MAC_OBJS := 	$(MAC_CFG_OBJS) \
		$(MAC_DPH_OBJS) \
		$(MAC_LIM_OBJS) \
		$(MAC_PMM_OBJS) \
		$(MAC_SCH_OBJS) \
		$(MAC_RRM_OBJS)

############ SAP ############
SAP_DIR :=	CORE/SAP
SAP_INC_DIR :=	$(SAP_DIR)/inc
SAP_SRC_DIR :=	$(SAP_DIR)/src

SAP_INC := 	-I$(WLAN_ROOT)/$(SAP_INC_DIR) \
		-I$(WLAN_ROOT)/$(SAP_SRC_DIR)

SAP_OBJS :=	$(SAP_SRC_DIR)/sapApiLinkCntl.o \
		$(SAP_SRC_DIR)/sapChSelect.o \
		$(SAP_SRC_DIR)/sapFsm.o \
		$(SAP_SRC_DIR)/sapModule.o

############ DFS ############ 350
DFS_DIR :=	CORE/SERVICES/DFS
DFS_INC_DIR :=	$(DFS_DIR)/inc
DFS_SRC_DIR :=	$(DFS_DIR)/src

DFS_INC :=	-I$(WLAN_ROOT)/$(DFS_INC_DIR) \
		-I$(WLAN_ROOT)/$(DFS_SRC_DIR)

DFS_OBJS :=	$(DFS_SRC_DIR)/dfs_bindetects.o \
		$(DFS_SRC_DIR)/dfs.o \
		$(DFS_SRC_DIR)/dfs_debug.o\
		$(DFS_SRC_DIR)/dfs_fcc_bin5.o\
		$(DFS_SRC_DIR)/dfs_init.o\
		$(DFS_SRC_DIR)/dfs_misc.o\
		$(DFS_SRC_DIR)/dfs_nol.o\
		$(DFS_SRC_DIR)/dfs_phyerr_tlv.o\
		$(DFS_SRC_DIR)/dfs_process_phyerr.o\
		$(DFS_SRC_DIR)/dfs_process_radarevent.o\
		$(DFS_SRC_DIR)/dfs_staggered.o

############ SME ############
SME_DIR :=	CORE/SME
SME_INC_DIR :=	$(SME_DIR)/inc
SME_SRC_DIR :=	$(SME_DIR)/src

SME_INC := 	-I$(WLAN_ROOT)/$(SME_INC_DIR) \
		-I$(WLAN_ROOT)/$(SME_SRC_DIR)/csr

SME_CCM_OBJS := $(SME_SRC_DIR)/ccm/ccmApi.o \
		$(SME_SRC_DIR)/ccm/ccmLogDump.o

SME_CSR_OBJS := $(SME_SRC_DIR)/csr/csrApiRoam.o \
		$(SME_SRC_DIR)/csr/csrApiScan.o \
		$(SME_SRC_DIR)/csr/csrCmdProcess.o \
		$(SME_SRC_DIR)/csr/csrLinkList.o \
		$(SME_SRC_DIR)/csr/csrLogDump.o \
		$(SME_SRC_DIR)/csr/csrNeighborRoam.o \
		$(SME_SRC_DIR)/csr/csrUtil.o

ifeq ($(CONFIG_QCOM_ESE),y)
ifneq ($(CONFIG_QCOM_ESE_UPLOAD),y)
SME_CSR_OBJS += $(SME_SRC_DIR)/csr/csrEse.o
endif
endif

ifeq ($(CONFIG_QCOM_TDLS),y)
SME_CSR_OBJS += $(SME_SRC_DIR)/csr/csrTdlsProcess.o
endif

SME_PMC_OBJS := $(SME_SRC_DIR)/pmc/pmcApi.o \
		$(SME_SRC_DIR)/pmc/pmc.o \
		$(SME_SRC_DIR)/pmc/pmcLogDump.o

SME_QOS_OBJS := $(SME_SRC_DIR)/QoS/sme_Qos.o

SME_CMN_OBJS := $(SME_SRC_DIR)/sme_common/sme_Api.o \
		$(SME_SRC_DIR)/sme_common/sme_FTApi.o \
		$(SME_SRC_DIR)/sme_common/sme_Trace.o

SME_BTC_OBJS := $(SME_SRC_DIR)/btc/btcApi.o

SME_OEM_DATA_OBJS := $(SME_SRC_DIR)/oemData/oemDataApi.o

SME_P2P_OBJS = $(SME_SRC_DIR)/p2p/p2p_Api.o

SME_RRM_OBJS := $(SME_SRC_DIR)/rrm/sme_rrm.o

ifeq ($(CONFIG_FEATURE_NAN),y)
SME_NAN_OBJS = $(SME_SRC_DIR)/nan/nan_Api.o
endif

SME_OBJS :=	$(SME_BTC_OBJS) \
		$(SME_CCM_OBJS) \
		$(SME_CMN_OBJS) \
		$(SME_CSR_OBJS) \
		$(SME_OEM_DATA_OBJS) \
		$(SME_P2P_OBJS) \
		$(SME_PMC_OBJS) \
		$(SME_QOS_OBJS) \
		$(SME_RRM_OBJS) \
		$(SME_NAN_OBJS)

############ SVC ############
SVC_DIR :=	CORE/SVC
SVC_INC_DIR :=	$(SVC_DIR)/inc
SVC_SRC_DIR :=	$(SVC_DIR)/src

SVC_INC := 	-I$(WLAN_ROOT)/$(SVC_INC_DIR) \
		-I$(WLAN_ROOT)/$(SVC_DIR)/external

BTC_SRC_DIR :=	$(SVC_SRC_DIR)/btc
BTC_OBJS :=	$(BTC_SRC_DIR)/wlan_btc_svc.o

NLINK_SRC_DIR := $(SVC_SRC_DIR)/nlink
NLINK_OBJS :=	$(NLINK_SRC_DIR)/wlan_nlink_srv.o

PTT_SRC_DIR :=	$(SVC_SRC_DIR)/ptt
PTT_OBJS :=	$(PTT_SRC_DIR)/wlan_ptt_sock_svc.o

WLAN_LOGGING_SRC_DIR := $(SVC_SRC_DIR)/logging
WLAN_LOGGING_OBJS := $(WLAN_LOGGING_SRC_DIR)/wlan_logging_sock_svc.o

SVC_OBJS :=	$(BTC_OBJS) \
		$(NLINK_OBJS) \
		$(PTT_OBJS) \
		$(WLAN_LOGGING_OBJS)

############ SYS ############
SYS_DIR :=	CORE/SYS

SYS_INC := 	-I$(WLAN_ROOT)/$(SYS_DIR)/common/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/pal/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/platform/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/system/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/utils/inc

SYS_COMMON_SRC_DIR := $(SYS_DIR)/common/src
SYS_LEGACY_SRC_DIR := $(SYS_DIR)/legacy/src
SYS_OBJS :=	$(SYS_COMMON_SRC_DIR)/wlan_qct_sys.o \
		$(SYS_LEGACY_SRC_DIR)/pal/src/palApiComm.o \
		$(SYS_LEGACY_SRC_DIR)/pal/src/palTimer.o \
		$(SYS_LEGACY_SRC_DIR)/platform/src/VossWrapper.o \
		$(SYS_LEGACY_SRC_DIR)/system/src/macInitApi.o \
		$(SYS_LEGACY_SRC_DIR)/system/src/sysEntryFunc.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/dot11f.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/logApi.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/logDump.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/macTrace.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/parserApi.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/utilsApi.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/utilsParser.o

############ TL ############
TL_DIR :=	CORE/TL
TL_INC_DIR :=	$(TL_DIR)/inc
TL_SRC_DIR :=	$(TL_DIR)/src

TL_INC := 	-I$(WLAN_ROOT)/$(TL_INC_DIR) \
		-I$(WLAN_ROOT)/$(TL_SRC_DIR)

TL_OBJS := 	$(TL_SRC_DIR)/wlan_qct_tl.o \
		$(TL_SRC_DIR)/wlan_qct_tl_ba.o \
		$(TL_SRC_DIR)/wlan_qct_tl_hosupport.o

############ VOSS ############
VOSS_DIR :=	CORE/VOSS
VOSS_INC_DIR :=	$(VOSS_DIR)/inc
VOSS_SRC_DIR :=	$(VOSS_DIR)/src

VOSS_INC := 	-I$(WLAN_ROOT)/$(VOSS_INC_DIR) \
		-I$(WLAN_ROOT)/$(VOSS_SRC_DIR)

VOSS_OBJS :=    $(VOSS_SRC_DIR)/vos_api.o \
		$(VOSS_SRC_DIR)/vos_event.o \
		$(VOSS_SRC_DIR)/vos_getBin.o \
		$(VOSS_SRC_DIR)/vos_list.o \
		$(VOSS_SRC_DIR)/vos_lock.o \
		$(VOSS_SRC_DIR)/vos_memory.o \
		$(VOSS_SRC_DIR)/vos_mq.o \
		$(VOSS_SRC_DIR)/vos_nvitem.o \
		$(VOSS_SRC_DIR)/vos_packet.o \
		$(VOSS_SRC_DIR)/vos_sched.o \
		$(VOSS_SRC_DIR)/vos_threads.o \
		$(VOSS_SRC_DIR)/vos_timer.o \
		$(VOSS_SRC_DIR)/vos_trace.o \
		$(VOSS_SRC_DIR)/vos_types.o \
                $(VOSS_SRC_DIR)/vos_utils.o

ifeq ($(BUILD_DIAG_VERSION),1)
VOSS_OBJS += $(VOSS_SRC_DIR)/vos_diag.o
endif

ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
########### BMI ###########
BMI_DIR := CORE/SERVICES/BMI

BMI_INC := -I$(WLAN_ROOT)/$(BMI_DIR)

BMI_OBJS := $(BMI_DIR)/bmi.o \
            $(BMI_DIR)/ol_fw.o

########### WMI ###########
WMI_DIR := CORE/SERVICES/WMI

WMI_INC := -I$(WLAN_ROOT)/$(WMI_DIR)

WMI_OBJS := $(WMI_DIR)/wmi_unified.o \
	    $(WMI_DIR)/wmi_tlv_helper.o

########### FWLOG ###########
FWLOG_DIR := CORE/UTILS/FWLOG

FWLOG_INC := -I$(WLAN_ROOT)/$(FWLOG_DIR)

FWLOG_OBJS := $(FWLOG_DIR)/dbglog_host.o

############ TLSHIM ############
TLSHIM_DIR := CORE/CLD_TXRX/TLSHIM
TLSHIM_INC := -I$(WLAN_ROOT)/$(TLSHIM_DIR)

TLSHIM_OBJS := $(TLSHIM_DIR)/tl_shim.o

############ TXRX ############
TXRX_DIR :=     CORE/CLD_TXRX/TXRX
TXRX_INC :=     -I$(WLAN_ROOT)/$(TXRX_DIR)

TXRX_OBJS := $(TXRX_DIR)/ol_txrx.o \
                $(TXRX_DIR)/ol_cfg.o \
                $(TXRX_DIR)/ol_rx.o \
                $(TXRX_DIR)/ol_rx_fwd.o \
                $(TXRX_DIR)/ol_txrx.o \
                $(TXRX_DIR)/ol_rx_defrag.o \
                $(TXRX_DIR)/ol_tx_desc.o \
                $(TXRX_DIR)/ol_tx_classify.o \
                $(TXRX_DIR)/ol_tx.o \
                $(TXRX_DIR)/ol_rx_reorder_timeout.o \
                $(TXRX_DIR)/ol_rx_reorder.o \
                $(TXRX_DIR)/ol_rx_pn.o \
                $(TXRX_DIR)/ol_tx_queue.o \
                $(TXRX_DIR)/ol_txrx_peer_find.o \
                $(TXRX_DIR)/ol_txrx_event.o \
                $(TXRX_DIR)/ol_txrx_encap.o \
                $(TXRX_DIR)/ol_tx_send.o \
                $(TXRX_DIR)/ol_tx_sched.o

############ PKTLOG ############
PKTLOG_DIR :=      CORE/UTILS/PKTLOG
PKTLOG_INC :=      -I$(WLAN_ROOT)/$(PKTLOG_DIR)/include

PKTLOG_OBJS :=	$(PKTLOG_DIR)/pktlog_ac.o \
		$(PKTLOG_DIR)/pktlog_internal.o \
		$(PKTLOG_DIR)/linux_ac.o

############ HTT ############
HTT_DIR :=      CORE/CLD_TXRX/HTT
HTT_INC :=      -I$(WLAN_ROOT)/$(HTT_DIR)

ifeq ($(CONFIG_QCA_WIFI_ISOC), 0)
HTT_OBJS := $(HTT_DIR)/htt_tx.o \
            $(HTT_DIR)/htt.o \
            $(HTT_DIR)/htt_t2h.o \
            $(HTT_DIR)/htt_h2t.o \
            $(HTT_DIR)/htt_fw_stats.o \
            $(HTT_DIR)/htt_rx.o
endif

############## HTC ##########
HTC_DIR := CORE/SERVICES/HTC

HTC_INC := -I$(WLAN_ROOT)/$(HTC_DIR)

ifeq ($(CONFIG_QCA_WIFI_ISOC), 1)
HTC_INC += -I$(WLAN_ROOT)/$(HTC_DIR)/linux/
HTC_OBJS := $(HTC_DIR)/linux/htc_smd.o
else
HTC_OBJS := $(HTC_DIR)/htc.o \
            $(HTC_DIR)/htc_send.o \
            $(HTC_DIR)/htc_recv.o \
            $(HTC_DIR)/htc_services.o
endif

ifneq ($(CONFIG_QCA_WIFI_SDIO), 1)
########### HIF ###########
HIF_DIR := CORE/SERVICES/HIF
ifeq ($(CONFIG_HIF_PCI), 1)
HIF_PCIE_DIR := $(HIF_DIR)/PCIe

HIF_INC := -I$(WLAN_ROOT)/$(HIF_PCIE_DIR)

HIF_OBJS := $(HIF_DIR)/ath_procfs.o

HIF_PCIE_OBJS := $(HIF_PCIE_DIR)/copy_engine.o \
                 $(HIF_PCIE_DIR)/hif_pci.o \
                 $(HIF_PCIE_DIR)/if_pci.o \
                 $(HIF_PCIE_DIR)/regtable.o \
                 $(HIF_PCIE_DIR)/mp_dev.o

HIF_OBJS += $(HIF_PCIE_OBJS)
endif
ifeq ($(CONFIG_HIF_USB), 1)
HIF_USB_DIR := $(HIF_DIR)/USB

HIF_INC := -I$(WLAN_ROOT)/$(HIF_USB_DIR)

HIF_OBJS := $(HIF_DIR)/ath_procfs.o

HIF_USB_OBJS := $(HIF_USB_DIR)/usbdrv.o \
                 $(HIF_USB_DIR)/hif_usb.o \
                 $(HIF_USB_DIR)/if_usb.o \
                 $(HIF_USB_DIR)/regtable.o

HIF_OBJS += $(HIF_USB_OBJS)
endif
endif

############ WMA ############
WMA_DIR :=      CORE/SERVICES/WMA

WMA_INC :=      -I$(WLAN_ROOT)/$(WMA_DIR)

WMA_OBJS :=     $(WMA_DIR)/wma.o \
		$(WMA_DIR)/wma_dfs_interface.o \
		$(WMA_DIR)/wlan_nv.o


ifeq ($(CONFIG_QCA_WIFI_ISOC), 1)
WMA_OBJS +=     $(WMA_DIR)/wma_isoc.o
else
WMA_OBJS +=     $(WMA_DIR)/regdomain.o
endif
endif

############ WDA ############
WDA_DIR :=	CORE/WDA
WDA_INC_DIR :=	$(WDA_DIR)/inc
WDA_SRC_DIR :=	$(WDA_DIR)/src

WDA_INC := 	-I$(WLAN_ROOT)/$(WDA_INC_DIR) \
		-I$(WLAN_ROOT)/$(WDA_INC_DIR)/legacy \
		-I$(WLAN_ROOT)/$(WDA_SRC_DIR)

WDA_OBJS :=	$(WDA_SRC_DIR)/wlan_qct_wda_debug.o \
		$(WDA_SRC_DIR)/wlan_qct_wda_legacy.o

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
WDA_OBJS +=	$(WDA_SRC_DIR)/wlan_qct_wda.o \
		$(WDA_SRC_DIR)/wlan_qct_wda_ds.o
endif

############ WDI ############
WDI_DIR :=	CORE/WDI

WDI_CP_INC :=	-I$(WLAN_ROOT)/$(WDI_DIR)/CP/inc/

WDI_CP_SRC_DIR := $(WDI_DIR)/CP/src
WDI_CP_OBJS :=	$(WDI_CP_SRC_DIR)/wlan_qct_wdi.o \
		$(WDI_CP_SRC_DIR)/wlan_qct_wdi_dp.o \
		$(WDI_CP_SRC_DIR)/wlan_qct_wdi_sta.o

WDI_DP_INC := -I$(WLAN_ROOT)/$(WDI_DIR)/DP/inc/

WDI_DP_SRC_DIR := $(WDI_DIR)/DP/src
WDI_DP_OBJS :=	$(WDI_DP_SRC_DIR)/wlan_qct_wdi_bd.o \
		$(WDI_DP_SRC_DIR)/wlan_qct_wdi_ds.o

WDI_TRP_INC :=	-I$(WLAN_ROOT)/$(WDI_DIR)/TRP/CTS/inc/ \
		-I$(WLAN_ROOT)/$(WDI_DIR)/TRP/DTS/inc/

WDI_TRP_CTS_SRC_DIR :=	$(WDI_DIR)/TRP/CTS/src
WDI_TRP_CTS_OBJS :=	$(WDI_TRP_CTS_SRC_DIR)/wlan_qct_wdi_cts.o

WDI_TRP_DTS_SRC_DIR :=	$(WDI_DIR)/TRP/DTS/src
WDI_TRP_DTS_OBJS :=	$(WDI_TRP_DTS_SRC_DIR)/wlan_qct_wdi_dts.o

WDI_TRP_OBJS :=	$(WDI_TRP_CTS_OBJS) \
		$(WDI_TRP_DTS_OBJS)

WDI_WPAL_INC := -I$(WLAN_ROOT)/$(WDI_DIR)/WPAL/inc

WDI_WPAL_SRC_DIR := $(WDI_DIR)/WPAL/src
WDI_WPAL_OBJS := $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_trace.o

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
WDI_WPAL_OBJS += $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_api.o \
		 $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_device.o \
		 $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_msg.o \
		 $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_packet.o \
		 $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_sync.o \
		 $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_timer.o
endif

WDI_INC :=	$(WDI_CP_INC) \
		$(WDI_DP_INC) \
		$(WDI_TRP_INC) \
		$(WDI_WPAL_INC)

WDI_OBJS :=	$(WDI_WPAL_OBJS)

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
WDI_OBJS +=	$(WDI_CP_OBJS) \
		$(WDI_DP_OBJS) \
		$(WDI_TRP_OBJS)
endif


WCNSS_INC :=	-I$(WLAN_ROOT)/wcnss/inc

LINUX_INC :=	-Iinclude/linux

INCS :=		$(BAP_INC) \
		$(DXE_INC) \
		$(HDD_INC) \
		$(EPPING_INC) \
		$(LINUX_INC) \
		$(MAC_INC) \
		$(WCNSS_INC) \
		$(SAP_INC) \
		$(SME_INC) \
		$(SVC_INC) \
		$(SYS_INC) \
		$(TL_INC) \
		$(VOSS_INC) \
		$(WDA_INC) \
		$(WDI_INC) \
		$(DFS_INC)

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
INCS +=		$(DXE_INC)
else
INCS +=		$(WMA_INC) \
		$(COMMON_INC) \
		$(WMI_INC) \
		$(FWLOG_INC) \
		$(ADF_INC) \
		$(TLSHIM_INC) \
		$(TXRX_INC) \
		$(PKTLOG_INC) \
		$(HTT_INC) \
		$(HTC_INC) \
		$(DFS_INC)

ifeq ($(CONFIG_QCA_WIFI_ISOC), 0)
INCS +=		$(HIF_INC) \
		$(BMI_INC)

ifeq ($(CONFIG_REMOVE_PKT_LOG), 0)
INCS +=		$(PKTLOG_INC)
endif

else
INCS +=		$(DXE_INC)
endif

endif

OBJS :=		$(BAP_OBJS) \
		$(HDD_OBJS) \
		$(EPPING_OBJS) \
		$(MAC_OBJS) \
		$(SAP_OBJS) \
		$(SME_OBJS) \
		$(SVC_OBJS) \
		$(SYS_OBJS) \
		$(VOSS_OBJS) \
		$(WDA_OBJS) \
		$(WDI_OBJS) \
		$(DFS_OBJS)

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
OBJS +=		$(DXE_OBJS) \
		$(TL_OBJS)
else
OBJS +=		$(WMA_OBJS) \
		$(TLSHIM_OBJS) \
		$(TXRX_OBJS) \
		$(WMI_OBJS) \
		$(FWLOG_OBJS) \
		$(HTC_OBJS) \
		$(ADF_OBJS) \
		$(DFS_OBJS)

ifeq ($(CONFIG_QCA_WIFI_ISOC), 0)
OBJS +=		$(HIF_OBJS) \
		$(BMI_OBJS) \
		$(HTT_OBJS)

ifeq ($(CONFIG_REMOVE_PKT_LOG), 0)
OBJS +=		$(PKTLOG_OBJS)
endif

else
OBJS +=		$(DXE_OBJS)
endif

endif

EXTRA_CFLAGS += $(INCS)

CDEFINES :=	-DANI_LITTLE_BYTE_ENDIAN \
		-DANI_LITTLE_BIT_ENDIAN \
		-DQC_WLAN_CHIPSET_QCA_CLD \
		-DINTEGRATION_READY \
		-DDOT11F_LITTLE_ENDIAN_HOST \
		-DGEN6_ONWARDS \
		-DANI_COMPILER_TYPE_GCC \
		-DANI_OS_TYPE_ANDROID=6 \
		-DANI_LOGDUMP \
		-DWLAN_PERF \
		-DPTT_SOCK_SVC_ENABLE \
		-Wall\
		-D__linux__ \
		-DHAL_SELF_STA_PER_BSS=1 \
		-DWLAN_FEATURE_VOWIFI_11R \
		-DWLAN_FEATURE_NEIGHBOR_ROAMING \
		-DFEATURE_WLAN_WAPI \
		-DFEATURE_OEM_DATA_SUPPORT\
		-DSOFTAP_CHANNEL_RANGE \
		-DWLAN_AP_STA_CONCURRENCY \
		-DFEATURE_WLAN_SCAN_PNO \
		-DWLAN_FEATURE_PACKET_FILTERING \
		-DWLAN_FEATURE_VOWIFI \
		-DWLAN_FEATURE_11AC \
		-DWLAN_ENABLE_AGEIE_ON_SCAN_RESULTS \
		-DWLAN_NS_OFFLOAD \
		-DWLAN_ACTIVEMODE_OFFLOAD_FEATURE \
		-DWLAN_FEATURE_HOLD_RX_WAKELOCK \
		-DWLAN_SOFTAP_VSTA_FEATURE \
		-DWLAN_FEATURE_ROAM_SCAN_OFFLOAD \
		-DWLAN_FEATURE_GTK_OFFLOAD \
		-DWLAN_WAKEUP_EVENTS \
		-DFEATURE_WLAN_RA_FILTERING\
	        -DWLAN_KD_READY_NOTIFIER \
		-DFEATURE_WLAN_BATCH_SCAN \
		-DFEATURE_WLAN_LPHB \
		-DFEATURE_WLAN_PAL_TIMER_DISABLE \
		-DFEATURE_WLAN_PAL_MEM_DISABLE \
		-DQCA_SUPPORT_TX_THROTTLE \
		-DWMI_INTERFACE_EVENT_LOGGING \
		-DATH_SUPPORT_WAPI \
		-DWLAN_FEATURE_LINK_LAYER_STATS \
		-DWLAN_LOGGING_SOCK_SVC_ENABLE \
		-DFEATURE_WLAN_EXTSCAN \
		-DQCA_LL_TX_FLOW_CT

ifeq ($(CONFIG_NL80211_TESTMODE), y)
CDEFINES +=	-DWLAN_NL80211_TESTMODE
endif

ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
CDEFINES +=     -DCONFIG_HL_SUPPORT \
                -DCONFIG_AR6320_SUPPORT \
                -DSDIO_3_0 \
                -DHIF_SDIO \
                -DCONFIG_ATH_PROCFS_DIAG_SUPPORT \
                -DHIF_MBOX_SLEEP_WAR \
                -DQCA_BAD_PEER_TX_FLOW_CL
endif

ifeq ($(CONFIG_QCA_WIFI_SDIO), 1)
CDEFINES += -DDHCP_SERVER_OFFLOAD
CDEFINES += -DWLAN_FEATURE_GPIO_LED_FLASHING
CDEFINES += -DWLAN_FEATURE_APFIND
endif

ifeq ($(CONFIG_MDNS_OFFLOAD_SUPPORT), 1)
CDEFINES += -DMDNS_OFFLOAD
endif

ifeq ($(CONFIG_ARCH_MSM), y)
CDEFINES += -DMSM_PLATFORM
endif

ifeq ($(CONFIG_QCA_WIFI_2_0), 0)
else
CDEFINES +=	-DOSIF_NEED_RX_PEER_ID \
		-DQCA_SUPPORT_TXRX_LOCAL_PEER_ID
ifeq ($(CONFIG_ROME_IF),pci)
CDEFINES +=	-DQCA_LL_TX_FLOW_CT \
		-DQCA_SUPPORT_TXRX_VDEV_PAUSE_LL \
		-DQCA_SUPPORT_TXRX_VDEV_LL_TXQ
endif
endif

ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
ifeq ($(CONFIG_DEBUG_LL),y)
CDEFINES +=    	-DQCA_PKT_PROTO_TRACE
endif
endif

ifneq ($(CONFIG_QCA_CLD_WLAN),)
CDEFINES += -DWCN_PRONTO
CDEFINES += -DWCN_PRONTO_V1
endif

ifeq ($(BUILD_DEBUG_VERSION),1)
CDEFINES +=	-DWLAN_DEBUG \
		-DWLAN_FEATURE_NEIGHBOR_ROAMING_DEBUG \
		-DWLAN_FEATURE_VOWIFI_11R_DEBUG \
		-DWLAN_FEATURE_P2P_DEBUG \
		-DWLANTL_DEBUG\
		-DTRACE_RECORD \
		-DLIM_TRACE_RECORD \
		-DSME_TRACE_RECORD \
		-DPE_DEBUG_LOGW \
		-DPE_DEBUG_LOGE \
		-DDEBUG
endif

ifeq ($(CONFIG_SLUB_DEBUG_ON),y)
CDEFINES += -DTIMER_MANAGER
CDEFINES += -DMEMORY_DEBUG
endif

ifeq ($(HAVE_CFG80211),1)
CDEFINES += -DWLAN_FEATURE_P2P
CDEFINES += -DWLAN_FEATURE_WFD
ifeq ($(CONFIG_QCOM_VOWIFI_11R),y)
CDEFINES += -DKERNEL_SUPPORT_11R_CFG80211
CDEFINES += -DUSE_80211_WMMTSPEC_FOR_RIC
endif
endif

ifeq ($(CONFIG_QCOM_ESE),y)
CDEFINES += -DFEATURE_WLAN_ESE
CDEFINES += -DQCA_COMPUTE_TX_DELAY
CDEFINES += -DQCA_COMPUTE_TX_DELAY_PER_TID
ifeq ($(CONFIG_QCOM_ESE_UPLOAD),y)
CDEFINES += -DFEATURE_WLAN_ESE_UPLOAD
endif
endif

#normally, TDLS negative behavior is not needed
ifeq ($(CONFIG_QCOM_TDLS),y)
CDEFINES += -DFEATURE_WLAN_TDLS
ifeq ($(BUILD_DEBUG_VERSION),1)
CDEFINES += -DWLAN_FEATURE_TDLS_DEBUG
endif
CDEFINES += -DCONFIG_TDLS_IMPLICIT
#CDEFINES += -DFEATURE_WLAN_TDLS_NEGATIVE
#Code under FEATURE_WLAN_TDLS_INTERNAL is ported from volans, This code
#is not tested only verifed that it compiles. This is not required for
#supplicant based implementation
#CDEFINES += -DFEATURE_WLAN_TDLS_INTERNAL
endif

ifeq ($(CONFIG_PRIMA_WLAN_BTAMP),y)
CDEFINES += -DWLAN_BTAMP_FEATURE
endif

ifeq ($(CONFIG_PRIMA_WLAN_LFR),y)
CDEFINES += -DFEATURE_WLAN_LFR
endif

ifeq ($(CONFIG_PRIMA_WLAN_OKC),y)
CDEFINES += -DFEATURE_WLAN_OKC
endif

ifeq ($(CONFIG_PRIMA_WLAN_11AC_HIGH_TP),y)
CDEFINES += -DWLAN_FEATURE_11AC_HIGH_TP
endif

ifeq ($(BUILD_DIAG_VERSION),1)
CDEFINES += -DFEATURE_WLAN_DIAG_SUPPORT
CDEFINES += -DFEATURE_WLAN_DIAG_SUPPORT_CSR
CDEFINES += -DFEATURE_WLAN_DIAG_SUPPORT_LIM
ifeq ($(CONFIG_HIF_PCI), 1)
CDEFINES += -DCONFIG_ATH_PROCFS_DIAG_SUPPORT
endif
endif

ifeq ($(CONFIG_HIF_USB), 1)
CDEFINES += -DCONFIG_ATH_PROCFS_DIAG_SUPPORT
CDEFINES += -DQCA_SUPPORT_OL_RX_REORDER_TIMEOUT
CDEFINES += -DCONFIG_ATH_PCIE_MAX_PERF=0 -DCONFIG_ATH_PCIE_AWAKE_WHILE_DRIVER_LOAD=0 -DCONFIG_DISABLE_CDC_MAX_PERF_WAR=0
CDEFINES += -DQCA_TX_HTT2_SUPPORT
CDEFINES += -DCONFIG_HDD_INIT_WITH_RTNL_LOCK
ifeq ($(CONFIG_HIF_USB_TASKLET), 1)
CDEFINES += -DHIF_USB_TASKLET
endif
endif

# enable the MAC Address auto-generation feature
CDEFINES += -DWLAN_AUTOGEN_MACADDR_FEATURE

ifeq ($(CONFIG_WLAN_FEATURE_11W),y)
CDEFINES += -DWLAN_FEATURE_11W
endif

ifeq ($(CONFIG_QCOM_LTE_COEX),y)
CDEFINES += -DFEATURE_WLAN_CH_AVOID
endif

ifeq ($(CONFIG_WLAN_FEATURE_LPSS),y)
CDEFINES += -DWLAN_FEATURE_LPSS
endif

ifeq ($(PANIC_ON_BUG),1)
CDEFINES += -DPANIC_ON_BUG
endif

ifeq ($(RE_ENABLE_WIFI_ON_WDI_TIMEOUT),1)
CDEFINES += -DWDI_RE_ENABLE_WIFI_ON_WDI_TIMEOUT
endif

ifeq ($(WLAN_OPEN_SOURCE), 1)
CDEFINES += -DWLAN_OPEN_SOURCE
endif

ifeq ($(CONFIG_ENABLE_LINUX_REG), y)
ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
CDEFINES += -DCONFIG_ENABLE_LINUX_REG
endif
endif

ifeq ($(CONFIG_FEATURE_STATS_EXT), 1)
CDEFINES += -DWLAN_FEATURE_STATS_EXT
endif

ifeq ($(CONFIG_FEATURE_NAN),y)
CDEFINES += -DWLAN_FEATURE_NAN
endif

ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
CDEFINES += -DQCA_WIFI_2_0
endif

ifeq ($(CONFIG_QCA_WIFI_ISOC), 1)
CDEFINES += -DQCA_WIFI_ISOC
CDEFINES += -DANI_BUS_TYPE_PLATFORM=1
endif

ifeq ($(CONFIG_QCA_WIFI_2_0), 1)
ifeq ($(CONFIG_QCA_IBSS_SUPPORT), 1)
CDEFINES += -DQCA_IBSS_SUPPORT
endif

#Enable the OS specific ADF abstraction
ifeq ($(CONFIG_ADF_SUPPORT), 1)
CDEFINES += -DADF_SUPPORT
endif

#Enable OL debug and wmi unified functions
ifeq ($(CONFIG_ATH_PERF_PWR_OFFLOAD), 1)
CDEFINES += -DATH_PERF_PWR_OFFLOAD
endif

#Disable packet log
ifeq ($(CONFIG_REMOVE_PKT_LOG), 1)
CDEFINES += -DREMOVE_PKT_LOG
endif

#Enable 11AC TX
ifeq ($(CONFIG_ATH_11AC_TXCOMPACT), 1)
CDEFINES += -DATH_11AC_TXCOMPACT
endif

#Enable per vdev Tx desc pool
ifeq ($(CONFIG_PER_VDEV_TX_DESC_POOL), 1)
CDEFINES += -DCONFIG_PER_VDEV_TX_DESC_POOL
endif

#Enable OS specific IRQ abstraction
ifeq ($(CONFIG_ATH_SUPPORT_SHARED_IRQ), 1)
CDEFINES += -DATH_SUPPORT_SHARED_IRQ
endif

#Enable message based HIF instead of RAW access in BMI
ifeq ($(CONFIG_HIF_MESSAGE_BASED), 1)
CDEFINES += -DHIF_MESSAGE_BASED
endif

#Enable PCI specific APIS (dma, etc)
ifeq ($(CONFIG_HIF_PCI), 1)
CDEFINES += -DHIF_PCI
endif

#Enable USB specific APIS
ifeq ($(CONFIG_HIF_USB), 1)
CDEFINES += -DHIF_USB
CDEFINES += -DCONFIG_HL_SUPPORT
CDEFINES += -DCONFIG_FW_LOGS_BASED_ON_INI
endif

#Enable pci read/write config functions
ifeq ($(CONFIG_ATH_PCI), 1)
CDEFINES += -DATH_PCI
endif

#Enable power management suspend/resume functionality
ifeq ($(CONFIG_ATH_BUS_PM), 1)
CDEFINES += -DATH_BUS_PM
endif

#Enable dword alignment for IP header
ifeq ($(CONFIG_IP_HDR_ALIGNMENT), 1)
CDEFINES += -DPERE_IP_HDR_ALIGNMENT_WAR
endif

#Enable FLOWMAC module support
ifeq ($(CONFIG_ATH_SUPPORT_FLOWMAC_MODULE), 1)
CDEFINES += -DATH_SUPPORT_FLOWMAC_MODULE
endif

#Enable spectral support
ifeq ($(CONFIG_ATH_SUPPORT_SPECTRAL), 1)
CDEFINES += -DATH_SUPPORT_SPECTRAL
endif

#Enable WDI Event support
ifeq ($(CONFIG_WDI_EVENT_ENABLE), 1)
CDEFINES += -DWDI_EVENT_ENABLE
endif

#Endianess selection
ifeq ($(CONFIG_LITTLE_ENDIAN), 1)
AH_LITTLE_ENDIAN=1234
CDEFINES += -DAH_BYTE_ORDER=$(AH_LITTLE_ENDIAN)
else
AH_BIG_ENDIAN=4321
CDEFINES += -DAH_BYTE_ORDER=$(AH_BIG_ENDIAN)
CDEFINES += -DBIG_ENDIAN_HOST
endif

#Enable TX reclaim support
ifeq ($(CONFIG_TX_CREDIT_RECLAIM_SUPPORT), 1)
CDEFINES += -DTX_CREDIT_RECLAIM_SUPPORT
endif

#Enable FTM support
ifeq ($(CONFIG_QCA_WIFI_FTM), 1)
CDEFINES += -DQCA_WIFI_FTM
endif

#Enable Checksum Offload support
ifeq ($(CONFIG_CHECKSUM_OFFLOAD), 1)
CDEFINES += -DCHECKSUM_OFFLOAD
endif

#Enable Checksum Offload support
ifeq ($(CONFIG_IPA_OFFLOAD), 1)
CDEFINES += -DIPA_OFFLOAD -DHDD_IPA_USE_IPA_RM_TIMER
endif

ifneq ($(CONFIG_ARCH_MDM9630), y)
ifeq ($(CONFIG_IPA_UC_OFFLOAD), 1)
CDEFINES += -DIPA_UC_OFFLOAD
endif
endif

#Enable GTK Offload
ifeq ($(CONFIG_GTK_OFFLOAD), 1)
CDEFINES += -DWLAN_FEATURE_GTK_OFFLOAD
CDEFINES += -DIGTK_OFFLOAD
endif

#Enable GTK Offload
ifeq ($(CONFIG_EXT_WOW), 1)
CDEFINES += -DWLAN_FEATURE_EXTWOW_SUPPORT
endif

#Mark it as SMP Kernel
ifeq ($(CONFIG_SMP),y)
CDEFINES += -DQCA_CONFIG_SMP
endif
endif

#features specific to mdm9630
ifeq ($(CONFIG_ARCH_MDM9630), y)

#enable MCC TO SCC switch
CDEFINES += -DFEATURE_WLAN_MCC_TO_SCC_SWITCH

#enable wlan auto shutdown feature for mdm9630
CDEFINES += -DFEATURE_WLAN_AUTO_SHUTDOWN

#enable for MBSSID
CDEFINES += -DWLAN_FEATURE_MBSSID

#Green AP feature
CDEFINES += -DFEATURE_GREEN_AP

#Enable 4address scheme for mdm9630
CDEFINES += -DFEATURE_WLAN_STA_4ADDR_SCHEME

#Disable STA-AP Mode DFS support
CDEFINES += -DFEATURE_WLAN_STA_AP_MODE_DFS_DISABLE

#Enable OBSS feature for mdm9630
CDEFINES += -DQCA_HT_2040_COEX

else

#Open P2P device interface only for non-MDM9630 platform
CDEFINES += -DWLAN_OPEN_P2P_INTERFACE

#Enable 2.4 GHz social channels in 5 GHz only mode for p2p usage
CDEFINES += -DWLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY

endif

#Enable Signed firmware support for split binary format
ifeq ($(CONFIG_QCA_SIGNED_SPLIT_BINARY_SUPPORT), 1)
CDEFINES += -DQCA_SIGNED_SPLIT_BINARY_SUPPORT
endif

#Enable single firmware binary format
ifeq ($(CONFIG_QCA_SINGLE_BINARY_SUPPORT), 1)
CDEFINES += -DQCA_SINGLE_BINARY_SUPPORT
endif

#Enable collecting target RAM dump after kernel panic
ifeq ($(CONFIG_TARGET_RAMDUMP_AFTER_KERNEL_PANIC), 1)
CDEFINES += -DTARGET_RAMDUMP_AFTER_KERNEL_PANIC
endif

ifeq ($(CONFIG_ATH_PCIE_ACCESS_DEBUG), 1)
CDEFINES += -DCONFIG_ATH_PCIE_ACCESS_DEBUG
endif

#Flag to enable/disable WLAN D0-WOW
ifeq ($(CONFIG_PCI_MSM), y)
CDEFINES += -DFEATURE_WLAN_D0WOW
endif

# Some kernel include files are being moved.  Check to see if
# the old version of the files are present

ifneq ($(wildcard $(srctree)/arch/$(SRCARCH)/mach-msm/include/mach/msm_smd.h),)
CDEFINES += -DEXISTS_MSM_SMD
endif

ifneq ($(wildcard $(srctree)/arch/$(SRCARCH)/mach-msm/include/mach/msm_smsm.h),)
CDEFINES += -DEXISTS_MSM_SMSM
endif

# Enable feature support fo Linux version QCMBR
ifeq ($(CONFIG_LINUX_QCMBR),y)
CDEFINES += -DLINUX_QCMBR
endif

# Enable feature Software AP Authentication Offload
ifeq ($(SAP_AUTH_OFFLOAD),1)
CDEFINES += -DSAP_AUTH_OFFLOAD
endif

# Enable featue sync tsf between multi devices
ifeq ($(CONFIG_WLAN_SYNC_TSF),y)
CDEFINES += -DWLAN_FEATURE_TSF
endif

# Enable target dump for non-qualcomm platform
ifeq ($(CONFIG_NON_QC_PLATFORM), y)
CDEFINES += -DCONFIG_NON_QC_PLATFORM
ifeq ($(CONFIG_CLD_HL_SDIO_CORE), y)
CDEFINES += -DTARGET_DUMP_FOR_NON_QC_PLATFORM
endif
endif

ifeq ($(CONFIG_WLAN_UDP_RESPONSE_OFFLOAD), y)
CDEFINES += -DWLAN_FEATURE_UDP_RESPONSE_OFFLOAD
endif

ifeq ($(CONFIG_WLAN_WOW_PULSE), y)
CDEFINES += -DWLAN_FEATURE_WOW_PULSE
endif


KBUILD_CPPFLAGS += $(CDEFINES)

# Currently, for versions of gcc which support it, the kernel Makefile
# is disabling the maybe-uninitialized warning.  Re-enable it for the
# WLAN driver.  Note that we must use EXTRA_CFLAGS here so that it
# will override the kernel settings.
ifeq ($(call cc-option-yn, -Wmaybe-uninitialized),y)
EXTRA_CFLAGS += -Wmaybe-uninitialized
endif

# Coding style is so bad that GCC >= 6.x complains about indentation.
# Below flag is used as a workaround to mute those warnings.
GCC_MAJOR := `$(CROSS_COMPILE)gcc -dumpversion | cut -d'.' -f1`
ifeq ($(shell test $(GCC_MAJOR) -gt 5; echo $$?),0)
EXTRA_CFLAGS += -Wno-misleading-indentation
endif

# Module information used by KBuild framework
obj-$(CONFIG_QCA_CLD_WLAN) += $(MODNAME).o
$(MODNAME)-y := $(OBJS)
