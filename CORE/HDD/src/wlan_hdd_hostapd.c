/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**========================================================================

  \file  wlan_hdd_hostapd.c
  \brief WLAN Host Device Driver implementation

  ========================================================================*/
/**=========================================================================
                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  04/5/09     Shailender     Created module.
  06/03/10    js - Added support to hostapd driven deauth/disassoc/mic failure
  ==========================================================================*/
/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <linux/semaphore.h>
#include <linux/compat.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <linux/etherdevice.h>
#include <wlan_hdd_includes.h>
#include <qc_sap_ioctl.h>
#include <wlan_hdd_hostapd.h>
#include <sapApi.h>
#include <sapInternal.h>
#include <wlan_qct_tl.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_main.h>
#include <linux/netdevice.h>
#include <linux/mmc/sdio_func.h>
#include "wlan_nlink_common.h"
#include "wlan_btc_svc.h"
#include <bap_hdd_main.h>
#include "wlan_hdd_p2p.h"
#ifdef IPA_OFFLOAD
#include <wlan_hdd_ipa.h>
#endif
#include "cfgApi.h"
#include "wniCfgAp.h"
#include "wlan_hdd_misc.h"

#if defined CONFIG_CNSS
#include <net/cnss.h>
#endif

#ifdef QCA_WIFI_2_0
#include "wma.h"
#ifdef DEBUG
#include "wma_api.h"
#endif
extern int process_wma_set_command(int sessid, int paramid,
                                   int sval, int vpdev);
#endif /* QCA_WIFI_2_0 */
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"
#include "wlan_hdd_cfg.h"
#include <wlan_hdd_wowl.h>
#include "wlan_hdd_tsf.h"

#define    IS_UP(_dev) \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define    IS_UP_AUTO(_ic) \
    (IS_UP((_ic)->ic_dev) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)
#define WE_WLAN_VERSION     1
#define WE_GET_STA_INFO_SIZE 30
/* WEXT limition: MAX allowed buf len for any *
 * IW_PRIV_TYPE_CHAR is 2Kbytes *
 */
#define WE_SAP_MAX_STA_INFO 0x7FF

#define RC_2_RATE_IDX(_rc)        ((_rc) & 0x7)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#define RC_2_RATE_IDX_11AC(_rc)        ((_rc) & 0xf)
#define HT_RC_2_STREAMS_11AC(_rc)    ((((_rc) & 0x30) >> 4) + 1)

#define SAP_24GHZ_CH_COUNT (14)

/*---------------------------------------------------------------------------
 *   Function definitions
 *-------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_open() - HDD Open function for hostapd interface

  This is called in response to ifconfig up

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
int hdd_hostapd_open (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = netdev_priv(dev);

   ENTER();

   MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                    TRACE_CODE_HDD_HOSTAPD_OPEN_REQUEST, NO_SESSION, 0));

   if (WLAN_HDD_GET_CTX(pAdapter)->isLoadInProgress ||
        WLAN_HDD_GET_CTX(pAdapter)->isUnloadInProgress)
   {
       hddLog(LOGE, FL("Driver load/unload in progress, ignore adapter open"));
       goto done;
   }

   WLAN_HDD_GET_AP_CTX_PTR(pAdapter)->dfs_cac_block_tx = VOS_TRUE;

   //Turn ON carrier state
   netif_carrier_on(dev);
   //Enable all Tx queues
   netif_tx_start_all_queues(dev);
done:
   EXIT();
   return 0;
}
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_stop() - HDD stop function for hostapd interface

  This is called in response to ifconfig down

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
int hdd_hostapd_stop (struct net_device *dev)
{
   ENTER();

   //Stop all tx queues
   netif_tx_disable(dev);

   //Turn OFF carrier state
   netif_carrier_off(dev);

   EXIT();
   return 0;
}
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_uninit() - HDD uninit function

  This is called during the netdev unregister to uninitialize all data
associated with the device

  \param  - dev Pointer to net_device structure

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_hostapd_uninit (struct net_device *dev)
{
   hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);

   ENTER();

   if (pHostapdAdapter && pHostapdAdapter->pHddCtx)
   {
      hdd_deinit_adapter(pHostapdAdapter->pHddCtx, pHostapdAdapter);

      /* after uninit our adapter structure will no longer be valid */
      pHostapdAdapter->dev = NULL;
   }

   EXIT();
}


/**============================================================================
  @brief hdd_hostapd_hard_start_xmit() - Function registered with the Linux OS for
  transmitting packets. There are 2 versions of this function. One that uses
  locked queue and other that uses lockless queues. Both have been retained to
  do some performance testing
  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra network device

  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued succesfully
  ===========================================================================*/
int hdd_hostapd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    return 0;
}
int hdd_hostapd_change_mtu(struct net_device *dev, int new_mtu)
{
    return 0;
}

static int hdd_hostapd_driver_command(hdd_adapter_t *pAdapter,
                                      hdd_priv_data_t *priv_data)
{
   tANI_U8 *command = NULL;
   int ret = 0;

   /*
    * Note that valid pointers are provided by caller
    */

   if (priv_data->total_len <= 0 ||
       priv_data->total_len > HOSTAPD_IOCTL_COMMAND_STRLEN_MAX)
   {
      /* below we allocate one more byte for command buffer.
       * To avoid addition overflow total_len should be
       * smaller than INT_MAX. */
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: integer out of range len %d",
             __func__, priv_data->total_len);
      ret = -EFAULT;
      goto exit;
   }

   /* Allocate +1 for '\0' */
   command = kmalloc((priv_data->total_len + 1), GFP_KERNEL);
   if (!command)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to allocate memory", __func__);
      ret = -ENOMEM;
      goto exit;
   }

   if (copy_from_user(command, priv_data->buf, priv_data->total_len))
   {
      ret = -EFAULT;
      goto exit;
   }

   /* Make sure the command is NUL-terminated */
   command[priv_data->total_len] = '\0';

   hddLog(VOS_TRACE_LEVEL_INFO,
          "***HOSTAPD*** : Received %s cmd from Wi-Fi GUI***", command);

   if (strncmp(command, "P2P_SET_NOA", 11) == 0)
   {
      hdd_setP2pNoa(pAdapter->dev, command);
   }
   else if (strncmp(command, "P2P_SET_PS", 10) == 0)
   {
      hdd_setP2pOpps(pAdapter->dev, command);
   }
#ifdef FEATURE_WLAN_BATCH_SCAN
   else if (strncmp(command, "WLS_BATCHING", 12) == 0)
   {
      ret = hdd_handle_batch_scan_ioctl(pAdapter, priv_data, command);
   }
#endif
   else if (strncmp(command, "SET_SAP_CHANNEL_LIST", 20) == 0)
   {
      /*
       * command should be a string having format
       * SET_SAP_CHANNEL_LIST <num channels> <channels seperated by spaces>
       */
      hddLog(VOS_TRACE_LEVEL_INFO,
             "%s: Received Command to Set Preferred Channels for SAP",
             __func__);

#ifdef WLAN_FEATURE_MBSSID
      ret = sapSetPreferredChannel(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
                                   command);
#else
      ret = sapSetPreferredChannel(command);
#endif
   }

exit:
   if (command)
   {
      kfree(command);
   }
   return ret;
}

#ifdef CONFIG_COMPAT
static int hdd_hostapd_driver_compat_ioctl(hdd_adapter_t *pAdapter,
                                           struct ifreq *ifr)
{
   struct {
      compat_uptr_t buf;
      int used_len;
      int total_len;
   } compat_priv_data;
   hdd_priv_data_t priv_data;
   int ret = 0;

   /*
    * Note that pAdapter and ifr have already been verified by caller,
    * and HDD context has also been validated
    */
   if (copy_from_user(&compat_priv_data, ifr->ifr_data,
                      sizeof(compat_priv_data))) {
       ret = -EFAULT;
       goto exit;
   }
   priv_data.buf = compat_ptr(compat_priv_data.buf);
   priv_data.used_len = compat_priv_data.used_len;
   priv_data.total_len = compat_priv_data.total_len;
   ret = hdd_hostapd_driver_command(pAdapter, &priv_data);
 exit:
   return ret;
}
#else /* CONFIG_COMPAT */
static int hdd_hostapd_driver_compat_ioctl(hdd_adapter_t *pAdapter,
                                           struct ifreq *ifr)
{
   /* will never be invoked */
   return 0;
}
#endif /* CONFIG_COMPAT */

static int hdd_hostapd_driver_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   hdd_priv_data_t priv_data;
   int ret = 0;

   /*
    * Note that pAdapter and ifr have already been verified by caller,
    * and HDD context has also been validated
    */
   if (copy_from_user(&priv_data, ifr->ifr_data, sizeof(priv_data))) {
      ret = -EFAULT;
   } else {
      ret = hdd_hostapd_driver_command(pAdapter, &priv_data);
   }
   return ret;
}

static int hdd_hostapd_ioctl(struct net_device *dev,
                             struct ifreq *ifr, int cmd)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   int ret;

   if (dev != pAdapter->dev) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: HDD adapter/dev inconsistency", __func__);
      ret = -ENODEV;
      goto exit;
   }

   if ((!ifr) || (!ifr->ifr_data))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("ifr or ifr->ifr_data is NULL"));
      ret = -EINVAL;
      goto exit;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (ret) {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: invalid context", __func__);
      ret = -EBUSY;
      goto exit;
   }

   switch (cmd) {
   case (SIOCDEVPRIVATE + 1):
      if (is_compat_task())
         ret = hdd_hostapd_driver_compat_ioctl(pAdapter, ifr);
      else
         ret = hdd_hostapd_driver_ioctl(pAdapter, ifr);
      break;
   default:
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: unknown ioctl %d",
             __func__, cmd);
      ret = -EINVAL;
      break;
   }
 exit:
   return ret;
}

#ifdef QCA_HT_2040_COEX
VOS_STATUS hdd_set_sap_ht2040_mode(hdd_adapter_t *pHostapdAdapter,
                                   tANI_U8 channel_type)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: change HT20/40 mode", __func__);

    if (WLAN_HDD_SOFTAP == pHostapdAdapter->device_mode) {
        hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
        if ( NULL == hHal ) {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Hal ctx is null", __func__);
            return VOS_STATUS_E_FAULT;
        }
        halStatus = sme_SetHT2040Mode(hHal, pHostapdAdapter->sessionId,
                                      channel_type);
        if (halStatus == eHAL_STATUS_FAILURE ) {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Failed to change HT20/40 mode", __func__);
            return VOS_STATUS_E_FAILURE;
        }
    }
    return VOS_STATUS_SUCCESS;
}
#endif

/**---------------------------------------------------------------------------

  \brief hdd_hostapd_set_mac_address() -
   This function sets the user specified mac address using
   the command ifconfig wlanX hw ether <mac adress>.

  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int hdd_hostapd_set_mac_address(struct net_device *dev, void *addr)
{
   struct sockaddr *psta_mac_addr = addr;
   ENTER();
   memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);
   EXIT();
   return 0;
}
void hdd_hostapd_inactivity_timer_cb(v_PVOID_t usrDataForCallback)
{
    struct net_device *dev = (struct net_device *)usrDataForCallback;
    v_BYTE_t we_custom_event[64];
    union iwreq_data wrqu;
#ifdef DISABLE_CONCURRENCY_AUTOSAVE
    VOS_STATUS vos_status;
    hdd_adapter_t *pHostapdAdapter;
    hdd_ap_ctx_t *pHddApCtx;
#endif /*DISABLE_CONCURRENCY_AUTOSAVE */

    /* event_name space-delimiter driver_module_name */
    /* Format of the event is "AUTO-SHUT.indication" " " "module_name" */
    char * autoShutEvent = "AUTO-SHUT.indication" " "  KBUILD_MODNAME;
    int event_len = strlen(autoShutEvent) + 1; /* For the NULL at the end */

    ENTER();

#ifdef DISABLE_CONCURRENCY_AUTOSAVE
    if (vos_concurrent_open_sessions_running())
    {
       /*
              This timer routine is going to be called only when AP
              persona is up.
              If there are concurrent sessions running we do not want
              to shut down the Bss.Instead we run the timer again so
              that if Autosave is enabled next time and other session
              was down only then we bring down AP
             */
        pHostapdAdapter = netdev_priv(dev);
        pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
        vos_status = vos_timer_start(
         &pHddApCtx->hdd_ap_inactivity_timer,
         (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff
          * 1000);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
        {
            hddLog(LOGE, FL("Failed to init AP inactivity timer"));
        }
        EXIT();
        return;
    }
#endif /*DISABLE_CONCURRENCY_AUTOSAVE */
    memset(&we_custom_event, '\0', sizeof(we_custom_event));
    memcpy(&we_custom_event, autoShutEvent, event_len);

    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = event_len;

    hddLog(LOG1, FL("Shutting down AP interface due to inactivity"));
    wireless_send_event(dev, IWEVCUSTOM, &wrqu, (char *)we_custom_event);

    EXIT();
}

VOS_STATUS hdd_change_mcc_go_beacon_interval(hdd_adapter_t *pHostapdAdapter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: UPDATE Beacon Params", __func__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Hal ctx is null", __func__);
            return VOS_STATUS_E_FAULT;
        }
        halStatus = sme_ChangeMCCBeaconInterval(hHal, pHostapdAdapter->sessionId);
        if(halStatus == eHAL_STATUS_FAILURE ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Failed to update Beacon Params", __func__);
            return VOS_STATUS_E_FAILURE;
        }
    }
    return VOS_STATUS_SUCCESS;
}

void hdd_clear_all_sta(hdd_adapter_t *pHostapdAdapter, v_PVOID_t usrDataForCallback)
{
    v_U8_t staId = 0;
    struct net_device *dev;
    dev = (struct net_device *)usrDataForCallback;

    hddLog(LOGE, FL("Clearing all the STA entry...."));
    for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
    {
        if ( pHostapdAdapter->aStaInfo[staId].isUsed &&
           ( staId != (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId))
        {
            //Disconnect all the stations
            hdd_softap_sta_disassoc(pHostapdAdapter, &pHostapdAdapter->aStaInfo[staId].macAddrSTA.bytes[0]);
        }
    }
}

static int hdd_stop_p2p_link(hdd_adapter_t *pHostapdAdapter,v_PVOID_t usrDataForCallback)
{
    struct net_device *dev;
    hdd_context_t     *pHddCtx = NULL;
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    dev = (struct net_device *)usrDataForCallback;
    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags))
    {
#ifdef WLAN_FEATURE_MBSSID
        status = WLANSAP_StopBss(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
#else
        status = WLANSAP_StopBss((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext);
#endif
        if (VOS_IS_STATUS_SUCCESS(status))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, FL("Deleting P2P link!!!!!!"));
        }
        clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
        wlan_hdd_decr_active_session(pHddCtx, pHostapdAdapter->device_mode);
    }
    EXIT();
    return (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
}

#ifdef SAP_AUTH_OFFLOAD
void hdd_set_sap_auth_offload(hdd_adapter_t *pHostapdAdapter,
                                     bool enabled)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    struct tSirSapOffloadInfo *sap_offload_info = NULL;

    /* Prepare the request to send to SME */
    sap_offload_info = vos_mem_malloc(sizeof(*sap_offload_info));
    if (NULL == sap_offload_info) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  "%s: could not allocate tSirSapOffloadInfo!", __func__);
        return;
    }

    vos_mem_zero(sap_offload_info, sizeof(*sap_offload_info));

    sap_offload_info->vdev_id = pHostapdAdapter->sessionId;
    sap_offload_info->sap_auth_offload_enable =
        pHddCtx->cfg_ini->enable_sap_auth_offload && enabled;
    sap_offload_info->sap_auth_offload_sec_type =
        pHddCtx->cfg_ini->sap_auth_offload_sec_type;
    sap_offload_info->key_len =
        strlen(pHddCtx->cfg_ini->sap_auth_offload_key);

    if (sap_offload_info->sap_auth_offload_enable) {
        if (sap_offload_info->key_len < 8 ||
            sap_offload_info->key_len > WLAN_PSK_STRING_LENGTH) {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: invalid key length(%d) of WPA security!", __func__,
                   sap_offload_info->key_len);
            goto end;
        }
    }

    vos_mem_copy(sap_offload_info->key,
                            pHddCtx->cfg_ini->sap_auth_offload_key,
                            sap_offload_info->key_len);
    if (eHAL_STATUS_SUCCESS !=
        sme_set_sap_auth_offload(pHddCtx->hHal, sap_offload_info)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  "%s: sme_set_sap_auth_offload fail!", __func__);
        goto end;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
           "%s: sme_set_sap_auth_offload successfully!", __func__);

end:
    vos_mem_free(sap_offload_info);
    return;
}


/**
 * hdd_set_client_block_info - get client block info from ini file
 * @padapter: hdd adapter pointer
 *
 * This function reads client block related info from ini file, these
 * configurations will be sent to fw through wmi.
 *
 * Return: 0 on success, otherwise error value
 */
int hdd_set_client_block_info(hdd_adapter_t *padapter)
{
	hdd_context_t *phddctx = WLAN_HDD_GET_CTX(padapter);
	struct sblock_info client_block_info;
	eHalStatus status;

	/* prepare the request to send to SME */
	client_block_info.vdev_id = padapter->sessionId;
	client_block_info.reconnect_cnt =
				phddctx->cfg_ini->connect_fail_count;

	client_block_info.con_fail_duration =
				phddctx->cfg_ini->connect_fail_duration;

	client_block_info.block_duration =
				phddctx->cfg_ini->connect_block_duration;

	status = sme_set_client_block_info(phddctx->hHal, &client_block_info);
	if (eHAL_STATUS_FAILURE == status) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: sme_set_client_block_info!", __func__);
		return -EIO;
	}

	hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
		"%s: sme_set_client_block_info success!", __func__);

	return 0;
}
#endif /* SAP_AUTH_OFFLOAD */

VOS_STATUS hdd_hostapd_SAPEventCB( tpSap_Event pSapEvent, v_PVOID_t usrDataForCallback)
{
    hdd_adapter_t *pHostapdAdapter;
    hdd_ap_ctx_t *pHddApCtx;
    hdd_hostapd_state_t *pHostapdState;
    struct net_device *dev;
    eSapHddEvent sapEvent;
    union iwreq_data wrqu;
    v_BYTE_t *we_custom_event_generic = NULL;
    int we_event = 0;
    int i = 0;
    v_U8_t staId;
    VOS_STATUS vos_status;
    v_BOOL_t bWPSState;
    v_BOOL_t bAuthRequired = TRUE;
    tpSap_AssocMacAddr pAssocStasArray = NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
    char unknownSTAEvent[IW_CUSTOM_MAX+1];
    char maxAssocExceededEvent[IW_CUSTOM_MAX+1];
#else
    char maxAssocExceeded_unknownSTAEvent[IW_CUSTOM_MAX+1];  //combined to avoid not stack memory compiler error
#endif
    v_BYTE_t we_custom_start_event[64];
    char *startBssEvent;
    hdd_context_t *pHddCtx;
    hdd_scaninfo_t *pScanInfo  = NULL;
    struct iw_michaelmicfailure msg;
    v_U8_t ignoreCAC = 0;
    hdd_config_t *cfg = NULL;
    struct wlan_dfs_info dfs_info;
    v_U8_t cc_len = WLAN_SVC_COUNTRY_CODE_LEN;

#ifdef WLAN_FEATURE_MBSSID
    hdd_adapter_t *con_sap_adapter;
#endif
#ifdef MSM_PLATFORM
    unsigned long flags;
#endif
    VOS_STATUS status = VOS_STATUS_SUCCESS;
#if defined CONFIG_CNSS
    int ret = 0;
#endif

    dev = (struct net_device *)usrDataForCallback;
    if (!dev)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: usrDataForCallback is null", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pHostapdAdapter = netdev_priv(dev);

    if ((NULL == pHostapdAdapter) ||
        (WLAN_HDD_ADAPTER_MAGIC != pHostapdAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "invalid adapter or adapter has invalid magic");
        return eHAL_STATUS_FAILURE;
    }

    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
    pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);

    if (!pSapEvent)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: pSapEvent is null", __func__);
        return eHAL_STATUS_FAILURE;
    }

    sapEvent = pSapEvent->sapHddEventCode;
    memset(&wrqu, '\0', sizeof(wrqu));
    pHddCtx = (hdd_context_t*)(pHostapdAdapter->pHddCtx);

    if (!pHddCtx) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is null"));
        return eHAL_STATUS_FAILURE;
    }

    cfg = pHddCtx->cfg_ini;

    if (!cfg) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD config is null"));
        return eHAL_STATUS_FAILURE;
    }

    dfs_info.channel = pHddApCtx->operatingChannel;
    sme_GetCountryCode(pHddCtx->hHal, dfs_info.country_code, &cc_len);

    switch(sapEvent)
    {
        case eSAP_START_BSS_EVENT :
            hddLog(LOG1, FL("BSS configured status = %s, channel = %u, bc sta Id = %d"),
                            pSapEvent->sapevt.sapStartBssCompleteEvent.status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS",
                            pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel,
                              pSapEvent->sapevt.sapStartBssCompleteEvent.staId);

            pHostapdAdapter->sessionId =
                    pSapEvent->sapevt.sapStartBssCompleteEvent.sessionId;

            pHostapdState->vosStatus = pSapEvent->sapevt.sapStartBssCompleteEvent.status;
            vos_status = vos_event_set(&pHostapdState->vosEvent);

            if (!VOS_IS_STATUS_SUCCESS(vos_status) || pHostapdState->vosStatus)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: startbss event failed!!"));
                goto stopbss;
            }
            else
            {
#ifdef FEATURE_WLAN_CH_AVOID
                sme_ChAvoidUpdateReq(pHddCtx->hHal);
#endif /* FEATURE_WLAN_CH_AVOID */

                pHddApCtx->uBCStaId = pSapEvent->sapevt.sapStartBssCompleteEvent.staId;

#ifdef QCA_LL_TX_FLOW_CT
                if (pHostapdAdapter->tx_flow_timer_initialized == VOS_FALSE)
                {
                    vos_timer_init(&pHostapdAdapter->tx_flow_control_timer,
                                   VOS_TIMER_TYPE_SW,
                                   hdd_tx_resume_timer_expired_handler,
                                   pHostapdAdapter);
                    pHostapdAdapter->tx_flow_timer_initialized = VOS_TRUE;
                }
                WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext,
                                             hdd_tx_resume_cb,
                                             pHostapdAdapter->sessionId,
                                             (void *)pHostapdAdapter);
#endif

                //@@@ need wep logic here to set privacy bit
                vos_status = hdd_softap_Register_BC_STA(pHostapdAdapter, pHddApCtx->uPrivacy);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGW, FL("Failed to register BC STA %d"), vos_status);
            }
#ifdef IPA_OFFLOAD
            if (hdd_ipa_is_enabled(pHddCtx))
            {
                status = hdd_ipa_wlan_evt(pHostapdAdapter, pHddApCtx->uBCStaId,
                        WLAN_AP_CONNECT, pHostapdAdapter->dev->dev_addr);

                if (status)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                     ("ERROR: WLAN_AP_CONNECT event failed!!"));
                    goto stopbss;
                }
            }
#endif

            if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
            {
                // AP Inactivity timer init and start
                vos_status = vos_timer_init( &pHddApCtx->hdd_ap_inactivity_timer, VOS_TIMER_TYPE_SW,
                                            hdd_hostapd_inactivity_timer_cb, (v_PVOID_t)dev );
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to init AP inactivity timer"));

                vos_status = vos_timer_start( &pHddApCtx->hdd_ap_inactivity_timer, (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff * 1000);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to init AP inactivity timer"));

            }
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
            wlan_hdd_auto_shutdown_enable(pHddCtx, VOS_TRUE);
#endif
            pHddApCtx->operatingChannel = pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel;
            pHostapdState->bssState = BSS_START;

#ifdef FEATURE_GREEN_AP
            if (!(VOS_STA & pHddCtx->concurrency_mode) &&
                    cfg->enable2x2 &&
                    cfg->enableGreenAP) {
                hdd_wlan_green_ap_mc(pHddCtx, GREEN_AP_PS_START_EVENT);
            } else {
                hdd_wlan_green_ap_mc(pHddCtx, GREEN_AP_PS_STOP_EVENT);
                hddLog(VOS_TRACE_LEVEL_INFO,
                    "Green-AP: is disabled, due to sta_concurrency: %d, enable2x2: %d, enableGreenAP: %d",
                     VOS_STA & pHddCtx->concurrency_mode,
                     cfg->enable2x2,
                     cfg->enableGreenAP);
            }
#endif
            // Send current operating channel of SoftAP to BTC-ES
            send_btc_nlink_msg(WLAN_BTC_SOFTAP_BSS_START, 0);

            //Set group key / WEP key every time when BSS is restarted
            if( pHddApCtx->groupKey.keyLength )
            {
                 status = WLANSAP_SetKeySta(
#ifdef WLAN_FEATURE_MBSSID
                               WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
#else
                               (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
#endif
                               &pHddApCtx->groupKey);
                 if (!VOS_IS_STATUS_SUCCESS(status))
                 {
                      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "%s: WLANSAP_SetKeySta failed", __func__);
                 }
            }
            else
            {
                for ( i = 0; i < CSR_MAX_NUM_KEY; i++ )
                {
                    if ( !pHddApCtx->wepKey[i].keyLength )
                          continue;

                    status = WLANSAP_SetKeySta(
#ifdef WLAN_FEATURE_MBSSID
                                WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
#else
                                (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
#endif
                                &pHddApCtx->wepKey[i]);
                    if (!VOS_IS_STATUS_SUCCESS(status))
                    {
                          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "%s: WLANSAP_SetKeySta failed idx %d", __func__, i);
                    }
                }
           }

            pHddCtx->dfs_radar_found = VOS_FALSE;
            WLANSAP_Get_Dfs_Ignore_CAC(pHddCtx->hHal, &ignoreCAC);
            if ((NV_CHANNEL_DFS !=
                vos_nv_getChannelEnabledState(pHddApCtx->operatingChannel))
                || ignoreCAC)
            {
                pHddApCtx->dfs_cac_block_tx = VOS_FALSE;
            }

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_MED,
                      "The value of dfs_cac_block_tx[%d] for ApCtx[%p]",
                      pHddApCtx->dfs_cac_block_tx, pHddApCtx);

            if ((NV_CHANNEL_DFS ==
                vos_nv_getChannelEnabledState(pHddApCtx->operatingChannel)) &&
                (pHddCtx->cfg_ini->IsSapDfsChSifsBurstEnabled == 0))
            {

                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "%s: Setting SIFS Burst disable for DFS channel %d",
                            __func__, pHddApCtx->operatingChannel);

                if (process_wma_set_command((int)pHostapdAdapter->sessionId,
                                            (int)WMI_PDEV_PARAM_BURST_ENABLE,
                                             0, PDEV_CMD))
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: Failed to Set SIFS Burst for DFS channel %d",
                                __func__, pHddApCtx->operatingChannel);
                }
            }

            //Fill the params for sending IWEVCUSTOM Event with SOFTAP.enabled
            startBssEvent = "SOFTAP.enabled";
            memset(&we_custom_start_event, '\0', sizeof(we_custom_start_event));
            memcpy(&we_custom_start_event, startBssEvent, strlen(startBssEvent));
            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = strlen(startBssEvent);
            we_event = IWEVCUSTOM;
            we_custom_event_generic = we_custom_start_event;
            hdd_dump_concurrency_info(pHddCtx);
            break; //Event will be sent after Switch-Case stmt

        case eSAP_STOP_BSS_EVENT:
            hddLog(LOG1, FL("BSS stop status = %s"),pSapEvent->sapevt.sapStopBssCompleteEvent.status ?
                             "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

            hdd_set_sap_auth_offload(pHostapdAdapter, FALSE);

#ifdef FEATURE_GREEN_AP
            hdd_wlan_green_ap_mc(pHddCtx, GREEN_AP_PS_STOP_EVENT);
#endif
            //Free up Channel List incase if it is set
#ifdef WLAN_FEATURE_MBSSID
            sapCleanupChannelList(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
#else
            sapCleanupChannelList();
#endif

            pHddApCtx->operatingChannel = 0; //Invalidate the channel info.
#ifdef IPA_OFFLOAD
            if (hdd_ipa_is_enabled(pHddCtx))
            {
                status = hdd_ipa_wlan_evt(pHostapdAdapter, pHddApCtx->uBCStaId,
                        WLAN_AP_DISCONNECT, pHostapdAdapter->dev->dev_addr);

                if (status)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                  ("ERROR: WLAN_AP_DISCONNECT event failed!!"));
                    goto stopbss;
                }
            }
#endif
#ifdef WLAN_FEATURE_MBSSID
            con_sap_adapter = hdd_get_con_sap_adapter(pHostapdAdapter);
            if (con_sap_adapter) {
                if (!VOS_IS_DFS_CH(
                                con_sap_adapter->sessionCtx.ap.operatingChannel))
                    pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
            }
#endif
            goto stopbss;

        case eSAP_DFS_CAC_START:
            wlan_hdd_send_svc_nlink_msg(WLAN_SVC_DFS_CAC_START_IND,
                                      &dfs_info, sizeof(struct wlan_dfs_info));
#ifdef WLAN_FEATURE_MBSSID
            pHddCtx->dev_dfs_cac_status = DFS_CAC_IN_PROGRESS;
#endif
            break;

        case eSAP_DFS_CAC_END:
            wlan_hdd_send_svc_nlink_msg(WLAN_SVC_DFS_CAC_END_IND,
                                      &dfs_info, sizeof(struct wlan_dfs_info));
            pHddApCtx->dfs_cac_block_tx = VOS_FALSE;
#ifdef WLAN_FEATURE_MBSSID
            pHddCtx->dev_dfs_cac_status = DFS_CAC_ALREADY_DONE;
#endif
            break;

        case eSAP_DFS_RADAR_DETECT:
            wlan_hdd_send_svc_nlink_msg(WLAN_SVC_DFS_RADAR_DETECT_IND,
                                      &dfs_info, sizeof(struct wlan_dfs_info));
#ifdef WLAN_FEATURE_MBSSID
            pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
#endif
            break;

        case eSAP_STA_SET_KEY_EVENT:
            //TODO: forward the message to hostapd once implementtation is done for now just print
            hddLog(LOG1, FL("SET Key: configured status = %s"),pSapEvent->sapevt.sapStationSetKeyCompleteEvent.status ?
                            "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");
            return VOS_STATUS_SUCCESS;
        case eSAP_STA_DEL_KEY_EVENT:
           //TODO: forward the message to hostapd once implementtation is done for now just print
           hddLog(LOG1, FL("Event received %s"),"eSAP_STA_DEL_KEY_EVENT");
           return VOS_STATUS_SUCCESS;
        case eSAP_STA_MIC_FAILURE_EVENT:
        {
            memset(&msg, '\0', sizeof(msg));
            msg.src_addr.sa_family = ARPHRD_ETHER;
            memcpy(msg.src_addr.sa_data, &pSapEvent->sapevt.sapStationMICFailureEvent.staMac, sizeof(v_MACADDR_t));
            hddLog(LOG1, "MIC MAC "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(msg.src_addr.sa_data));
            if(pSapEvent->sapevt.sapStationMICFailureEvent.multicast == eSAP_TRUE)
             msg.flags = IW_MICFAILURE_GROUP;
            else
             msg.flags = IW_MICFAILURE_PAIRWISE;
            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = sizeof(msg);
            we_event = IWEVMICHAELMICFAILURE;
            we_custom_event_generic = (v_BYTE_t *)&msg;
        }
      /* inform mic failure to nl80211 */
        cfg80211_michael_mic_failure(dev,
                                     pSapEvent->sapevt.
                                     sapStationMICFailureEvent.staMac.bytes,
                                     ((pSapEvent->sapevt.sapStationMICFailureEvent.multicast == eSAP_TRUE) ?
                                      NL80211_KEYTYPE_GROUP :
                                      NL80211_KEYTYPE_PAIRWISE),
                                     pSapEvent->sapevt.sapStationMICFailureEvent.keyId,
                                     pSapEvent->sapevt.sapStationMICFailureEvent.TSC,
                                     GFP_KERNEL);
            break;

        case eSAP_STA_ASSOC_EVENT:
        case eSAP_STA_REASSOC_EVENT:
            wrqu.addr.sa_family = ARPHRD_ETHER;
            memcpy(wrqu.addr.sa_data, &pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac,
                sizeof(v_MACADDR_t));
            hddLog(LOG1, " associated "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            we_event = IWEVREGISTERED;

#ifdef WLAN_FEATURE_MBSSID
            WLANSAP_Get_WPS_State(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), &bWPSState);
#else
            WLANSAP_Get_WPS_State((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext, &bWPSState);
#endif

            if ( (eCSR_ENCRYPT_TYPE_NONE == pHddApCtx->ucEncryptType) ||
                 ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == pHddApCtx->ucEncryptType ) ||
                 ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == pHddApCtx->ucEncryptType ) )
            {
                bAuthRequired = FALSE;
            }

            if (bAuthRequired || bWPSState == eANI_BOOLEAN_TRUE )
            {
                vos_status = hdd_softap_RegisterSTA(pHostapdAdapter,
                                                    TRUE,
                                                    pHddApCtx->uPrivacy,
                                                    pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                                                    0,
                                                    0,
                                                    (v_MACADDR_t *)wrqu.addr.sa_data,
                                                    pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.wmmEnabled);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                     hddLog(LOGW, FL("Failed to register STA %d "MAC_ADDRESS_STR""),
                                     vos_status, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            }
            else
            {
                vos_status = hdd_softap_RegisterSTA(pHostapdAdapter,
                                                    FALSE,
                                                    pHddApCtx->uPrivacy,
                                                    pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                                                    0,
                                                    0,
                                                    (v_MACADDR_t *)wrqu.addr.sa_data,
                                                    pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.wmmEnabled);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGW, FL("Failed to register STA %d "MAC_ADDRESS_STR""),
                           vos_status, MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            }
#ifdef IPA_OFFLOAD
            if (hdd_ipa_is_enabled(pHddCtx))
            {
                status = hdd_ipa_wlan_evt(pHostapdAdapter,
                   pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                   WLAN_CLIENT_CONNECT_EX,
                   pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac.bytes);
                if (status)
                {
                   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: WLAN_CLIENT_CONNECT_EX event failed!!"));
                   goto stopbss;
                }
            }
#endif
#ifdef QCA_PKT_PROTO_TRACE
            /* Peer associated, update into trace buffer */
            if (pHddCtx->cfg_ini->gEnableDebugLog)
            {
               vos_pkt_trace_buf_update("HA:ASSOC");
            }
#endif /* QCA_PKT_PROTO_TRACE */

#ifdef MSM_PLATFORM
            /* start timer in sap/p2p_go */
            if (pHddApCtx->bApActive == VOS_FALSE)
            {
                spin_lock_irqsave(&pHddCtx->bus_bw_lock, flags);
                pHostapdAdapter->prev_tx_packets = pHostapdAdapter->stats.tx_packets;
                pHostapdAdapter->prev_rx_packets = pHostapdAdapter->stats.rx_packets;
                hdd_start_bus_bw_compute_timer(pHostapdAdapter);
                spin_unlock_irqrestore(&pHddCtx->bus_bw_lock, flags);
            }
#endif
            pHddApCtx->bApActive = VOS_TRUE;
            // Stop AP inactivity timer
            if (pHddApCtx->hdd_ap_inactivity_timer.state == VOS_TIMER_STATE_RUNNING)
            {
                vos_status = vos_timer_stop(&pHddApCtx->hdd_ap_inactivity_timer);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to start AP inactivity timer"));
            }
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
            wlan_hdd_auto_shutdown_enable(pHddCtx, VOS_FALSE);
#endif
            vos_wake_lock_timeout_acquire(&pHddCtx->sap_wake_lock,
                                          HDD_SAP_WAKE_LOCK_DURATION);
            {
               struct station_info staInfo;
               v_U16_t iesLen =  pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.iesLen;

               memset(&staInfo, 0, sizeof(staInfo));
               if (iesLen <= MAX_ASSOC_IND_IE_LEN )
               {
                  staInfo.assoc_req_ies =
                     (const u8 *)&pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.ies[0];
                  staInfo.assoc_req_ies_len = iesLen;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,31)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
                  staInfo.filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
                  cfg80211_new_sta(dev,
                        (const u8 *)&pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac.bytes[0],
                        &staInfo, GFP_KERNEL);
               }
               else
               {
                  hddLog(LOGE, FL(" Assoc Ie length is too long"));
               }
            }

            pScanInfo =  &pHostapdAdapter->scan_info;
            // Lets do abort scan to ensure smooth authentication for client
            if ((pScanInfo != NULL) && pScanInfo->mScanPending)
            {
                hdd_abort_mac_scan(pHddCtx, pHostapdAdapter->sessionId,
                                   eCSR_SCAN_ABORT_DEFAULT);
            }
#ifdef QCA_WIFI_2_0
            if (pHostapdAdapter->device_mode == WLAN_HDD_P2P_GO)
            {
                /* send peer status indication to oem app */
                hdd_SendPeerStatusIndToOemApp(
                  &pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac,
                  ePeerConnected,
                  pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.timingMeasCap,
                  pHostapdAdapter->sessionId, pHddApCtx->operatingChannel);
            }
#endif

#ifdef FEATURE_GREEN_AP
            hdd_wlan_green_ap_mc(pHddCtx, GREEN_AP_ADD_STA_EVENT);
#endif
            break;
        case eSAP_STA_DISASSOC_EVENT:
            memcpy(wrqu.addr.sa_data, &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac,
                   sizeof(v_MACADDR_t));
            hddLog(LOG1, " disassociated "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(wrqu.addr.sa_data));

            vos_status = vos_event_set(&pHostapdState->vosEvent);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "ERROR: Station deauth event reporting failed!!");

            if (pSapEvent->sapevt.sapStationDisassocCompleteEvent.reason == eSAP_USR_INITATED_DISASSOC)
                hddLog(LOG1," User initiated disassociation");
            else
                hddLog(LOG1," MAC initiated disassociation");
            we_event = IWEVEXPIRED;
            vos_status = hdd_softap_GetStaId(pHostapdAdapter, &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac, &staId);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, FL("ERROR: HDD Failed to find sta id!!"));
                return VOS_STATUS_E_FAILURE;
            }
#ifdef IPA_OFFLOAD
            if (hdd_ipa_is_enabled(pHddCtx))
            {
                status = hdd_ipa_wlan_evt(pHostapdAdapter, staId, WLAN_CLIENT_DISCONNECT,
                pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac.bytes);

                if (status)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        ("ERROR: WLAN_CLIENT_DISCONNECT event failed!!"));
                    goto stopbss;
                }
            }
#endif
#ifdef QCA_PKT_PROTO_TRACE
            /* Peer dis-associated, update into trace buffer */
            if (pHddCtx->cfg_ini->gEnableDebugLog)
            {
               vos_pkt_trace_buf_update("HA:DISASC");
            }
#endif /* QCA_PKT_PROTO_TRACE */
            hdd_softap_DeregisterSTA(pHostapdAdapter, staId);

            pHddApCtx->bApActive = VOS_FALSE;
            spin_lock_bh( &pHostapdAdapter->staInfo_lock );
            for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
            {
                if (pHostapdAdapter->aStaInfo[i].isUsed && i != (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId)
                {
                    pHddApCtx->bApActive = VOS_TRUE;
                    break;
                }
            }
            spin_unlock_bh( &pHostapdAdapter->staInfo_lock );

            // Start AP inactivity timer if no stations associated with it
            if ((0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff))
            {
                if (pHddApCtx->bApActive == FALSE)
                {
                    if (pHddApCtx->hdd_ap_inactivity_timer.state == VOS_TIMER_STATE_STOPPED)
                    {
                        vos_status = vos_timer_start(&pHddApCtx->hdd_ap_inactivity_timer, (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff * 1000);
                        if (!VOS_IS_STATUS_SUCCESS(vos_status))
                            hddLog(LOGE, FL("Failed to init AP inactivity timer"));
                    }
                    else
                        VOS_ASSERT(vos_timer_getCurrentState(&pHddApCtx->hdd_ap_inactivity_timer) == VOS_TIMER_STATE_STOPPED);
                }
            }
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
            wlan_hdd_auto_shutdown_enable(pHddCtx, VOS_TRUE);
#endif

            if (pSapEvent->sapevt.sapStationDisassocCompleteEvent.statusCode ==
                             eSIR_SME_SAP_AUTH_OFFLOAD_PEER_UPDATE_STATUS) {
                /** eSIR_SME_SAP_AUTH_OFFLOAD_PEER_UPDATE_STATUS indicates:
                 * The existing sta connection needs to be updated instead
                 * of clean up the sta. This condition could only happens
                 * when Host SAP sleep with WOW and SAP Auth offload enabled.
                 */

                hddLog(LOG1,"SAP peer update sta:Id=%d, Mac="MAC_ADDRESS_STR,
                    pSapEvent->sapevt.sapStationDisassocCompleteEvent.staId,
                    MAC_ADDR_ARRAY(pSapEvent->sapevt.
                    sapStationDisassocCompleteEvent.staMac.bytes));
            } else {
                hddLog(LOG1,"SAP del sta: staId=%d, staMac="MAC_ADDRESS_STR,
                    pSapEvent->sapevt.sapStationDisassocCompleteEvent.staId,
                    MAC_ADDR_ARRAY(pSapEvent->sapevt.
                    sapStationDisassocCompleteEvent.staMac.bytes));

                cfg80211_del_sta(dev,
                    (const u8 *)&pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac.bytes[0],
                    GFP_KERNEL);
            }

            //Update the beacon Interval if it is P2P GO
            vos_status = hdd_change_mcc_go_beacon_interval(pHostapdAdapter);
            if (VOS_STATUS_SUCCESS != vos_status)
            {
                hddLog(LOGE, "%s: failed to update Beacon interval %d",
                       __func__, vos_status);
            }
#ifdef QCA_WIFI_2_0
            if (pHostapdAdapter->device_mode == WLAN_HDD_P2P_GO)
            {
                /* send peer status indication to oem app */
                hdd_SendPeerStatusIndToOemApp(
                  &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac,
                  ePeerDisconnected, 0,
                  pHostapdAdapter->sessionId, pHddApCtx->operatingChannel);
            }
#endif

#ifdef MSM_PLATFORM
            /*stop timer in sap/p2p_go */
            if (pHddApCtx->bApActive == FALSE)
            {
                spin_lock_irqsave(&pHddCtx->bus_bw_lock, flags);
                pHostapdAdapter->prev_tx_packets = 0;
                pHostapdAdapter->prev_rx_packets = 0;
                hdd_stop_bus_bw_compute_timer(pHostapdAdapter);
                spin_unlock_irqrestore(&pHddCtx->bus_bw_lock, flags);
            }
#endif
#ifdef FEATURE_GREEN_AP
            hdd_wlan_green_ap_mc(pHddCtx, GREEN_AP_DEL_STA_EVENT);
#endif
            break;
        case eSAP_WPS_PBC_PROBE_REQ_EVENT:
        {
                static const char * message ="MLMEWPSPBCPROBEREQ.indication";
                union iwreq_data wreq;

                down(&pHddApCtx->semWpsPBCOverlapInd);
                pHddApCtx->WPSPBCProbeReq.probeReqIELen = pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.probeReqIELen;

                vos_mem_copy(pHddApCtx->WPSPBCProbeReq.probeReqIE, pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.probeReqIE,
                    pHddApCtx->WPSPBCProbeReq.probeReqIELen);

                vos_mem_copy(pHddApCtx->WPSPBCProbeReq.peerMacAddr, pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.peerMacAddr, sizeof(v_MACADDR_t));
                hddLog(LOG1, "WPS PBC probe req "MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pHddApCtx->WPSPBCProbeReq.peerMacAddr));
                memset(&wreq, 0, sizeof(wreq));
                wreq.data.length = strlen(message); // This is length of message
                wireless_send_event(dev, IWEVCUSTOM, &wreq, (char *)message);

                return VOS_STATUS_SUCCESS;
        }
        case eSAP_ASSOC_STA_CALLBACK_EVENT:
            pAssocStasArray = pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas;
            if (pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta != 0)
            {   // List of associated stations
                for (i = 0; i < pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta; i++)
                {
                    hddLog(LOG1,"Associated Sta Num %d:assocId=%d, staId=%d, staMac="MAC_ADDRESS_STR,
                        i+1,
                        pAssocStasArray->assocId,
                        pAssocStasArray->staId,
                                    MAC_ADDR_ARRAY(pAssocStasArray->staMac.bytes));
                        pAssocStasArray++;
            }
            }
            vos_mem_free(pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas);// Release caller allocated memory here
            pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas = NULL;
            return VOS_STATUS_SUCCESS;
        case eSAP_INDICATE_MGMT_FRAME:
           hdd_indicateMgmtFrame( pHostapdAdapter,
                                 pSapEvent->sapevt.sapManagementFrameInfo.nFrameLength,
                                 pSapEvent->sapevt.sapManagementFrameInfo.pbFrames,
                                 pSapEvent->sapevt.sapManagementFrameInfo.frameType,
                                 pSapEvent->sapevt.sapManagementFrameInfo.rxChan, 0);
           return VOS_STATUS_SUCCESS;
        case eSAP_REMAIN_CHAN_READY:
           hdd_remainChanReadyHandler( pHostapdAdapter );
           return VOS_STATUS_SUCCESS;
        case eSAP_SEND_ACTION_CNF:
           hdd_sendActionCnf( pHostapdAdapter,
                              ( eSAP_STATUS_SUCCESS ==
                                pSapEvent->sapevt.sapActionCnf.actionSendSuccess ) ?
                                TRUE : FALSE );
           return VOS_STATUS_SUCCESS;
        case eSAP_UNKNOWN_STA_JOIN:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
            snprintf(unknownSTAEvent, IW_CUSTOM_MAX, "JOIN_UNKNOWN_STA-%02x:%02x:%02x:%02x:%02x:%02x",
#else
            snprintf(maxAssocExceeded_unknownSTAEvent, IW_CUSTOM_MAX, "JOIN_UNKNOWN_STA-%02x:%02x:%02x:%02x:%02x:%02x",
#endif
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[0],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[1],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[2],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[3],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[4],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[5]);
            we_event = IWEVCUSTOM; /* Discovered a new node (AP mode). */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
            wrqu.data.pointer = unknownSTAEvent;
            wrqu.data.length = strlen(unknownSTAEvent);
            we_custom_event_generic = (v_BYTE_t *)unknownSTAEvent;
            hddLog(LOGE,"%s", unknownSTAEvent);
#else
            wrqu.data.pointer = maxAssocExceeded_unknownSTAEvent;
            wrqu.data.length = strlen(maxAssocExceeded_unknownSTAEvent);
            we_custom_event_generic = (v_BYTE_t *)maxAssocExceeded_unknownSTAEvent;//unknownSTAEvent;
            hddLog(LOGE,"%s", maxAssocExceeded_unknownSTAEvent);
#endif
            break;

        case eSAP_MAX_ASSOC_EXCEEDED:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
            snprintf(maxAssocExceededEvent, IW_CUSTOM_MAX, "Peer %02x:%02x:%02x:%02x:%02x:%02x denied"
#else
            snprintf(maxAssocExceeded_unknownSTAEvent, IW_CUSTOM_MAX, "Peer %02x:%02x:%02x:%02x:%02x:%02x denied"
#endif
                    " assoc due to Maximum Mobile Hotspot connections reached. Please disconnect"
                    " one or more devices to enable the new device connection",
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[0],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[1],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[2],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[3],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[4],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[5]);
            we_event = IWEVCUSTOM; /* Discovered a new node (AP mode). */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
            wrqu.data.pointer = maxAssocExceededEvent;
            wrqu.data.length = strlen(maxAssocExceededEvent);
            we_custom_event_generic = (v_BYTE_t *)maxAssocExceededEvent;
            hddLog(LOG1,"%s", maxAssocExceededEvent);
#else
            wrqu.data.pointer = maxAssocExceeded_unknownSTAEvent;
            wrqu.data.length = strlen(maxAssocExceeded_unknownSTAEvent);
            we_custom_event_generic = (v_BYTE_t *)maxAssocExceeded_unknownSTAEvent;
            hddLog(LOG1,"%s", maxAssocExceeded_unknownSTAEvent);
#endif
            break;
        case eSAP_STA_ASSOC_IND:
            return VOS_STATUS_SUCCESS;

        case eSAP_DISCONNECT_ALL_P2P_CLIENT:
            hddLog(LOG1, FL(" Disconnecting all the P2P Clients...."));
            hdd_clear_all_sta(pHostapdAdapter, usrDataForCallback);
            return VOS_STATUS_SUCCESS;

        case eSAP_MAC_TRIG_STOP_BSS_EVENT :
            vos_status = hdd_stop_p2p_link(pHostapdAdapter, usrDataForCallback);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                hddLog(LOGW, FL("hdd_stop_p2p_link failed %d"), vos_status);
            }
            return VOS_STATUS_SUCCESS;

        case eSAP_CHANNEL_CHANGE_EVENT:
            hddLog(LOG1, FL("Received eSAP_CHANNEL_CHANGE_EVENT event"));
            /* TODO Need to indicate operating channel change to hostapd */
            return VOS_STATUS_SUCCESS;

        case eSAP_DFS_NOL_GET:
            hddLog(VOS_TRACE_LEVEL_INFO,
                    FL("Received eSAP_DFS_NOL_GET event"));
#if defined CONFIG_CNSS
            /* get the dfs nol from cnss */
            ret = cnss_wlan_get_dfs_nol(
                      pSapEvent->sapevt.sapDfsNolInfo.pDfsList,
                      pSapEvent->sapevt.sapDfsNolInfo.sDfsList);

            if (ret > 0) {
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                        "%s: Get %d bytes of dfs nol from cnss",
                        __func__, ret);
                return VOS_STATUS_SUCCESS;
            } else {
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                        "%s: No dfs nol entry in CNSS, ret: %d",
                        __func__, ret);
                return VOS_STATUS_E_FAULT;
            }
#endif
        case eSAP_DFS_NOL_SET:
            hddLog(VOS_TRACE_LEVEL_INFO, FL("Received eSAP_DFS_NOL_SET event"));
#if defined CONFIG_CNSS
            /* set the dfs nol to cnss */
            ret = cnss_wlan_set_dfs_nol(
                    pSapEvent->sapevt.sapDfsNolInfo.pDfsList,
                    pSapEvent->sapevt.sapDfsNolInfo.sDfsList);

            if (ret) {
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                        "%s: Failed to set dfs nol - ret: %d",
                        __func__, ret);
            } else {
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                        "%s: Set %d bytes dfs nol to cnss",
                        __func__,
                        pSapEvent->sapevt.sapDfsNolInfo.sDfsList);
            }
#endif
            return VOS_STATUS_SUCCESS;

        default:
            hddLog(LOG1,"SAP message is not handled");
            goto stopbss;
            return VOS_STATUS_SUCCESS;
    }
    wireless_send_event(dev, we_event, &wrqu, (char *)we_custom_event_generic);
    return VOS_STATUS_SUCCESS;

stopbss :
    {
        v_BYTE_t we_custom_event[64];
        char *stopBssEvent = "STOP-BSS.response";//17
        int event_len = strlen(stopBssEvent);

        hddLog(LOG1, FL("BSS stop status = %s"),
               pSapEvent->sapevt.sapStopBssCompleteEvent.status ?
                            "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

        /* Change the BSS state now since, as we are shutting things down,
         * we don't want interfaces to become re-enabled */
        pHostapdState->bssState = BSS_STOP;

        if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
        {
            if (VOS_TIMER_STATE_RUNNING == pHddApCtx->hdd_ap_inactivity_timer.state)
            {
                vos_status = vos_timer_stop(&pHddApCtx->hdd_ap_inactivity_timer);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGE, FL("Failed to stop AP inactivity timer"));
            }

            vos_status = vos_timer_destroy(&pHddApCtx->hdd_ap_inactivity_timer);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
                hddLog(LOGE, FL("Failed to Destroy AP inactivity timer"));
        }
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
        wlan_hdd_auto_shutdown_enable(pHddCtx, VOS_TRUE);
#endif

        /* Stop the pkts from n/w stack as we are going to free all of
         * the TX WMM queues for all STAID's */
        hdd_hostapd_stop(dev);

        /* reclaim all resources allocated to the BSS */
        vos_status = hdd_softap_stop_bss(pHostapdAdapter);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
            hddLog(LOGW, FL("hdd_softap_stop_bss failed %d"), vos_status);

        /* once the event is set, structure dev/pHostapdAdapter should
         * not be touched since they are now subject to being deleted
         * by another thread */
        if (eSAP_STOP_BSS_EVENT == sapEvent)
            vos_event_set(&pHostapdState->stop_bss_event);

        /* notify userspace that the BSS has stopped */
        memset(&we_custom_event, '\0', sizeof(we_custom_event));
        memcpy(&we_custom_event, stopBssEvent, event_len);
        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = event_len;
        we_event = IWEVCUSTOM;
        we_custom_event_generic = we_custom_event;
        wireless_send_event(dev, we_event, &wrqu, (char *)we_custom_event_generic);
        hdd_dump_concurrency_info(pHddCtx);
    }
    return VOS_STATUS_SUCCESS;
}

int hdd_softap_unpackIE(
                tHalHandle halHandle,
                eCsrEncryptionType *pEncryptType,
                eCsrEncryptionType *mcEncryptType,
                eCsrAuthType *pAuthType,
                v_BOOL_t *pMFPCapable,
                v_BOOL_t *pMFPRequired,
                u_int16_t gen_ie_len,
                u_int8_t *gen_ie )
{
    tDot11fIERSN dot11RSNIE;
    tDot11fIEWPA dot11WPAIE;

    tANI_U8 *pRsnIe;
    tANI_U16 RSNIeLen;

    if (NULL == halHandle)
    {
        hddLog(LOGE, FL("Error haHandle returned NULL"));
        return -EINVAL;
    }

    // Validity checks
    if ((gen_ie_len < VOS_MIN(DOT11F_IE_RSN_MIN_LEN, DOT11F_IE_WPA_MIN_LEN)) ||
        (gen_ie_len > VOS_MAX(DOT11F_IE_RSN_MAX_LEN, DOT11F_IE_WPA_MAX_LEN)) )
        return -EINVAL;
    // Type check
    if ( gen_ie[0] ==  DOT11F_EID_RSN)
    {
        // Validity checks
        if ((gen_ie_len < DOT11F_IE_RSN_MIN_LEN ) ||
            (gen_ie_len > DOT11F_IE_RSN_MAX_LEN) )
        {
            return VOS_STATUS_E_FAILURE;
        }
        // Skip past the EID byte and length byte
        pRsnIe = gen_ie + 2;
        RSNIeLen = gen_ie_len - 2;
        // Unpack the RSN IE
        memset(&dot11RSNIE, 0, sizeof(tDot11fIERSN));
        dot11fUnpackIeRSN((tpAniSirGlobal) halHandle,
                            pRsnIe,
                            RSNIeLen,
                            &dot11RSNIE);
        // Copy out the encryption and authentication types
        hddLog(LOG1, FL("%s: pairwise cipher suite count: %d"),
                __func__, dot11RSNIE.pwise_cipher_suite_count );
        hddLog(LOG1, FL("%s: authentication suite count: %d"),
                __func__, dot11RSNIE.akm_suite_count);
        /*Here we have followed the apple base code,
          but probably I suspect we can do something different*/
        //dot11RSNIE.akm_suite_count
        // Just translate the FIRST one
        *pAuthType =  hdd_TranslateRSNToCsrAuthType(dot11RSNIE.akm_suites[0]);
        //dot11RSNIE.pwise_cipher_suite_count
        *pEncryptType = hdd_TranslateRSNToCsrEncryptionType(dot11RSNIE.pwise_cipher_suites[0]);
        //dot11RSNIE.gp_cipher_suite_count
        *mcEncryptType = hdd_TranslateRSNToCsrEncryptionType(dot11RSNIE.gp_cipher_suite);
        // Set the PMKSA ID Cache for this interface
        *pMFPCapable = 0 != (dot11RSNIE.RSN_Cap[0] & 0x80);
        *pMFPRequired = 0 != (dot11RSNIE.RSN_Cap[0] & 0x40);
        // Calling csrRoamSetPMKIDCache to configure the PMKIDs into the cache
    } else
    if (gen_ie[0] == DOT11F_EID_WPA)
    {
        // Validity checks
        if ((gen_ie_len < DOT11F_IE_WPA_MIN_LEN ) ||
            (gen_ie_len > DOT11F_IE_WPA_MAX_LEN))
        {
            return VOS_STATUS_E_FAILURE;
        }
        // Skip past the EID byte and length byte - and four byte WiFi OUI
        pRsnIe = gen_ie + 2 + 4;
        RSNIeLen = gen_ie_len - (2 + 4);
        // Unpack the WPA IE
        memset(&dot11WPAIE, 0, sizeof(tDot11fIEWPA));
        dot11fUnpackIeWPA((tpAniSirGlobal) halHandle,
                            pRsnIe,
                            RSNIeLen,
                            &dot11WPAIE);
        // Copy out the encryption and authentication types
        hddLog(LOG1, FL("%s: WPA unicast cipher suite count: %d"),
                __func__, dot11WPAIE.unicast_cipher_count );
        hddLog(LOG1, FL("%s: WPA authentication suite count: %d"),
                __func__, dot11WPAIE.auth_suite_count);
        //dot11WPAIE.auth_suite_count
        // Just translate the FIRST one
        *pAuthType =  hdd_TranslateWPAToCsrAuthType(dot11WPAIE.auth_suites[0]);
        //dot11WPAIE.unicast_cipher_count
        *pEncryptType = hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.unicast_ciphers[0]);
        //dot11WPAIE.unicast_cipher_count
        *mcEncryptType = hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.multicast_cipher);
        *pMFPCapable = VOS_FALSE;
        *pMFPRequired = VOS_FALSE;
    }
    else
    {
        hddLog(LOGW, FL("%s: gen_ie[0]: %d"), __func__, gen_ie[0]);
        return VOS_STATUS_E_FAILURE;
    }
    return VOS_STATUS_SUCCESS;
}

 /**---------------------------------------------------------------------------

  \brief hdd_softap_set_channel_change() -
   This function to support SAP channel change with CSA IE
   set in the beacons.

  \param  - dev - Pointer to the net device.
          - target_channel - target channel number.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static
int hdd_softap_set_channel_change(struct net_device *dev, int target_channel)
{
    VOS_STATUS status;
    int ret = 0;
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hdd_context_t *pHddCtx = NULL;

#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif

    pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (ret)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: invalid HDD context", __func__);
        ret = -EBUSY;
        return ret;
    }

    /*
     * Set the dfs_radar_found flag to mimic channel change
     * when a radar is found. This will enable synchronizing
     * SAP and HDD states similar to that of radar indication.
     * Suspend the netif queues to stop queuing Tx frames
     * from upper layers.  netif queues will be resumed
     * once the channel change is completed and SAP will
     * post eSAP_START_BSS_EVENT success event to HDD.
     */
    pHddCtx->dfs_radar_found = VOS_TRUE;

    /*
     * Post the Channel Change request to SAP.
     */
    status = WLANSAP_SetChannelChangeWithCsa(
#ifdef WLAN_FEATURE_MBSSID
                WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
#else
                pVosContext,
#endif
                (v_U32_t) target_channel);

    if (VOS_STATUS_SUCCESS != status)
    {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: SAP set channel failed for channel = %d",
                 __func__, target_channel);
        /*
         * If channel change command fails then clear the
         * radar found flag and also restart the netif
         * queues.
         */

        pHddCtx->dfs_radar_found = VOS_FALSE;

        ret = -EINVAL;
    }

    return ret;
}

int
static iw_softap_set_ini_cfg(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    VOS_STATUS vstatus;
    int ret = 0; /* success */
    hdd_adapter_t *pAdapter = (netdev_priv(dev));
    hdd_context_t *pHddCtx;

    if (pAdapter == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                        "%s: pAdapter is NULL!", __func__);
        return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (ret != 0)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return ret;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received data %s", __func__, extra);

    vstatus = hdd_execute_global_config_command(pHddCtx, extra);
#ifdef WLAN_FEATURE_MBSSID
    if (vstatus == VOS_STATUS_E_PERM) {
        vstatus = hdd_execute_sap_dyn_config_command(pAdapter, extra);
        if (vstatus == VOS_STATUS_SUCCESS)
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Stored in Dynamic SAP ini config", __func__);
    }
#endif
    if (VOS_STATUS_SUCCESS != vstatus)
    {
        ret = -EINVAL;
    }

    return ret;
}

int
static iw_softap_get_ini_cfg(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    int ret = 0;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (ret != 0)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return ret;
    }

#ifdef WLAN_FEATURE_MBSSID
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Printing Adapter MBSSID SAP Dyn INI Config", __func__);
    hdd_cfg_get_sap_dyn_config(pAdapter, extra, QCSAP_IOCTL_MAX_STR_LEN);
    /* Overwrite extra buffer with global ini config if need to return in buf */
#endif
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Printing CLD global INI Config", __func__);
    hdd_cfg_get_global_config(pHddCtx, extra, QCSAP_IOCTL_MAX_STR_LEN);
    wrqu->data.length = strlen(extra)+1;

    return 0;
}

int
static iw_softap_wowl_config_pattern(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    int sub_cmd;
    int ret = 0; /* success */
    char *pBuffer = NULL;
    hdd_adapter_t *pAdapter = (netdev_priv(dev));
    struct iw_point s_priv_data;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    /* helper function to get iwreq_data with compat handling. */
    if (hdd_priv_get_data(&s_priv_data, wrqu)) {
        return -EINVAL;
    }

    /* make sure all params are correctly passed to function */
    if ((NULL == s_priv_data.pointer) || (0 == s_priv_data.length)) {
        return -EINVAL;
    }

    sub_cmd = s_priv_data.flags;

    /* ODD number is used for set, copy data using copy_from_user */
    pBuffer = mem_alloc_copy_from_user_helper(s_priv_data.pointer,
                                              s_priv_data.length);
    if (NULL == pBuffer)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "mem_alloc_copy_from_user_helper fail");
        return -ENOMEM;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received length %d", __func__, s_priv_data.length);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received data %s", __func__, pBuffer);

    switch(sub_cmd)
    {
    case WE_WOWL_ADD_PTRN:
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "ADD_PTRN");
        hdd_add_wowl_ptrn(pAdapter, pBuffer);
        break;
    case WE_WOWL_DEL_PTRN:
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "DEL_PTRN");
        hdd_del_wowl_ptrn(pAdapter, pBuffer);
        break;
    default:
        hddLog(LOGE, "%s: Invalid sub command %d", __func__, sub_cmd);
        ret = -EINVAL;
        break;
    }
    kfree(pBuffer);
    return ret;
}

static void print_mac_list(v_MACADDR_t *macList, v_U8_t size)
{
    int i;
    v_BYTE_t *macArray;

    for (i = 0; i < size; i++) {
        macArray = (macList + i)->bytes;
        pr_info("** ACL entry %i - %02x:%02x:%02x:%02x:%02x:%02x \n",
                                          i, MAC_ADDR_ARRAY(macArray));
    }
    return;
}

static VOS_STATUS hdd_print_acl(hdd_adapter_t *pHostapdAdapter)
{
    eSapMacAddrACL acl_mode;
    v_MACADDR_t MacList[MAX_ACL_MAC_ADDRESS];
    v_U8_t listnum;
    v_PVOID_t pvosGCtx = NULL;

#ifdef WLAN_FEATURE_MBSSID
    pvosGCtx = WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter);
#else
    pvosGCtx = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif
    vos_mem_zero(&MacList[0], sizeof(MacList));
    if (VOS_STATUS_SUCCESS == WLANSAP_GetACLMode(pvosGCtx, &acl_mode)) {
        pr_info("******** ACL MODE *********\n");
        switch (acl_mode) {
        case eSAP_ACCEPT_UNLESS_DENIED:
            pr_info("ACL Mode = ACCEPT_UNLESS_DENIED\n");
            break;
        case eSAP_DENY_UNLESS_ACCEPTED:
            pr_info("ACL Mode = DENY_UNLESS_ACCEPTED\n");
            break;
        case eSAP_SUPPORT_ACCEPT_AND_DENY:
            pr_info("ACL Mode = ACCEPT_AND_DENY\n");
            break;
        case eSAP_ALLOW_ALL:
            pr_info("ACL Mode = ALLOW_ALL\n");
            break;
        default:
            pr_info("Invalid SAP ACL Mode = %d\n", acl_mode);
            return VOS_STATUS_E_FAILURE;
        }
    } else {
        return VOS_STATUS_E_FAILURE;
    }

    if (VOS_STATUS_SUCCESS == WLANSAP_GetACLAcceptList(pvosGCtx,
                                                       &MacList[0], &listnum)) {
        pr_info("******* WHITE LIST ***********\n");
        if (listnum < MAX_ACL_MAC_ADDRESS)
            print_mac_list(&MacList[0], listnum);
    } else {
        return VOS_STATUS_E_FAILURE;
    }

    if (VOS_STATUS_SUCCESS == WLANSAP_GetACLDenyList(pvosGCtx,
                                                     &MacList[0], &listnum)) {
        pr_info("******* BLACK LIST ***********\n");
        if (listnum < MAX_ACL_MAC_ADDRESS)
            print_mac_list(&MacList[0], listnum);
    } else {
        return VOS_STATUS_E_FAILURE;
    }
    return VOS_STATUS_SUCCESS;
}

int
static iw_softap_setparam(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal;
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int set_value = value[1];
    eHalStatus status;
    int ret = 0; /* success */
    v_CONTEXT_t pVosContext;

    if (!pHostapdAdapter || !pHostapdAdapter->pHddCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: either hostapd Adapter is null or HDD ctx is null",
                 __func__);
       return -1;
    }

    hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    if (!hHal)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Hal ctx is null", __func__);
       return -1;
    }

    pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    if (!pVosContext)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Vos ctx is null", __func__);
       return -1;
    }

    switch(sub_cmd)
    {

        case QCSAP_PARAM_CLR_ACL:
            if (VOS_STATUS_SUCCESS != WLANSAP_ClearACL(
#ifdef WLAN_FEATURE_MBSSID
                 WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter)
#else
                 pVosContext
#endif
            ))
            {
               ret = -EIO;
            }
            break;

        case QCSAP_PARAM_ACL_MODE:
            if ((eSAP_ALLOW_ALL < (eSapMacAddrACL)set_value) ||
                (eSAP_ACCEPT_UNLESS_DENIED > (eSapMacAddrACL)set_value))
            {
                hddLog(LOGE, FL("Invalid ACL Mode value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
#ifdef WLAN_FEATURE_MBSSID
                WLANSAP_SetMode(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), set_value);
#else
                WLANSAP_SetMode(pVosContext, set_value);
#endif

            }
            break;

        case QCSAP_PARAM_AUTO_CHANNEL:
            if ((0 != set_value) && (1 != set_value))
            {
                hddLog(LOGE, FL("Invalid setAutoChannel value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
#ifdef WLAN_FEATURE_MBSSID
                pHostapdAdapter->sap_dyn_ini_cfg.apAutoChannelSelection =
                                                                      set_value;
#else
                (WLAN_HDD_GET_CTX
                (pHostapdAdapter))->cfg_ini->apAutoChannelSelection = set_value;
#endif
            }
            break;

        case QCSAP_PARAM_SET_CHANNEL_CHANGE:
            if ( WLAN_HDD_SOFTAP == pHostapdAdapter->device_mode )
            {
                hddLog(LOG1, "SET SAP Channel Change to new channel= %d",
                              set_value);
                ret = hdd_softap_set_channel_change(dev, set_value);
            }
            else
            {
                hddLog(LOGE, FL("%s:Channel Change Failed, Device in test mode"),
                                 __func__);
                ret = -EINVAL;
            }
            break;

        case QCSAP_PARAM_MAX_ASSOC:
            if (WNI_CFG_ASSOC_STA_LIMIT_STAMIN > set_value)
            {
                hddLog(LOGE, FL("Invalid setMaxAssoc value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
                if (WNI_CFG_ASSOC_STA_LIMIT_STAMAX < set_value)
                {
                    hddLog(LOGW, FL("setMaxAssoc value %d higher than max allowed %d."
                                "Setting it to max allowed and continuing"),
                                set_value, WNI_CFG_ASSOC_STA_LIMIT_STAMAX);
                    set_value = WNI_CFG_ASSOC_STA_LIMIT_STAMAX;
                }
                status = ccmCfgSetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT,
                                      set_value, NULL, eANI_BOOLEAN_FALSE);
                if ( status != eHAL_STATUS_SUCCESS )
                {
                    hddLog(LOGE, FL("setMaxAssoc failure, status %d"),
                            status);
                    ret = -EIO;
                }
            }
            break;

        case QCSAP_PARAM_HIDE_SSID:
            {
                eHalStatus status = eHAL_STATUS_SUCCESS;
                status = sme_HideSSID(hHal, pHostapdAdapter->sessionId, set_value);
                if(eHAL_STATUS_SUCCESS != status)
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: QCSAP_PARAM_HIDE_SSID failed",
                            __func__);
                    return status;
                }
                break;
            }
        case QCSAP_PARAM_SET_MC_RATE:
            {
                tSirRateUpdateInd *rateUpdate;
                hdd_context_t*pHddCtx = (hdd_context_t*)(pHostapdAdapter->pHddCtx);
                hdd_config_t *pConfig = NULL;

                if (pHddCtx)
                    pConfig = pHddCtx->cfg_ini;
                else {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                           "%s: pHddCtx = NULL", __func__);
                    ret = -1;
                    break;
                }

                rateUpdate = (tSirRateUpdateInd *)
                             vos_mem_malloc(sizeof(tSirRateUpdateInd));
                if (NULL == rateUpdate)
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: SET_MC_RATE indication alloc fail", __func__);
                   ret = -1;
                   break;
                }
                vos_mem_zero(rateUpdate, sizeof(tSirRateUpdateInd ));

                hddLog(VOS_TRACE_LEVEL_INFO, "MC Target rate %d", set_value);
                memcpy(rateUpdate->bssid,
                       pHostapdAdapter->macAddressCurrent.bytes,
                       sizeof(tSirMacAddr));
                rateUpdate->nss = (pConfig->enable2x2 == 0) ? 0 : 1;
                rateUpdate->dev_mode = pHostapdAdapter->device_mode;
                rateUpdate->mcastDataRate24GHz = set_value;
                rateUpdate->mcastDataRate24GHzTxFlag = 1;
                rateUpdate->mcastDataRate5GHz = set_value;
                rateUpdate->bcastDataRate = -1;
                status = sme_SendRateUpdateInd(hHal, rateUpdate);
                if (eHAL_STATUS_SUCCESS != status)
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: SET_MC_RATE failed", __func__);
                    vos_mem_free(rateUpdate);
                    ret = -1;
                }
                break;
            }

#ifdef QCA_WIFI_2_0
         case QCSAP_PARAM_SET_TXRX_FW_STATS:
             {
                  hddLog(LOG1, "QCSAP_PARAM_SET_TXRX_FW_STATS val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID,
                                               set_value, VDEV_CMD);
                  break;
             }
         /* Firmware debug log */
         case QCSAP_DBGLOG_LOG_LEVEL:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_LOG_LEVEL val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_LOG_LEVEL,
                                               set_value, DBG_CMD);
                  break;
             }

         case QCSAP_DBGLOG_VAP_ENABLE:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_VAP_ENABLE val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_VAP_ENABLE,
                                               set_value, DBG_CMD);
                  break;
             }

         case QCSAP_DBGLOG_VAP_DISABLE:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_VAP_DISABLE val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_VAP_DISABLE,
                                               set_value, DBG_CMD);
                  break;
             }

         case QCSAP_DBGLOG_MODULE_ENABLE:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_MODULE_ENABLE val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_MODULE_ENABLE,
                                               set_value, DBG_CMD);
                  break;
             }

         case QCSAP_DBGLOG_MODULE_DISABLE:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_MODULE_DISABLE val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_MODULE_DISABLE,
                                               set_value, DBG_CMD);
                  break;
             }

         case QCSAP_DBGLOG_MOD_LOG_LEVEL:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_MOD_LOG_LEVEL val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_MOD_LOG_LEVEL,
                                               set_value, DBG_CMD);
                  break;
             }

         case QCSAP_DBGLOG_TYPE:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_TYPE val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_TYPE,
                                               set_value, DBG_CMD);
                  break;
             }
         case QCSAP_DBGLOG_REPORT_ENABLE:
             {
                  hddLog(LOG1, "QCSAP_DBGLOG_REPORT_ENABLE val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)WMI_DBGLOG_REPORT_ENABLE,
                                               set_value, DBG_CMD);
                  break;
             }
#ifdef DEBUG
         case QCSAP_FW_CRASH_INJECT:
             {
                  hddLog(LOGE, "WE_FW_CRASH_INJECT %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                                (int)GEN_PARAM_CRASH_INJECT,
                                                set_value, GEN_CMD);
                  break;
             }
#endif
         case QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY:
             {
                  tVOS_CONCURRENCY_MODE concurrent_state = 0;
                  v_U8_t first_adapter_operating_channel = 0;
                  int ret = 0; /* success */
                  hddLog(LOG1, "%s: iwpriv cmd to set MCC latency with val: "
                          "%dms", __func__, set_value);
                  concurrent_state = hdd_get_concurrency_mode();
                  /**
                   * Check if concurrency mode is active.
                   * Need to modify this code to support MCC modes other than
                   * STA/P2P GO
                   */
                  if (concurrent_state == (VOS_STA | VOS_P2P_GO))
                  {
                      hddLog(LOG1, "%s: STA & P2P are both enabled", __func__);
                      /**
                       * The channel number and latency are formatted in
                       * a bit vector then passed on to WMA layer.
                       +**********************************************+
                       | bits 31-16 | bits 15-8         |  bits 7-0   |
                       | Unused     | latency - Chan. 1 |  channel no.|
                       +**********************************************+
                       */

                      /* Get the operating channel of the designated vdev */
                      first_adapter_operating_channel =
                                    hdd_get_operating_channel
                                    (
                                    pHostapdAdapter->pHddCtx,
                                    pHostapdAdapter->device_mode
                                    );
                      /* Move the time latency for the adapter to bits 15-8 */
                      set_value = set_value << 8;
                      /* Store the channel number at bits 7-0 of the bit vector
                       * as per the bit format above.
                       */
                      set_value = set_value | first_adapter_operating_channel;
                      /* Send command to WMA */
                      ret = process_wma_set_command
                                        (
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMA_VDEV_MCC_SET_TIME_LATENCY,
                                        set_value, VDEV_CMD
                                        );
                  }
                  else
                  {
                      hddLog(LOG1, "%s: MCC is not active. Exit w/o setting"
                              " latency", __func__);
                  }
                  break;
             }

         case QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA:
             {
                 v_U8_t first_adapter_operating_channel = 0;
                 v_U8_t second_adapter_opertaing_channel = 0;
                 tVOS_CONCURRENCY_MODE concurrent_state = 0;
                 hdd_adapter_t *staAdapter = NULL;
                 int ret = 0; /* success */

                 hddLog(LOG1, "%s: iwpriv cmd to set MCC quota value %dms",
                         __func__, set_value);
                 /**
                  * Check if concurrency mode is active.
                  * Need to modify this code to support MCC modes other than
                  * STA/P2P GO
                  */
                 concurrent_state = hdd_get_concurrency_mode();
                 if (concurrent_state == (VOS_STA | VOS_P2P_GO))
                 {
                     hddLog(LOG1, "%s: STA & P2P are both enabled", __func__);
                     /**
                      * The channel numbers for both adapters and the time
                      * quota for the 1st adapter, i.e., one specified in cmd
                      * are formatted as a bit vector then passed on to WMA
                      +************************************************+
                      |bit 31-24 |bit 23-16  |  bits 15-8  |bits 7-0   |
                      |  Unused  |  Quota for| chan. # for |chan. # for|
                      |          |  1st chan.| 1st chan.   |2nd chan.  |
                      +************************************************+
                      */

                     /* Get the operating channel of the specified vdev */
                     first_adapter_operating_channel =
                                         hdd_get_operating_channel
                                         (
                                         pHostapdAdapter->pHddCtx,
                                         pHostapdAdapter->device_mode
                                         );
                     hddLog(LOG1, "%s: 1st channel No.:%d and quota:%dms",
                             __func__, first_adapter_operating_channel,
                             set_value);
                     /* Move the time quota for first adapter to bits 15-8 */
                     set_value = set_value << 8;
                     /** Store the operating channel number of 1st adapter at
                      * the lower 8-bits of bit vector.
                      */
                     set_value = set_value | first_adapter_operating_channel;
                     if (pHostapdAdapter->device_mode ==
                                                      WLAN_HDD_INFRA_STATION)
                     {
                         /* iwpriv cmd issued on wlan0; get p2p0 vdev chan. */
                         if ((concurrent_state & VOS_P2P_CLIENT) != 0)
                         {
                             /* The 2nd MCC vdev is P2P client */
                             staAdapter = hdd_get_adapter
                                                   (
                                                   pHostapdAdapter->pHddCtx,
                                                   WLAN_HDD_P2P_CLIENT
                                                   );
                         } else
                         {
                             /* The 2nd MCC vdev is P2P GO */
                             staAdapter = hdd_get_adapter
                                                    (
                                                    pHostapdAdapter->pHddCtx,
                                                    WLAN_HDD_P2P_GO
                                                    );
                         }
                     }
                     else
                     {
                         /* iwpriv cmd issued on p2p0; get channel for wlan0 */
                         staAdapter = hdd_get_adapter
                                                  (
                                                  pHostapdAdapter->pHddCtx,
                                                  WLAN_HDD_INFRA_STATION
                                                  );
                     }
                     if (staAdapter != NULL)
                     {
                         second_adapter_opertaing_channel =
                                 hdd_get_operating_channel
                                 (
                                 staAdapter->pHddCtx,
                                 staAdapter->device_mode
                                 );
                         hddLog(LOG1, "%s: 2nd vdev channel No. is:%d",
                                 __func__, second_adapter_opertaing_channel);
                         /* Move the time quota and operating channel number
                          * for the first adapter to bits 23-16 & bits 15-8
                          *  of set_value vector, respectively.
                          */
                         set_value = set_value << 8;
                         /* Store the channel number for 2nd MCC vdev at bits
                          * 7-0 of set_value vector as per the bit format above.
                          */
                         set_value = set_value |
                                             second_adapter_opertaing_channel;
                         ret = process_wma_set_command
                                              (
                                              (int)pHostapdAdapter->sessionId,
                                              (int)WMA_VDEV_MCC_SET_TIME_QUOTA,
                                              set_value,
                                              VDEV_CMD
                                              );
                     }
                     else
                     {
                         hddLog(LOGE, "%s: NULL adapter handle. Exit",
                                 __func__);
                     }
                 }
                 else
                 {
                     hddLog(LOG1, "%s: MCC is not active. "
                             "Exit w/o setting latency", __func__);
                 }
                 break;
             }

         case QCASAP_TXRX_FWSTATS_RESET:
             {
                  hddLog(LOG1, "WE_TXRX_FWSTATS_RESET val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                                (int)WMA_VDEV_TXRX_FWSTATS_RESET_CMDID,
                                                set_value, VDEV_CMD);
                  break;
             }

         case QCSAP_PARAM_RTSCTS:
            {
                ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                (int)WMI_VDEV_PARAM_ENABLE_RTSCTS,
                                set_value, VDEV_CMD);
                if (ret) {
                    hddLog(LOGE, "FAILED TO SET RTSCTS at SAP");
                    ret = -EIO;
                }
                break;
            }
        case QCASAP_SET_11N_RATE:
            {
                u_int8_t preamble = 0, nss = 0, rix = 0;
                tsap_Config_t *pConfig =
                        &pHostapdAdapter->sessionCtx.ap.sapConfig;

                hddLog(LOG1, "WMI_VDEV_PARAM_FIXED_RATE val %d", set_value);

                rix = RC_2_RATE_IDX(set_value);
                if (set_value & 0x80) {
                    if (pConfig->SapHw_mode == eSAP_DOT11_MODE_11b ||
                        pConfig->SapHw_mode == eSAP_DOT11_MODE_11b_ONLY ||
                        pConfig->SapHw_mode == eSAP_DOT11_MODE_11g ||
                        pConfig->SapHw_mode == eSAP_DOT11_MODE_11g_ONLY ||
                        pConfig->SapHw_mode == eSAP_DOT11_MODE_abg ||
                        pConfig->SapHw_mode == eSAP_DOT11_MODE_11a) {
                        hddLog(VOS_TRACE_LEVEL_ERROR, "Not valid mode for HT");
                        ret = -EIO;
                        break;
                    }
                    preamble = WMI_RATE_PREAMBLE_HT;
                    nss = HT_RC_2_STREAMS(set_value) - 1;
                } else if (set_value & 0x10) {
                    if (pConfig->SapHw_mode == eSAP_DOT11_MODE_11a) {
                        hddLog(VOS_TRACE_LEVEL_ERROR, "Not valid for cck");
                        ret = -EIO;
                        break;
                    }
                    preamble = WMI_RATE_PREAMBLE_CCK;
                    /* Enable Short preamble always for CCK except 1mbps */
                    if (rix != 0x3)
                        rix |= 0x4;
                } else {
                    if (pConfig->SapHw_mode == eSAP_DOT11_MODE_11b ||
                        pConfig->SapHw_mode == eSAP_DOT11_MODE_11b_ONLY) {
                        hddLog(VOS_TRACE_LEVEL_ERROR, "Not valid for OFDM");
                        ret = -EIO;
                        break;
                    }
                    preamble = WMI_RATE_PREAMBLE_OFDM;
                }
                hddLog(LOG1, "WMI_VDEV_PARAM_FIXED_RATE val %d rix %d "
                    "preamble %x nss %d", set_value, rix, preamble, nss);

                set_value = (preamble << 6) | (nss << 4) | rix;
                ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                              (int)WMI_VDEV_PARAM_FIXED_RATE,
                                              set_value, VDEV_CMD);
                break;
            }

        case QCASAP_SET_VHT_RATE:
            {
                u_int8_t preamble, nss, rix;
                tsap_Config_t *pConfig =
                    &pHostapdAdapter->sessionCtx.ap.sapConfig;

                if (pConfig->SapHw_mode != eSAP_DOT11_MODE_11ac &&
                    pConfig->SapHw_mode != eSAP_DOT11_MODE_11ac_ONLY) {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: SET_VHT_RATE error: SapHw_mode= 0x%x, ch = %d",
                        __func__, pConfig->SapHw_mode, pConfig->channel);
                    ret = -EIO;
                    break;
                }

                rix = RC_2_RATE_IDX_11AC(set_value);
                preamble = WMI_RATE_PREAMBLE_VHT;
                nss = HT_RC_2_STREAMS_11AC(set_value) - 1;

                hddLog(LOG1, "WMI_VDEV_PARAM_FIXED_RATE val %d rix %d "
                    "preamble %x nss %d", set_value, rix, preamble, nss);

                set_value = (preamble << 6) | (nss << 4) | rix;
                ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                              (int)WMI_VDEV_PARAM_FIXED_RATE,
                                              set_value, VDEV_CMD);
                break;
            }

         case QCASAP_SHORT_GI:
             {
                  hddLog(LOG1, "QCASAP_SET_SHORT_GI val %d", set_value);

                  ret = sme_UpdateHTConfig(hHal, pHostapdAdapter->sessionId,
                                           WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ, /* same as 40MHZ */
                                           set_value);
                  if (ret)
                      hddLog(LOGE, "Failed to set ShortGI value ret(%d)", ret);
                  break;
             }

         case QCSAP_SET_AMPDU:
             {
                  hddLog(LOG1, "QCSAP_SET_AMPDU val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)GEN_VDEV_PARAM_AMPDU,
                                               set_value, GEN_CMD);
                  break;
             }

         case QCSAP_SET_AMSDU:
             {
                  hddLog(LOG1, "QCSAP_SET_AMSDU val %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                               (int)GEN_VDEV_PARAM_AMSDU,
                                               set_value, GEN_CMD);
                  break;
             }
        case QCSAP_GTX_HT_MCS:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_HT_MCS %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_HT_MCS,
                                         set_value, GTX_CMD);
                  break;
             }

        case QCSAP_GTX_VHT_MCS:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_VHT_MCS %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_VHT_MCS,
                                         set_value, GTX_CMD);
                  break;
             }

       case QCSAP_GTX_USRCFG:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_USR_CFG %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_USR_CFG,
                                         set_value, GTX_CMD);
                  break;
             }

        case QCSAP_GTX_THRE:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_THRE %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_THRE,
                                         set_value, GTX_CMD);
                  break;
             }

        case QCSAP_GTX_MARGIN:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_MARGIN %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_MARGIN,
                                         set_value, GTX_CMD);
                  break;
             }

        case QCSAP_GTX_STEP:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_STEP %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_STEP,
                                         set_value, GTX_CMD);
                  break;
             }

        case QCSAP_GTX_MINTPC:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_MINTPC %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_MINTPC,
                                         set_value, GTX_CMD);
                  break;
             }

        case QCSAP_GTX_BWMASK:
             {
                  hddLog(LOG1, "WMI_VDEV_PARAM_GTX_BWMASK %d", set_value);
                  ret = process_wma_set_command((int)pHostapdAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_BW_MASK,
                                         set_value, GTX_CMD);
                  break;
             }



#ifdef QCA_PKT_PROTO_TRACE
         case QCASAP_SET_DEBUG_LOG:
             {
                  hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

                  hddLog(LOG1, "QCASAP_SET_DEBUG_LOG val %d", set_value);
                  /* Trace buffer dump only */
                  if (VOS_PKT_TRAC_DUMP_CMD == set_value)
                  {
                      vos_pkt_trace_buf_dump();
                      break;
                  }
                  pHddCtx->cfg_ini->gEnableDebugLog = set_value;
                  break;
             }
#endif /* QCA_PKT_PROTO_TRACE */

        case QCASAP_SET_TM_LEVEL:
             {
                  hddLog(VOS_TRACE_LEVEL_INFO, "Set Thermal Mitigation Level %d",
                            set_value);
#ifdef QCA_WIFI_ISOC
                  hddLog(VOS_TRACE_LEVEL_ERROR, " 'setTmLevel' Command Not supported for this mode");
#else
                  (void)sme_SetThermalLevel(hHal, set_value);
#endif
                  break;
             }


        case QCASAP_SET_DFS_IGNORE_CAC:
             {
                  hddLog(VOS_TRACE_LEVEL_INFO, "Set Dfs ignore CAC  %d",
                            set_value);

                  if (pHostapdAdapter->device_mode != WLAN_HDD_SOFTAP)
                       return -EINVAL;

                  ret = WLANSAP_Set_Dfs_Ignore_CAC(hHal, set_value);
                  break;
             }

        case QCASAP_SET_DFS_TARGET_CHNL:
             {
                  hddLog(VOS_TRACE_LEVEL_INFO, "Set Dfs target channel  %d",
                            set_value);

                  if (pHostapdAdapter->device_mode != WLAN_HDD_SOFTAP)
                       return -EINVAL;

                  ret = WLANSAP_Set_Dfs_Target_Chnl(hHal, set_value);
                  break;
             }

#endif /* QCA_WIFI_2_0 */

        case QCASAP_SET_DFS_NOL:
             WLANSAP_Set_DfsNol(
#ifdef WLAN_FEATURE_MBSSID
                     WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
#else
                     pVosContext,
#endif
                     (eSapDfsNolType)set_value
                     );
             break;

        case QCASAP_SET_RADAR_CMD:
            {
                hdd_context_t *pHddCtx =
                    WLAN_HDD_GET_CTX(pHostapdAdapter);
                v_U8_t ch =
                    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->operatingChannel;
                v_BOOL_t isDfsch;

                isDfsch = (NV_CHANNEL_DFS ==
                                vos_nv_getChannelEnabledState(ch));

                hddLog(VOS_TRACE_LEVEL_INFO,
                       FL("Set QCASAP_SET_RADAR_CMD val %d"), set_value);

                if (!pHddCtx->dfs_radar_found && isDfsch) {
                    ret = process_wma_set_command(
                            (int)pHostapdAdapter->sessionId,
                            (int)WMA_VDEV_DFS_CONTROL_CMDID,
                            set_value, VDEV_CMD);
                } else {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                        FL("Ignore command due to "
                            "dfs_radar_found: %d, is_dfs_channel: %d"),
                        pHddCtx->dfs_radar_found, isDfsch);
                }
                break;
            }
        default:
            hddLog(LOGE, FL("Invalid setparam command %d value %d"),
                    sub_cmd, set_value);
            ret = -EINVAL;
            break;
    }

    return ret;
}

/**
 * __iw_softap_get_three() - return three value to upper layer.
 *
 * @dev: pointer of net_device of this wireless card
 * @info: meta data about Request sent
 * @wrqu: include request info
 * @extra: buf used for in/out
 *
 * Return: execute result
 */
static int __iw_softap_get_three(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	uint32_t *value = (uint32_t *)extra;
	uint32_t sub_cmd = value[0];
	int ret = 0; /* success */

	hdd_adapter_t *padapter = WLAN_HDD_GET_PRIV_PTR(dev);

	switch (sub_cmd) {
	case QCSAP_GET_TSF:
		ret = hdd_indicate_tsf(padapter, value, 3);
		break;
	default:
		hddLog(LOGE, FL("Invalid getparam command %d"), sub_cmd);
		break;
	}
	return ret;
}


/**
 * iw_softap_get_three() - return three value to upper layer.
 *
 * @dev: pointer of net_device of this wireless card
 * @info: meta data about Request sent
 * @wrqu: include request info
 * @extra: buf used for in/Output
 *
 * Return: execute result
 */
static int iw_softap_get_three(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_softap_get_three(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

int
static iw_softap_getparam(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int sub_cmd = value[0];
    eHalStatus status;
    int ret = 0; /* success */
    hdd_context_t *pHddCtx = NULL;
    pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    switch (sub_cmd)
    {
    case QCSAP_PARAM_MAX_ASSOC:
        status = ccmCfgGetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT, (tANI_U32 *)value);
        if (eHAL_STATUS_SUCCESS != status)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      FL("failed to get WNI_CFG_ASSOC_STA_LIMIT from cfg %d"),status);
            ret = -EIO;
        }
        break;

    case QCSAP_PARAM_GET_WLAN_DBG:
        {
            vos_trace_display();
            *value = 0;
            break;
        }

    case QCSAP_PARAM_AUTO_CHANNEL:
        {
#ifdef WLAN_FEATURE_MBSSID
            *value = pHostapdAdapter->sap_dyn_ini_cfg.apAutoChannelSelection;
#else
            *value = (WLAN_HDD_GET_CTX
                      (pHostapdAdapter))->cfg_ini->apAutoChannelSelection;
#endif
             break;
        }

    case QCSAP_PARAM_RTSCTS:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            void *wmapvosContext = wmahddCtxt->pvosContext;
            *value = wma_cli_get_command(wmapvosContext,
                                             (int)pHostapdAdapter->sessionId,
                                             (int)WMI_VDEV_PARAM_ENABLE_RTSCTS,
                                             VDEV_CMD);
            break;
        }

    case QCASAP_SHORT_GI:
        {
            *value = (int)sme_GetHTConfig(hHal,
                                          pHostapdAdapter->sessionId,
                                          WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ);
            break;
        }

    case QCSAP_GTX_HT_MCS:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_HT_MCS");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_HT_MCS,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_VHT_MCS:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_VHT_MCS");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_VHT_MCS,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_USRCFG:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_USR_CFG");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_USR_CFG,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_THRE:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_THRE");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_THRE,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_MARGIN:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_MARGIN");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_MARGIN,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_STEP:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_STEP");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_STEP,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_MINTPC:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_MINTPC");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_MINTPC,
                                        GTX_CMD);
            break;
        }

    case QCSAP_GTX_BWMASK:
        {
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
            hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_BW_MASK");
            *value = wma_cli_get_command(wmahddCtxt->pvosContext,
                                        (int)pHostapdAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_BW_MASK,
                                        GTX_CMD);
            break;
        }

    case QCASAP_GET_DFS_NOL:
        {
#ifndef WLAN_FEATURE_MBSSID
            hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pHostapdAdapter);
#endif
            WLANSAP_Get_DfsNol(
#ifdef WLAN_FEATURE_MBSSID
                    WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter)
#else
                    wmahddCtxt->pvosContext
#endif
                    );
        }
        break;

    case QCSAP_GET_ACL:
        {
            hddLog(LOG1, FL("QCSAP_GET_ACL"));
            if (hdd_print_acl(pHostapdAdapter) != VOS_STATUS_SUCCESS) {
                hddLog(LOGE, FL("QCSAP_GET_ACL returned Error: not completed"));
            }
            *value = 0;
            break;
        }

    case QCSAP_CAP_TSF:
        {
            ret = hdd_capture_tsf(pHostapdAdapter, (uint32_t *)value, 1);
            break;
        }
    default:
        hddLog(LOGE, FL("Invalid getparam command %d"), sub_cmd);
        ret = -EINVAL;
        break;
    }

    return ret;
}

/* Usage:
    BLACK_LIST  = 0
    WHITE_LIST  = 1
    ADD MAC = 0
    REMOVE MAC  = 1

    mac addr will be accepted as a 6 octet mac address with each octet inputted in hex
    for e.g. 00:0a:f5:11:22:33 will be represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
    while using this ioctl

    Syntax:
    iwpriv softap.0 modify_acl
    <6 octet mac addr> <list type> <cmd type>

    Examples:
    eg 1. to add a mac addr 00:0a:f5:89:89:90 to the black list
    iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 0 0
    eg 2. to delete a mac addr 00:0a:f5:89:89:90 from white list
    iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 1 1
*/
int iw_softap_modify_acl(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif
    v_BYTE_t *value = (v_BYTE_t*)extra;
    v_U8_t pPeerStaMac[VOS_MAC_ADDR_SIZE];
    int listType, cmd, i;
    int ret = 0; /* success */
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

    ENTER();
    for (i=0; i<VOS_MAC_ADDR_SIZE; i++)
    {
        pPeerStaMac[i] = *(value+i);
    }
    listType = (int)(*(value+i));
    i++;
    cmd = (int)(*(value+i));

    hddLog(LOG1, "%s: SAP Modify ACL arg0 " MAC_ADDRESS_STR " arg1 %d arg2 %d",
            __func__, MAC_ADDR_ARRAY(pPeerStaMac), listType, cmd);

#ifdef WLAN_FEATURE_MBSSID
    vos_status = WLANSAP_ModifyACL(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), pPeerStaMac,(eSapACLType)listType,(eSapACLCmdType)cmd);
#else
    vos_status = WLANSAP_ModifyACL(pVosContext, pPeerStaMac,(eSapACLType)listType,(eSapACLCmdType)cmd);
#endif
    if (!VOS_IS_STATUS_SUCCESS(vos_status))
    {
        hddLog(LOGE, FL("Modify ACL failed"));
        ret = -EIO;
    }
    EXIT();
    return ret;
}

int
static iw_softap_getchannel(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));

    int *value = (int *)extra;

    *value = (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->operatingChannel;
    return 0;
}

int
static iw_softap_set_max_tx_power(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int set_value;
    tSirMacAddr bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tSirMacAddr selfMac = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    if (NULL == value)
        return -ENOMEM;

    /* Assign correct slef MAC address */
    vos_mem_copy(bssid, pHostapdAdapter->macAddressCurrent.bytes,
                 VOS_MAC_ADDR_SIZE);
    vos_mem_copy(selfMac, pHostapdAdapter->macAddressCurrent.bytes,
                 VOS_MAC_ADDR_SIZE);

    set_value = value[0];
    if (eHAL_STATUS_SUCCESS != sme_SetMaxTxPower(hHal, bssid, selfMac, set_value))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting maximum tx power failed",
                __func__);
        return -EIO;
    }

    return 0;
}

int
static iw_display_data_path_snapshot(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{

    /* Function intitiating dumping states of
     *  HDD(WMM Tx Queues)
     *  TL State (with Per Client infor)
     *  DXE Snapshot (Called at the end of TL Snapshot)
     */
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hddLog(LOGE, "%s: called for SAP",__func__);
    hdd_wmm_tx_snapshot(pHostapdAdapter);
    WLANTL_TLDebugMessage(VOS_TRUE);
    return 0;
}

int
static iw_softap_set_tx_power(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int set_value;
    tSirMacAddr bssid;

    if (NULL == value)
        return -ENOMEM;

    vos_mem_copy(bssid, pHostapdAdapter->macAddressCurrent.bytes,
           VOS_MAC_ADDR_SIZE);

    set_value = value[0];
    if (eHAL_STATUS_SUCCESS != sme_SetTxPower(hHal, pHostapdAdapter->sessionId, bssid,
                                              pHostapdAdapter->device_mode,
                                              set_value))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting tx power failed",
                __func__);
        return -EIO;
    }

    return 0;
}

/**---------------------------------------------------------------------------

  \brief iw_softap_set_trafficmonitor() -
   This function dynamically enable/disable traffic monitor functonality
   the command iwpriv wlanX setTrafficMon <value>.

  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int iw_softap_set_trafficmonitor(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int *isSetTrafficMon = (int *)wrqu->data.pointer;
    hdd_context_t *pHddCtx;
    int status;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s : ", __func__);

    if (NULL == isSetTrafficMon)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from extra", __func__);
        return -ENOMEM;
    }

    if (TRUE == *isSetTrafficMon)
    {
        pHddCtx->cfg_ini->enableTrafficMonitor= TRUE;
        if (VOS_STATUS_SUCCESS != hdd_start_trafficMonitor(pAdapter))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: failed to Start Traffic Monitor timer ", __func__ );
            return -EIO;
        }
    }
    else if (FALSE == *isSetTrafficMon)
    {
        pHddCtx->cfg_ini->enableTrafficMonitor= FALSE;
        if (VOS_STATUS_SUCCESS != hdd_stop_trafficMonitor(pAdapter))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: failed to Stop Traffic Monitor timer ", __func__ );
            return -EIO;
        }

    }
    return 0;
}

#define IS_BROADCAST_MAC(x) (((x[0] & x[1] & x[2] & x[3] & x[4] & x[5]) == 0xff) ? 1 : 0)

int
static iw_softap_getassoc_stamacaddr(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hdd_station_info_t *pStaInfo = pHostapdAdapter->aStaInfo;
    char *buf;
    int cnt = 0;
    int left;
    int ret = 0;
    /* maclist_index must be u32 to match userspace */
    u32 maclist_index;

    /*
     * NOTE WELL: this is a "get" ioctl but it uses an even ioctl
     * number, and even numbered iocts are supposed to have "set"
     * semantics.  Hence the wireless extensions support in the kernel
     * won't correctly copy the result to userspace, so the ioctl
     * handler itself must copy the data.  Output format is 32-bit
     * record length, followed by 0 or more 6-byte STA MAC addresses.
     *
     * Further note that due to the incorrect semantics, the "iwpriv"
     * userspace application is unable to correctly invoke this API,
     * hence it is not registered in the hostapd_private_args.  This
     * API can only be invoked by directly invoking the ioctl() system
     * call.
     */

    /* make sure userspace allocated a reasonable buffer size */
    if (wrqu->data.length < sizeof(maclist_index)) {
        hddLog(LOG1, "%s: invalid userspace buffer", __func__);
        return -EINVAL;
    }

    /* allocate local buffer to build the response */
    buf = kmalloc(wrqu->data.length, GFP_KERNEL);
    if (!buf) {
        hddLog(LOG1, "%s: failed to allocate response buffer", __func__);
        return -ENOMEM;
    }

    /* start indexing beyond where the record count will be written */
    maclist_index = sizeof(maclist_index);
    left = wrqu->data.length - maclist_index;

    spin_lock_bh(&pHostapdAdapter->staInfo_lock);
    while ((cnt < WLAN_MAX_STA_COUNT) && (left >= VOS_MAC_ADDR_SIZE)) {
        if ((pStaInfo[cnt].isUsed) &&
            (!IS_BROADCAST_MAC(pStaInfo[cnt].macAddrSTA.bytes))) {
            memcpy(&buf[maclist_index], &(pStaInfo[cnt].macAddrSTA),
                   VOS_MAC_ADDR_SIZE);
            maclist_index += VOS_MAC_ADDR_SIZE;
            left -= VOS_MAC_ADDR_SIZE;
        }
        cnt++;
    }
    spin_unlock_bh(&pHostapdAdapter->staInfo_lock);

    *((u32 *)buf) = maclist_index;
    wrqu->data.length = maclist_index;
    if (copy_to_user(wrqu->data.pointer, buf, maclist_index)) {
        hddLog(LOG1, "%s: failed to copy response to user buffer", __func__);
        ret = -EFAULT;
    }
    kfree(buf);
    return ret;
}

/* Usage:
    mac addr will be accepted as a 6 octet mac address with each octet inputted in hex
    for e.g. 00:0a:f5:11:22:33 will be represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
    while using this ioctl

    Syntax:
    iwpriv softap.0 disassoc_sta <6 octet mac address>

    e.g.
    disassociate sta with mac addr 00:0a:f5:11:22:33 from softap
    iwpriv softap.0 disassoc_sta 0x00 0x0a 0xf5 0x11 0x22 0x33
*/

int
static iw_softap_disassoc_sta(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_U8_t *peerMacAddr;

    ENTER();
    /* iwpriv tool or framework calls this ioctl with
     * data passed in extra (less than 16 octets);
     */
    peerMacAddr = (v_U8_t *)(extra);

    hddLog(LOG1, "%s data "  MAC_ADDRESS_STR,
           __func__, MAC_ADDR_ARRAY(peerMacAddr));
    hdd_softap_sta_disassoc(pHostapdAdapter, peerMacAddr);
    EXIT();
    return 0;
}

int
static iw_softap_ap_stats(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    WLANTL_TRANSFER_STA_TYPE  statBuffer;
    char *pstatbuf;
    int len = wrqu->data.length;
    pstatbuf = wrqu->data.pointer;

    memset(&statBuffer, 0, sizeof(statBuffer));
    WLANSAP_GetStatistics((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
                           &statBuffer, (v_BOOL_t)wrqu->data.flags);

    pstatbuf = kmalloc(wrqu->data.length, GFP_KERNEL);
    if(NULL == pstatbuf) {
        hddLog(LOG1, "unable to allocate memory");
        return -ENOMEM;
    }
    len = scnprintf(pstatbuf, wrqu->data.length,
                    "RUF=%d RMF=%d RBF=%d "
                    "RUB=%d RMB=%d RBB=%d "
                    "TUF=%d TMF=%d TBF=%d "
                    "TUB=%d TMB=%d TBB=%d",
                    (int)statBuffer.rxUCFcnt, (int)statBuffer.rxMCFcnt,
                    (int)statBuffer.rxBCFcnt, (int)statBuffer.rxUCBcnt,
                    (int)statBuffer.rxMCBcnt, (int)statBuffer.rxBCBcnt,
                    (int)statBuffer.txUCFcnt, (int)statBuffer.txMCFcnt,
                    (int)statBuffer.txBCFcnt, (int)statBuffer.txUCBcnt,
                    (int)statBuffer.txMCBcnt, (int)statBuffer.txBCBcnt);

    if (len > wrqu->data.length ||
        copy_to_user((void *)wrqu->data.pointer, (void *)pstatbuf, len))
    {
        hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
        kfree(pstatbuf);
        return -EFAULT;
    }
    wrqu->data.length -= len;
    kfree(pstatbuf);
    return 0;
}

static int iw_softap_set_channel_range(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

    int *value = (int *)extra;
    int startChannel = value[0];
    int endChannel = value[1];
    int band = value[2];
    VOS_STATUS status;
    int ret = 0; /* success */

    status = WLANSAP_SetChannelRange(hHal,startChannel,endChannel,band);
    if(status != VOS_STATUS_SUCCESS)
    {
      hddLog( LOGE, FL("iw_softap_set_channel_range:  startChannel = %d, endChannel = %d band = %d"),
                                  startChannel,endChannel, band);
      ret = -EINVAL;
    }

    pHddCtx->is_dynamic_channel_range_set = 1;

    return ret;
}

int iw_softap_get_channel_list(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    v_U32_t num_channels = 0;
    v_U8_t i = 0;
    v_U8_t bandStartChannel = RF_CHAN_1;
    v_U8_t bandEndChannel = RF_CHAN_165;
    v_U32_t temp_num_channels = 0;
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    v_REGDOMAIN_t domainIdCurrentSoftap;
    tpChannelListInfo channel_list = (tpChannelListInfo) extra;
    eCsrBand curBand = eCSR_BAND_ALL;

    if (eHAL_STATUS_SUCCESS != sme_GetFreqBand(hHal, &curBand))
    {
        hddLog(LOGE,FL("not able get the current frequency band"));
        return -EIO;
    }
    wrqu->data.length = sizeof(tChannelListInfo);
    ENTER();

    if (eCSR_BAND_24 == curBand)
    {
        bandStartChannel = RF_CHAN_1;
        bandEndChannel = RF_CHAN_14;
    }
    else if (eCSR_BAND_5G == curBand)
    {
        bandStartChannel = RF_CHAN_36;
        bandEndChannel = RF_CHAN_165;
    }

    hddLog(LOG1, FL("curBand = %d, bandStartChannel = %hu, "
                "bandEndChannel = %hu "), curBand,
                bandStartChannel, bandEndChannel );

    for( i = bandStartChannel; i <= bandEndChannel; i++ )
    {
        if( NV_CHANNEL_ENABLE == regChannels[i].enabled )
        {
            channel_list->channels[num_channels] = rfChannels[i].channelNum;
            num_channels++;
        }
    }

    /* remove indoor channels if the domain is FCC, channels 36 - 48 */

    temp_num_channels = num_channels;

    if(eHAL_STATUS_SUCCESS != sme_getSoftApDomain(hHal,(v_REGDOMAIN_t *) &domainIdCurrentSoftap))
    {
        hddLog(LOGE,FL("Failed to get Domain ID, %d "),domainIdCurrentSoftap);
        return -EIO;
    }

    if(REGDOMAIN_FCC == domainIdCurrentSoftap)
    {
        for(i = 0; i < temp_num_channels; i++)
        {

           if((channel_list->channels[i] > 35) &&
              (channel_list->channels[i] < 49))
           {
               vos_mem_move(&channel_list->channels[i],
                            &channel_list->channels[i+1],
                            temp_num_channels - (i-1));
               num_channels--;
               temp_num_channels--;
               i--;
           }
        }
    }

    hddLog(LOG1,FL(" number of channels %d"), num_channels);

    if (num_channels > IW_MAX_FREQUENCIES)
    {
        num_channels = IW_MAX_FREQUENCIES;
    }

    channel_list->num_channels = num_channels;
    EXIT();

    return 0;
}

static
int iw_get_genie(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif
    eHalStatus status;
    v_U32_t length = DOT11F_IE_RSN_MAX_LEN;
    v_U8_t genIeBytes[DOT11F_IE_RSN_MAX_LEN];
    ENTER();
    hddLog(LOG1,FL("getGEN_IE ioctl"));
    // Actually retrieve the RSN IE from CSR.  (We previously sent it down in the CSR Roam Profile.)
    status = WLANSap_getstationIE_information(
#ifdef WLAN_FEATURE_MBSSID
                                   WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
#else
                                   pVosContext,
#endif
                                   &length,
                                   genIeBytes
                                   );

    length = VOS_MIN((u_int16_t) length, DOT11F_IE_RSN_MAX_LEN);
    if (wrqu->data.length < length ||
        copy_to_user(wrqu->data.pointer,
                      (v_VOID_t*)genIeBytes, length))
    {
        hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
        return -EFAULT;
    }
    wrqu->data.length = length;

    hddLog(LOG1,FL(" RSN IE of %d bytes returned"), wrqu->data.length );


    EXIT();
    return 0;
}
static
int iw_get_WPSPBCProbeReqIEs(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    sQcSapreq_WPSPBCProbeReqIES_t WPSPBCProbeReqIEs;
    hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    ENTER();

    hddLog(LOG1,FL("get_WPSPBCProbeReqIEs ioctl"));
    memset((void*)&WPSPBCProbeReqIEs, 0, sizeof(WPSPBCProbeReqIEs));

    WPSPBCProbeReqIEs.probeReqIELen = pHddApCtx->WPSPBCProbeReq.probeReqIELen;
    vos_mem_copy(&WPSPBCProbeReqIEs.probeReqIE,
                 pHddApCtx->WPSPBCProbeReq.probeReqIE,
                 WPSPBCProbeReqIEs.probeReqIELen);
    vos_mem_copy(&WPSPBCProbeReqIEs.macaddr,
                 pHddApCtx->WPSPBCProbeReq.peerMacAddr,
                 sizeof(v_MACADDR_t));
    if (copy_to_user(wrqu->data.pointer,
                     (void *)&WPSPBCProbeReqIEs,
                      sizeof(WPSPBCProbeReqIEs)))
    {
         hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
         return -EFAULT;
    }
    wrqu->data.length = 12 + WPSPBCProbeReqIEs.probeReqIELen;
    hddLog(LOG1, FL("Macaddress : "MAC_ADDRESS_STR),
           MAC_ADDR_ARRAY(WPSPBCProbeReqIEs.macaddr));
    up(&pHddApCtx->semWpsPBCOverlapInd);
    EXIT();
    return 0;
}

/**---------------------------------------------------------------------------

  \brief iw_set_auth_hostap() -
   This function sets the auth type received from the wpa_supplicant.

  \param  - dev - Pointer to the net device.
              - info - Pointer to the iw_request_info.
              - wrqu - Pointer to the iwreq_data.
              - extra - Pointer to the data.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
int iw_set_auth_hostap(struct net_device *dev,struct iw_request_info *info,
                        union iwreq_data *wrqu,char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

   ENTER();
   switch(wrqu->param.flags & IW_AUTH_INDEX)
   {
      case IW_AUTH_TKIP_COUNTERMEASURES:
      {
         if(wrqu->param.value) {
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   "Counter Measure started %d", wrqu->param.value);
            pWextState->mTKIPCounterMeasures = TKIP_COUNTER_MEASURE_STARTED;
         }
         else {
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   "Counter Measure stopped=%d", wrqu->param.value);
            pWextState->mTKIPCounterMeasures = TKIP_COUNTER_MEASURE_STOPED;
         }

         hdd_softap_tkip_mic_fail_counter_measure(pAdapter,
                                                  wrqu->param.value);
      }
      break;

      default:

         hddLog(LOGW, "%s called with unsupported auth type %d", __func__,
               wrqu->param.flags & IW_AUTH_INDEX);
      break;
   }

   EXIT();
   return 0;
}

static int iw_set_ap_encodeext(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif
    hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    int retval = 0;
    VOS_STATUS vstatus;
    struct iw_encode_ext *ext = (struct iw_encode_ext*)extra;
    v_U8_t groupmacaddr[VOS_MAC_ADDR_SIZE] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int key_index;
    struct iw_point *encoding = &wrqu->encoding;
    tCsrRoamSetKey  setKey;
//    tCsrRoamRemoveKey RemoveKey;
    int i;

    ENTER();

    key_index = encoding->flags & IW_ENCODE_INDEX;

    if(key_index > 0) {

         /*Convert from 1-based to 0-based keying*/
        key_index--;
    }
    if(!ext->key_len) {
#if 0
      /*Set the encrytion type to NONE*/
#if 0
       pRoamProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
#endif

         RemoveKey.keyId = key_index;
         if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
              /*Key direction for group is RX only*/
             vos_mem_copy(RemoveKey.peerMac,groupmacaddr, VOS_MAC_ADDR_SIZE);
         }
         else {
             vos_mem_copy(RemoveKey.peerMac,ext->addr.sa_data, VOS_MAC_ADDR_SIZE);
         }
         switch(ext->alg)
         {
           case IW_ENCODE_ALG_NONE:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_NONE;
              break;
           case IW_ENCODE_ALG_WEP:
              RemoveKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
              break;
           case IW_ENCODE_ALG_TKIP:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_TKIP;
              break;
           case IW_ENCODE_ALG_CCMP:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_AES;
              break;
          default:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_NONE;
              break;
         }
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Remove key cipher_alg:%d key_len%d *pEncryptionType :%d",
                    __func__,(int)ext->alg,(int)ext->key_len,RemoveKey.encType);
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Peer Mac = "MAC_ADDRESS_STR,
                    __func__, MAC_ADDR_ARRAY(RemoveKey.peerMac));
          );
#ifdef WLAN_FEATURE_MBSSID
         vstatus = WLANSAP_DelKeySta( WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), &RemoveKey );
#else
         vstatus = WLANSAP_DelKeySta( pVosContext, &RemoveKey );
#endif

         if ( vstatus != VOS_STATUS_SUCCESS )
         {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "[%4d] WLANSAP_DeleteKeysSta returned ERROR status= %d",
                        __LINE__, vstatus );
             retval = -EINVAL;
         }
#endif
         return retval;

    }

    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));

    setKey.keyId = key_index;
    setKey.keyLength = ext->key_len;

    if(ext->key_len <= CSR_MAX_KEY_LEN) {
       vos_mem_copy(&setKey.Key[0],ext->key,ext->key_len);
    }

    if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
      /*Key direction for group is RX only*/
       setKey.keyDirection = eSIR_RX_ONLY;
       vos_mem_copy(setKey.peerMac,groupmacaddr, VOS_MAC_ADDR_SIZE);
    }
    else {

       setKey.keyDirection =  eSIR_TX_RX;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data, VOS_MAC_ADDR_SIZE);
    }
    if(ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
    {
       setKey.keyDirection = eSIR_TX_DEFAULT;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data, VOS_MAC_ADDR_SIZE);
    }

    /*For supplicant pae role is zero*/
    setKey.paeRole = 0;

    switch(ext->alg)
    {
       case IW_ENCODE_ALG_NONE:
         setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
         break;

       case IW_ENCODE_ALG_WEP:
         setKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
         pHddApCtx->uPrivacy = 1;
         hddLog(LOG1, "(%s) uPrivacy=%d", __func__, pHddApCtx->uPrivacy);
         break;

       case IW_ENCODE_ALG_TKIP:
       {
          v_U8_t *pKey = &setKey.Key[0];

          setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;

          vos_mem_zero(pKey, CSR_MAX_KEY_LEN);

          /*Supplicant sends the 32bytes key in this order

                |--------------|----------|----------|
                |   Tk1        |TX-MIC    |  RX Mic  |
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->

                */
          /*Sme expects the 32 bytes key to be in the below order

                |--------------|----------|----------|
                |   Tk1        |RX-MIC    |  TX Mic  |
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->
               */
          /* Copy the Temporal Key 1 (TK1) */
          vos_mem_copy(pKey,ext->key,16);

         /*Copy the rx mic first*/
          vos_mem_copy(&pKey[16],&ext->key[24],8);

         /*Copy the tx mic */
          vos_mem_copy(&pKey[24],&ext->key[16],8);

       }
       break;

       case IW_ENCODE_ALG_CCMP:
          setKey.encType = eCSR_ENCRYPT_TYPE_AES;
          break;

       default:
          setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
          break;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%s:EncryptionType:%d key_len:%d, KeyId:%d"), __func__, setKey.encType, setKey.keyLength,
            setKey.keyId);
    for(i=0; i< ext->key_len; i++)
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%02x"), setKey.Key[i]);

#ifdef WLAN_FEATURE_MBSSID
    vstatus = WLANSAP_SetKeySta( WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), &setKey );
#else
    vstatus = WLANSAP_SetKeySta( pVosContext, &setKey );
#endif

    if ( vstatus != VOS_STATUS_SUCCESS )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "[%4d] WLANSAP_SetKeySta returned ERROR status= %d", __LINE__, vstatus );
       retval = -EINVAL;
    }

   return retval;
}


static int iw_set_ap_mlme(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu,
                       char *extra)
{
#if 0
    hdd_adapter_t *pAdapter = (netdev_priv(dev));
    struct iw_mlme *mlme = (struct iw_mlme *)extra;

    ENTER();

    //reason_code is unused. By default it is set to eCSR_DISCONNECT_REASON_UNSPECIFIED
    switch (mlme->cmd) {
        case IW_MLME_DISASSOC:
        case IW_MLME_DEAUTH:
            hddLog(LOG1, "Station disassociate");
            if( pAdapter->conn_info.connState == eConnectionState_Associated )
            {
                eCsrRoamDisconnectReason reason = eCSR_DISCONNECT_REASON_UNSPECIFIED;

                if( mlme->reason_code == HDD_REASON_MICHAEL_MIC_FAILURE )
                    reason = eCSR_DISCONNECT_REASON_MIC_ERROR;

                status = sme_RoamDisconnect( pAdapter->hHal,pAdapter->sessionId, reason);

                //clear all the reason codes
                if (status != 0)
                {
                    hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate : csrRoamDisconnect failure returned %d", __func__, (int)mlme->cmd, (int)status );
                }

               netif_stop_queue(dev);
               netif_carrier_off(dev);
            }
            else
            {
                hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate called but station is not in associated state", __func__, (int)mlme->cmd );
            }
        default:
            hddLog(LOGE,"%s %d Command should be Disassociate/Deauthenticate", __func__, (int)mlme->cmd );
            return -EINVAL;
    }//end of switch
    EXIT();
#endif
    return 0;
//    return status;
}

static int iw_get_ap_rts_threshold(struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   v_U32_t status = 0;

   status = hdd_wlan_get_rts_threshold(pHostapdAdapter, wrqu);

   return status;
}

static int iw_get_ap_frag_threshold(struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_U32_t status = 0;

    status = hdd_wlan_get_frag_threshold(pHostapdAdapter, wrqu);

    return status;
}

static int iw_get_ap_freq(struct net_device *dev, struct iw_request_info *info,
             struct iw_freq *fwrq, char *extra)
{
   v_U32_t status = FALSE, channel = 0, freq = 0;
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   tHalHandle hHal;
   hdd_hostapd_state_t *pHostapdState;
   hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pHostapdAdapter))->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
   }

   pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
   hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);

   if(pHostapdState->bssState == BSS_STOP )
   {
       if (ccmCfgGetInt(hHal, WNI_CFG_CURRENT_CHANNEL, &channel)
                                                  != eHAL_STATUS_SUCCESS)
       {
           return -EIO;
       }
       else
       {
          status = hdd_wlan_get_freq(channel, &freq);
          if( TRUE == status)
          {
              /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
               * iwlist & iwconfig command shows frequency into proper
               * format (2.412 GHz instead of 246.2 MHz)*/
              fwrq->m = freq;
              fwrq->e = MHZ;
          }
       }
    }
    else
    {
       channel = pHddApCtx->operatingChannel;
       status = hdd_wlan_get_freq(channel, &freq);
       if( TRUE == status)
       {
          /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
           * iwlist & iwconfig command shows frequency into proper
           * format (2.412 GHz instead of 246.2 MHz)*/
           fwrq->m = freq;
           fwrq->e = MHZ;
       }
    }
   return 0;
}

static int iw_get_mode(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    int status = 0;

    wrqu->mode = IW_MODE_MASTER;

    return status;
}

static int iw_softap_setwpsie(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
#ifndef WLAN_FEATURE_MBSSID
   v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif
   hdd_hostapd_state_t *pHostapdState;
   eHalStatus halStatus= eHAL_STATUS_SUCCESS;
   u_int8_t *wps_genie;
   u_int8_t *fwps_genie;
   u_int8_t *pos;
   tpSap_WPSIE pSap_WPSIe;
   u_int8_t WPSIeType;
   u_int16_t length;
   struct iw_point s_priv_data;

   ENTER();

   /* helper function to get iwreq_data with compat handling. */
   if (hdd_priv_get_data(&s_priv_data, wrqu)) {
      return -EINVAL;
   }

   if ((NULL == s_priv_data.pointer) ||
       (s_priv_data.length < QCSAP_MAX_WSC_IE)) {
      return -EINVAL;
   }

   wps_genie = mem_alloc_copy_from_user_helper(s_priv_data.pointer,
                                               s_priv_data.length);

   if (NULL == wps_genie) {
       hddLog(LOG1,
              "%s: failed to alloc memory and copy data from user buffer",
              __func__);
       return -EFAULT;
   }

   fwps_genie = wps_genie;

   pSap_WPSIe = vos_mem_malloc(sizeof(tSap_WPSIE));
   if (NULL == pSap_WPSIe)
   {
      hddLog(LOGE, "VOS unable to allocate memory");
      kfree(fwps_genie);
      return -ENOMEM;
   }
   vos_mem_zero(pSap_WPSIe, sizeof(tSap_WPSIE));

   hddLog(LOG1,"%s WPS IE type[0x%X] IE[0x%X], LEN[%d]", __func__, wps_genie[0], wps_genie[1], wps_genie[2]);
   WPSIeType = wps_genie[0];
   if ( wps_genie[0] == eQC_WPS_BEACON_IE)
   {
      pSap_WPSIe->sapWPSIECode = eSAP_WPS_BEACON_IE;
      wps_genie = wps_genie + 1;
      switch ( wps_genie[0] )
      {
         case DOT11F_EID_WPA:
            if (wps_genie[1] < 2 + 4)
            {
               vos_mem_free(pSap_WPSIe);
               kfree(fwps_genie);
               return -EINVAL;
            }
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0)
            {
             hddLog (LOG1, "%s Set WPS BEACON IE(len %d)",__func__, wps_genie[1]+2);
             pos = &wps_genie[6];
             while (((size_t)pos - (size_t)&wps_genie[6])  < (wps_genie[1] - 4) )
             {
                switch((u_int16_t)(*pos<<8) | *(pos+1))
                {
                   case HDD_WPS_ELEM_VERSION:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.Version = *pos;
                      hddLog(LOG1, "WPS version %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.Version);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_VER_PRESENT;
                      pos += 1;
                      break;

                   case HDD_WPS_ELEM_WPS_STATE:
                      pos +=4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.wpsState = *pos;
                      hddLog(LOG1, "WPS State %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.wpsState);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_STATE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_APSETUPLOCK:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.APSetupLocked = *pos;
                      hddLog(LOG1, "AP setup lock %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.APSetupLocked);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_APSETUPLOCK_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_SELECTEDREGISTRA:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistra = *pos;
                      hddLog(LOG1, "Selected Registra %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistra);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_SELECTEDREGISTRA_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_DEVICE_PASSWORD_ID:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.DevicePasswordID = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Password ID: %x", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.DevicePasswordID);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_DEVICEPASSWORDID_PRESENT;
                      pos += 2;
                      break;
                   case HDD_WPS_ELEM_REGISTRA_CONF_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistraCfgMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Select Registra Config Methods: %x", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistraCfgMethod);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT;
                      pos += 2;
                      break;

                   case HDD_WPS_ELEM_UUID_E:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSBeaconIE.UUID_E, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_UUIDE_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_RF_BANDS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.RFBand = *pos;
                      hddLog(LOG1, "RF band: %d", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.RFBand);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_RF_BANDS_PRESENT;
                      pos += 1;
                      break;

                   default:
                      hddLog (LOGW, "UNKNOWN TLV in WPS IE(%x)", (*pos<<8 | *(pos+1)));
                      vos_mem_free(pSap_WPSIe);
                      kfree(fwps_genie);
                      return -EINVAL;
                }
              }
            }
            else {
                 hddLog (LOGE, "%s WPS IE Mismatch %X",
                         __func__, wps_genie[0]);
            }
            break;

         default:
            hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, wps_genie[0]);
            vos_mem_free(pSap_WPSIe);
            kfree(fwps_genie);
            return 0;
      }
    }
    else if( wps_genie[0] == eQC_WPS_PROBE_RSP_IE)
    {
      pSap_WPSIe->sapWPSIECode = eSAP_WPS_PROBE_RSP_IE;
      wps_genie = wps_genie + 1;
      switch ( wps_genie[0] )
      {
         case DOT11F_EID_WPA:
            if (wps_genie[1] < 2 + 4)
            {
               vos_mem_free(pSap_WPSIe);
               kfree(fwps_genie);
               return -EINVAL;
            }
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0)
            {
             hddLog (LOG1, "%s Set WPS PROBE RSP IE(len %d)",__func__, wps_genie[1]+2);
             pos = &wps_genie[6];
             while (((size_t)pos - (size_t)&wps_genie[6])  < (wps_genie[1] - 4) )
             {
              switch((u_int16_t)(*pos<<8) | *(pos+1))
              {
                   case HDD_WPS_ELEM_VERSION:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Version = *pos;
                      hddLog(LOG1, "WPS version %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Version);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_VER_PRESENT;
                      pos += 1;
                      break;

                   case HDD_WPS_ELEM_WPS_STATE:
                      pos +=4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.wpsState = *pos;
                      hddLog(LOG1, "WPS State %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.wpsState);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_STATE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_APSETUPLOCK:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.APSetupLocked = *pos;
                      hddLog(LOG1, "AP setup lock %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.APSetupLocked);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_APSETUPLOCK_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_SELECTEDREGISTRA:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistra = *pos;
                      hddLog(LOG1, "Selected Registra %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistra);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SELECTEDREGISTRA_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_DEVICE_PASSWORD_ID:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DevicePasswordID = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Password ID: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DevicePasswordID);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_DEVICEPASSWORDID_PRESENT;
                      pos += 2;
                      break;
                   case HDD_WPS_ELEM_REGISTRA_CONF_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Select Registra Config Methods: %x", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT;
                      pos += 2;
                      break;
                  case HDD_WPS_ELEM_RSP_TYPE:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ResponseType = *pos;
                      hddLog(LOG1, "Config Methods: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ResponseType);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_RESPONSETYPE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_UUID_E:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.UUID_E, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_UUIDE_PRESENT;
                      pos += length;
                      break;

                   case HDD_WPS_ELEM_MANUFACTURER:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Manufacture.num_name = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Manufacture.name, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MANUFACTURE_PRESENT;
                      pos += length;
                      break;

                   case HDD_WPS_ELEM_MODEL_NAME:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelName.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelName.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MODELNAME_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_MODEL_NUM:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelNumber.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelNumber.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MODELNUMBER_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_SERIAL_NUM:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SerialNumber.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SerialNumber.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SERIALNUMBER_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_PRIMARY_DEVICE_TYPE:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceCategory = (*pos<<8 | *(pos+1));
                      hddLog(LOG1, "primary dev category: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceCategory);
                      pos += 2;

                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceOUI, pos, HDD_WPS_DEVICE_OUI_LEN);
                      hddLog(LOG1, "primary dev oui: %02x, %02x, %02x, %02x", pos[0], pos[1], pos[2], pos[3]);
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceSubCategory = (*pos<<8 | *(pos+1));
                      hddLog(LOG1, "primary dev sub category: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceSubCategory);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT;
                      break;
                   case HDD_WPS_ELEM_DEVICE_NAME:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceName.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceName.text, pos, length);
                      pos += length;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_DEVICENAME_PRESENT;
                      break;
                   case HDD_WPS_ELEM_CONFIG_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ConfigMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Config Methods: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_CONFIGMETHODS_PRESENT;
                      break;

                   case HDD_WPS_ELEM_RF_BANDS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.RFBand = *pos;
                      hddLog(LOG1, "RF band: %d", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.RFBand);
                      pos += 1;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_RF_BANDS_PRESENT;
                      break;
              }  // switch
            }
         }
         else
         {
            hddLog (LOGE, "%s WPS IE Mismatch %X",__func__, wps_genie[0]);
         }

      } // switch
    }

#ifdef WLAN_FEATURE_MBSSID
    halStatus = WLANSAP_Set_WpsIe(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), pSap_WPSIe);
#else
    halStatus = WLANSAP_Set_WpsIe(pVosContext, pSap_WPSIe);
#endif
    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
    if( pHostapdState->bCommit && WPSIeType == eQC_WPS_PROBE_RSP_IE)
    {
        //hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
        //v_CONTEXT_t pVosContext = pHostapdAdapter->pvosContext;
#ifdef WLAN_FEATURE_MBSSID
        WLANSAP_Update_WpsIe ( WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter) );
#else
        WLANSAP_Update_WpsIe ( pVosContext );
#endif
    }

    vos_mem_free(pSap_WPSIe);
    kfree(fwps_genie);
    EXIT();
    return halStatus;
}

static int iw_softap_stopbss(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    hdd_context_t *pHddCtx         = NULL;

    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags))
    {
        hdd_hostapd_state_t *pHostapdState =
                       WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
        vos_event_reset(&pHostapdState->stop_bss_event);
#ifdef WLAN_FEATURE_MBSSID
        status = WLANSAP_StopBss(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
#else
        status = WLANSAP_StopBss((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext);
#endif
        if (VOS_IS_STATUS_SUCCESS(status))
        {
            status = vos_wait_single_event(&pHostapdState->stop_bss_event,
                                           10000);
            if (!VOS_IS_STATUS_SUCCESS(status))
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         ("%s: ERROR: HDD vos wait for single_event failed!!"),
                         __func__);
                VOS_ASSERT(0);
            }
        }
        clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
        wlan_hdd_decr_active_session(pHddCtx, pHostapdAdapter->device_mode);
    }
    EXIT();
    return (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
}

static int iw_softap_version(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));

    ENTER();
    hdd_wlan_get_version(pHostapdAdapter, wrqu, extra);
    EXIT();
    return 0;
}

VOS_STATUS hdd_softap_get_sta_info(hdd_adapter_t *pAdapter, v_U8_t *pBuf, int buf_len)
{
    v_U8_t i;
    v_U8_t maxSta = 0;
    int len = 0;
    const char sta_info_header[] = "staId staAddress";
    hdd_context_t *pHddCtx = (hdd_context_t*)(pAdapter->pHddCtx);

    len = scnprintf(pBuf, buf_len, sta_info_header);
    pBuf += len;
    buf_len -= len;

    if(pHddCtx)
        maxSta = pHddCtx->cfg_ini->maxNumberOfPeers;

    for (i = 0; i <= maxSta; i++)
    {
        if(pAdapter->aStaInfo[i].isUsed)
        {
            len = scnprintf(pBuf, buf_len, "%5d .%02x:%02x:%02x:%02x:%02x:%02x",
                                       pAdapter->aStaInfo[i].ucSTAId,
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[0],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[1],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[2],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[3],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[4],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[5]);
            pBuf += len;
            buf_len -= len;
        }
        if(WE_GET_STA_INFO_SIZE > buf_len)
        {
            break;
        }
    }
    return VOS_STATUS_SUCCESS;
}

static int iw_softap_get_sta_info(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    VOS_STATUS status;
    ENTER();
    status = hdd_softap_get_sta_info(pHostapdAdapter, extra, WE_SAP_MAX_STA_INFO);
    if ( !VOS_IS_STATUS_SUCCESS( status ) ) {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s Failed!!!",__func__);
       return -EINVAL;
    }
    wrqu->data.length = strlen(extra);
    EXIT();
    return 0;
}

static int iw_set_ap_genie(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{

    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
#endif
    eHalStatus halStatus= eHAL_STATUS_SUCCESS;
    u_int8_t *genie = (u_int8_t *)extra;

    ENTER();

    if(!wrqu->data.length)
    {
        EXIT();
        return 0;
    }

    switch (genie[0])
    {
        case DOT11F_EID_WPA:
        case DOT11F_EID_RSN:
            if((WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy == 0)
            {
                hdd_softap_Deregister_BC_STA(pHostapdAdapter);
                hdd_softap_Register_BC_STA(pHostapdAdapter, 1);
            }
            (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = 1;
#ifdef WLAN_FEATURE_MBSSID
            halStatus = WLANSAP_Set_WPARSNIes(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), genie, wrqu->data.length);
#else
            halStatus = WLANSAP_Set_WPARSNIes(pVosContext, genie, wrqu->data.length);
#endif
            break;

        default:
            hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, genie[0]);
            halStatus = 0;
    }

    EXIT();
    return halStatus;
}
#ifdef QCA_WIFI_ISOC
static VOS_STATUS  wlan_hdd_get_classAstats_for_station(hdd_adapter_t *pAdapter, u8 staid)
{
   eHalStatus hstatus;
   long lrc;
   struct statsContext context;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: pAdapter is NULL", __func__);
      return VOS_STATUS_E_FAULT;
   }

   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;
   hstatus = sme_GetStatistics( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                  eCSR_HDD,
                                  SME_GLOBAL_CLASSA_STATS,
                                  hdd_GetClassA_statisticsCB,
                                  0, // not periodic
                                  FALSE, //non-cached results
                                  staid,
                                  &context,
                                  pAdapter->sessionId );
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Unable to retrieve statistics for link speed",
            __func__);
   }
   else
   {
      lrc = wait_for_completion_interruptible_timeout(&context.completion,
            msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
      if (lrc <= 0)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: SME %s while retrieving link speed",
              __func__, (0 == lrc) ? "timeout" : "interrupt");
      }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   return VOS_STATUS_SUCCESS;
}
#endif

VOS_STATUS  wlan_hdd_get_linkspeed_for_peermac(hdd_adapter_t *pAdapter,
                                               tSirMacAddr macAddress)
{
   eHalStatus hstatus;
   long lrc;
   struct linkspeedContext context;
   tSirLinkSpeedInfo *linkspeed_req;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: pAdapter is NULL", __func__);
      return VOS_STATUS_E_FAULT;
   }
   linkspeed_req = (tSirLinkSpeedInfo *)vos_mem_malloc(sizeof(*linkspeed_req));
   if (NULL == linkspeed_req)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s Request Buffer Alloc Fail", __func__);
      return VOS_STATUS_E_INVAL;
   }
   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = LINK_CONTEXT_MAGIC;

   vos_mem_copy(linkspeed_req->peer_macaddr, macAddress, sizeof(tSirMacAddr) );
   hstatus = sme_GetLinkSpeed( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                  linkspeed_req,
                                  &context,
                                  hdd_GetLink_SpeedCB);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Unable to retrieve statistics for link speed",
            __func__);
      vos_mem_free(linkspeed_req);
   }
   else
   {
      lrc = wait_for_completion_interruptible_timeout(&context.completion,
            msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
      if (lrc <= 0)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: SME %s while retrieving link speed",
              __func__, (0 == lrc) ? "timeout" : "interrupt");
      }
   }

   /* either we never sent a request, we sent a request and received a
     response or we sent a request and timed out.  if we never sent a
     request or if we sent a request and got a response, we want to
     clear the magic out of paranoia.  if we timed out there is a
     race condition such that the callback function could be
     executing at the same time we are. of primary concern is if the
     callback function had already verified the "magic" but had not
     yet set the completion variable when a timeout occurred. we
     serialize these activities by invalidating the magic while
     holding a shared spinlock which will cause us to block if the
     callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);
   return VOS_STATUS_SUCCESS;
}


int iw_get_softap_linkspeed(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)

{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   hdd_context_t *pHddCtx;
   char *pLinkSpeed = (char*)extra;
   char *pmacAddress;
   v_U32_t link_speed = 0;
   int len = sizeof(v_U32_t)+1;
   tSirMacAddr macAddress;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   int rc, valid, i;

   pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
   valid = wlan_hdd_validate_context(pHddCtx);

   if (0 != valid)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context not valid"));
       return valid;
   }

   hddLog(VOS_TRACE_LEVEL_INFO, "%s wrqu->data.length= %d\n", __func__, wrqu->data.length);

   if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1)
   {
      pmacAddress = kmalloc(MAC_ADDRESS_STR_LEN, GFP_KERNEL);
      if (NULL == pmacAddress) {
          hddLog(LOG1, "unable to allocate memory");
          return -ENOMEM;
      }
      if (copy_from_user((void *)pmacAddress,
          wrqu->data.pointer, MAC_ADDRESS_STR_LEN))
      {
          hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
          kfree(pmacAddress);
          return -EFAULT;
      }
      pmacAddress[MAC_ADDRESS_STR_LEN] = '\0';

      status = hdd_string_to_hex (pmacAddress, MAC_ADDRESS_STR_LEN, macAddress );
      kfree(pmacAddress);

      if (!VOS_IS_STATUS_SUCCESS(status ))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR, FL("String to Hex conversion Failed"));
      }
   }
   /* If no mac address is passed and/or its length is less than 17,
    * link speed for first connected client will be returned.
    */
   if (wrqu->data.length < 17 || !VOS_IS_STATUS_SUCCESS(status )) {
      for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
          if (pHostapdAdapter->aStaInfo[i].isUsed &&
             (!vos_is_macaddr_broadcast(&pHostapdAdapter->aStaInfo[i].macAddrSTA))) {
             vos_copy_macaddr((v_MACADDR_t *)macAddress,
                               &pHostapdAdapter->aStaInfo[i].macAddrSTA);
             status = VOS_STATUS_SUCCESS;
             break;
          }
      }
   }
   if (!VOS_IS_STATUS_SUCCESS(status )) {
      hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid peer macaddress"));
      return -EINVAL;
   }
   status = wlan_hdd_get_linkspeed_for_peermac(pHostapdAdapter,
                                               macAddress);
   if (!VOS_IS_STATUS_SUCCESS(status ))
   {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Unable to retrieve SME linkspeed"));
        return -EINVAL;
   }

   link_speed = pHostapdAdapter->ls_stats.estLinkSpeed;

   /* linkspeed in units of 500 kbps */
   link_speed = link_speed / 500;
   wrqu->data.length  = len;
   rc = snprintf(pLinkSpeed, len, "%u", link_speed);
   if ((rc < 0) || (rc >= len))
   {
       // encoding or length error?
       hddLog(VOS_TRACE_LEVEL_ERROR,FL("Unable to encode link speed"));
       return -EIO;
   }

   return 0;
}


/**
 * hdd_get_rssi_cb() - get station's rssi callback
 * @sta_rssi: pointer of rssi information
 * @context: get rssi callback context
 *
 * This function will fill rssi information to hostapd
 * adapter
 *
 */
void hdd_get_rssi_cb(struct sir_rssi_resp *sta_rssi, void *context)
{
	struct statsContext *get_rssi_context;
	struct sir_rssi_info *rssi_info;
	uint8_t peer_num;
	int i;
	int buf = 0;
	int length = 0;
	char *rssi_info_output;
	union iwreq_data *wrqu;

	if ((NULL == sta_rssi) || (NULL == context)) {

		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: Bad param, sta_rssi [%p] context [%p]",
			__func__, sta_rssi, context);
		return;
	}

	spin_lock(&hdd_context_lock);
	/*
	 * there is a race condition that exists between this callback
	 * function and the caller since the caller could time out either
	 * before or while this code is executing.  we use a spinlock to
	 * serialize these actions
	 */
	get_rssi_context = context;
	if (RSSI_CONTEXT_MAGIC !=
			get_rssi_context->magic) {

		/*
		 * the caller presumably timed out so there is nothing
		 * we can do
		 */
		spin_unlock(&hdd_context_lock);
		hddLog(VOS_TRACE_LEVEL_WARN,
			"%s: Invalid context, magic [%08x]",
			__func__,
			get_rssi_context->magic);
		return;
	}

	rssi_info_output = get_rssi_context->extra;
	wrqu = get_rssi_context->wrqu;
	peer_num = sta_rssi->count;
	rssi_info = sta_rssi->info;
	get_rssi_context->magic = 0;

	hddLog(LOG1, "%s : %d peers", __func__, peer_num);


	/*
	 * The iwpriv tool default print is before mac addr and rssi.
	 * Add '\n' before first rssi item to align the frist rssi item
	 * with others
	 *
	 * wlan     getRSSI:
	 * [macaddr1] [rssi1]
	 * [macaddr2] [rssi2]
	 * [macaddr3] [rssi3]
	 */
	length = scnprintf((rssi_info_output), WE_MAX_STR_LEN, "\n");
	for (i = 0; i < peer_num; i++) {
		buf = scnprintf
			(
			(rssi_info_output + length), WE_MAX_STR_LEN - length,
			"[%pM] [%d]\n",
			rssi_info[i].peer_macaddr,
			rssi_info[i].rssi
			);
			length += buf;
	}
	wrqu->data.length = length + 1;

	/* notify the caller */
	complete(&get_rssi_context->completion);

	/* serialization is complete */
	spin_unlock(&hdd_context_lock);
}

/**
 * wlan_hdd_get_peer_rssi() - get station's rssi
 * @adapter: hostapd interface
 * @macaddress: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call sme_get_rssi to get rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int  wlan_hdd_get_peer_rssi(hdd_adapter_t *adapter,
					v_MACADDR_t macaddress,
					char *extra,
					union iwreq_data *wrqu)
{
	eHalStatus hstatus;
	int ret;
	struct statsContext context;
	struct sir_rssi_req rssi_req;

	if (NULL == adapter) {
		hddLog(VOS_TRACE_LEVEL_ERROR, "%s: pAdapter is NULL",
			__func__);
		return -EFAULT;
	}

	init_completion(&context.completion);
	context.magic = RSSI_CONTEXT_MAGIC;
	context.extra = extra;
	context.wrqu = wrqu;

	vos_mem_copy(&(rssi_req.peer_macaddr), &macaddress,
				VOS_MAC_ADDR_SIZE);
	rssi_req.sessionId = adapter->sessionId;
	hstatus = sme_get_rssi(WLAN_HDD_GET_HAL_CTX(adapter),
				rssi_req,
				&context,
				hdd_get_rssi_cb);
	if (eHAL_STATUS_SUCCESS != hstatus) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: Unable to retrieve statistics for rssi",
			__func__);
		ret = -EFAULT;
	} else {
		if (!wait_for_completion_timeout(&context.completion,
				msecs_to_jiffies(WLAN_WAIT_TIME_STATS))) {
			hddLog(VOS_TRACE_LEVEL_ERROR,
				"%s: SME timed out while retrieving rssi",
				__func__);
			ret = -EFAULT;
		} else
			ret = 0;
	}
	/*
	 * either we never sent a request, we sent a request and received a
	 * response or we sent a request and timed out.  if we never sent a
	 * request or if we sent a request and got a response, we want to
	 * clear the magic out of paranoia.  if we timed out there is a
	 * race condition such that the callback function could be
	 * executing at the same time we are. of primary concern is if the
	 * callback function had already verified the "magic" but had not
	 * yet set the completion variable when a timeout occurred. we
	 * serialize these activities by invalidating the magic while
	 * holding a shared spinlock which will cause us to block if the
	 * callback is currently executing
	 */
	spin_lock(&hdd_context_lock);
	context.magic = 0;
	spin_unlock(&hdd_context_lock);
	return ret;
}

/**
 * __iw_get_peer_rssi() - get station's rssi
 * @dev: net device
 * @info: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call wlan_hdd_get_peer_rssi
 * to get rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int
__iw_get_peer_rssi(struct net_device *dev, struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *adapter = (netdev_priv(dev));
	hdd_context_t *hddctx;
	char macaddrarray[18];
	v_MACADDR_t macaddress = VOS_MAC_ADDR_BROADCAST_INITIALIZER;
	VOS_STATUS status = VOS_STATUS_E_FAILURE;
	int ret;

	ENTER();

	hddctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hddctx);
	if (0 != ret)
		return ret;

	hddLog(VOS_TRACE_LEVEL_INFO, "%s wrqu->data.length= %d",
			__func__, wrqu->data.length);

	if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1) {

		if (copy_from_user(macaddrarray,
			wrqu->data.pointer, MAC_ADDRESS_STR_LEN - 1)) {

			hddLog(LOG1, "%s: failed to copy data to user buffer",
					__func__);
			return -EFAULT;
		}

		macaddrarray[MAC_ADDRESS_STR_LEN - 1] = '\0';
		hddLog(LOG1, "%s, %s",
				__func__, macaddrarray);

		status = hdd_string_to_hex(macaddrarray,
				MAC_ADDRESS_STR_LEN, macaddress.bytes );

		if (!VOS_IS_STATUS_SUCCESS(status)) {
			hddLog(VOS_TRACE_LEVEL_ERROR,
				FL("String to Hex conversion Failed"));
		}
	}

	return wlan_hdd_get_peer_rssi(adapter, macaddress, extra, wrqu);
}

/**
 * iw_get_peer_rssi() - get station's rssi
 * @dev: net device
 * @info: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call __iw_get_peer_rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int
iw_get_peer_rssi(struct net_device *dev, struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_get_peer_rssi(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

static const iw_handler      hostapd_handler[] =
{
   (iw_handler) NULL,           /* SIOCSIWCOMMIT */
   (iw_handler) NULL,           /* SIOCGIWNAME */
   (iw_handler) NULL,           /* SIOCSIWNWID */
   (iw_handler) NULL,           /* SIOCGIWNWID */
   (iw_handler) NULL,           /* SIOCSIWFREQ */
   (iw_handler) iw_get_ap_freq,    /* SIOCGIWFREQ */
   (iw_handler) NULL,           /* SIOCSIWMODE */
   (iw_handler) iw_get_mode,    /* SIOCGIWMODE */
   (iw_handler) NULL,           /* SIOCSIWSENS */
   (iw_handler) NULL,           /* SIOCGIWSENS */
   (iw_handler) NULL,           /* SIOCSIWRANGE */
   (iw_handler) NULL,           /* SIOCGIWRANGE */
   (iw_handler) NULL,           /* SIOCSIWPRIV */
   (iw_handler) NULL,           /* SIOCGIWPRIV */
   (iw_handler) NULL,           /* SIOCSIWSTATS */
   (iw_handler) NULL,           /* SIOCGIWSTATS */
   (iw_handler) NULL,           /* SIOCSIWSPY */
   (iw_handler) NULL,           /* SIOCGIWSPY */
   (iw_handler) NULL,           /* SIOCSIWTHRSPY */
   (iw_handler) NULL,           /* SIOCGIWTHRSPY */
   (iw_handler) NULL,           /* SIOCSIWAP */
   (iw_handler) NULL,           /* SIOCGIWAP */
   (iw_handler) iw_set_ap_mlme,    /* SIOCSIWMLME */
   (iw_handler) NULL,           /* SIOCGIWAPLIST */
   (iw_handler) NULL,           /* SIOCSIWSCAN */
   (iw_handler) NULL,           /* SIOCGIWSCAN */
   (iw_handler) NULL,           /* SIOCSIWESSID */
   (iw_handler) NULL,           /* SIOCGIWESSID */
   (iw_handler) NULL,           /* SIOCSIWNICKN */
   (iw_handler) NULL,           /* SIOCGIWNICKN */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* SIOCSIWRATE */
   (iw_handler) NULL,           /* SIOCGIWRATE */
   (iw_handler) NULL,           /* SIOCSIWRTS */
   (iw_handler) iw_get_ap_rts_threshold,     /* SIOCGIWRTS */
   (iw_handler) NULL,           /* SIOCSIWFRAG */
   (iw_handler) iw_get_ap_frag_threshold,    /* SIOCGIWFRAG */
   (iw_handler) NULL,           /* SIOCSIWTXPOW */
   (iw_handler) NULL,           /* SIOCGIWTXPOW */
   (iw_handler) NULL,           /* SIOCSIWRETRY */
   (iw_handler) NULL,           /* SIOCGIWRETRY */
   (iw_handler) NULL,           /* SIOCSIWENCODE */
   (iw_handler) NULL,           /* SIOCGIWENCODE */
   (iw_handler) NULL,           /* SIOCSIWPOWER */
   (iw_handler) NULL,           /* SIOCGIWPOWER */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) iw_set_ap_genie,     /* SIOCSIWGENIE */
   (iw_handler) NULL,           /* SIOCGIWGENIE */
   (iw_handler) iw_set_auth_hostap,    /* SIOCSIWAUTH */
   (iw_handler) NULL,           /* SIOCGIWAUTH */
   (iw_handler) iw_set_ap_encodeext,     /* SIOCSIWENCODEEXT */
   (iw_handler) NULL,           /* SIOCGIWENCODEEXT */
   (iw_handler) NULL,           /* SIOCSIWPMKSA */
};

/*
 * Note that the following ioctls were defined with semantics which
 * cannot be handled by the "iwpriv" userspace application and hence
 * they are not included in the hostapd_private_args array
 *     QCSAP_IOCTL_ASSOC_STA_MACADDR
 */

static const struct iw_priv_args hostapd_private_args[] = {
  { QCSAP_IOCTL_SETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam" },
  { QCSAP_IOCTL_SETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
  { QCSAP_PARAM_MAX_ASSOC,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setMaxAssoc" },
   { QCSAP_PARAM_HIDE_SSID,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "hideSSID" },
   { QCSAP_PARAM_SET_MC_RATE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setMcRate" },
   { QCSAP_PARAM_SET_TXRX_FW_STATS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "txrx_fw_stats" },
   { QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setMccLatency" },
   { QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setMccQuota" },
   { QCSAP_PARAM_AUTO_CHANNEL,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setAutoChannel" },
   { QCSAP_PARAM_SET_CHANNEL_CHANGE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setChanChange" },

#ifdef QCA_WIFI_2_0
 /* Sub-cmds DBGLOG specific commands */
    {   QCSAP_DBGLOG_LOG_LEVEL ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_loglevel" },

    {   QCSAP_DBGLOG_VAP_ENABLE ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_vapon" },

    {   QCSAP_DBGLOG_VAP_DISABLE ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_vapoff" },

    {   QCSAP_DBGLOG_MODULE_ENABLE ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_modon" },

    {   QCSAP_DBGLOG_MODULE_DISABLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_modoff" },

    {   QCSAP_DBGLOG_MOD_LOG_LEVEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_mod_loglevel" },

    {   QCSAP_DBGLOG_TYPE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_type" },
    {   QCSAP_DBGLOG_REPORT_ENABLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_report" },
#ifdef DEBUG
    {   QCSAP_FW_CRASH_INJECT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "crash_inject" },
#endif
    {   QCASAP_TXRX_FWSTATS_RESET,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "txrx_fw_st_rst" },
    {   QCSAP_PARAM_RTSCTS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "enablertscts" },

    {   QCASAP_SET_11N_RATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set11NRates" },

    {   QCASAP_SET_VHT_RATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set11ACRates" },

    {   QCASAP_SHORT_GI,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "enable_short_gi" },

    {   QCSAP_SET_AMPDU,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "ampdu" },

    {   QCSAP_SET_AMSDU,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "amsdu" },

    {  QCSAP_GTX_HT_MCS,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxHTMcs" },

    {  QCSAP_GTX_VHT_MCS,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxVHTMcs" },

    {  QCSAP_GTX_USRCFG,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxUsrCfg" },

    {  QCSAP_GTX_THRE,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxThre" },

    {  QCSAP_GTX_MARGIN,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxMargin" },

    {  QCSAP_GTX_STEP,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxStep" },

    {  QCSAP_GTX_MINTPC,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxMinTpc" },

    {  QCSAP_GTX_BWMASK,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxBWMask" },

    { QCSAP_PARAM_CLR_ACL,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
      0,
      "setClearAcl" },

   {  QCSAP_PARAM_ACL_MODE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
      0,
      "setAclMode" },

#ifdef QCA_PKT_PROTO_TRACE
    {   QCASAP_SET_DEBUG_LOG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setDbgLvl" },
#endif /* QCA_PKT_PROTO_TRACE */

    {   QCASAP_SET_TM_LEVEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTmLevel" },

    {   QCASAP_SET_DFS_IGNORE_CAC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setDfsIgnoreCAC" },

    {   QCASAP_SET_DFS_NOL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setdfsnol" },

    {   QCASAP_SET_DFS_TARGET_CHNL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setNextChnl" },

    {   QCASAP_SET_RADAR_CMD,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setRadar" },

#endif /* QCA_WIFI_2_0 */

  { QCSAP_IOCTL_GETPARAM, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getparam" },
  { QCSAP_IOCTL_GETPARAM, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "" },
  { QCSAP_PARAM_MAX_ASSOC, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getMaxAssoc" },
  { QCSAP_PARAM_GET_WLAN_DBG, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getwlandbg" },
  { QCSAP_PARAM_AUTO_CHANNEL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getAutoChannel" },
  { QCSAP_GTX_BWMASK, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxBWMask" },
  { QCSAP_GTX_MINTPC, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxMinTpc" },
  { QCSAP_GTX_STEP, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxStep" },
  { QCSAP_GTX_MARGIN, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxMargin" },
  { QCSAP_GTX_THRE, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxThre" },
  { QCSAP_GTX_USRCFG, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxUsrCfg" },
  { QCSAP_GTX_VHT_MCS, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxVHTMcs" },
  { QCSAP_GTX_HT_MCS, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_gtxHTMcs" },
  { QCASAP_SHORT_GI, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_short_gi" },
  { QCSAP_PARAM_RTSCTS, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_rtscts" },
  { QCASAP_GET_DFS_NOL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getdfsnol" },
  { QCSAP_GET_ACL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_acl_list" },
#ifdef WLAN_FEATURE_TSF
  { QCSAP_CAP_TSF, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "cap_tsf" },
#endif
  { QCSAP_IOCTL_SET_NONE_GET_THREE, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,    "" },
#ifdef WLAN_FEATURE_TSF
  { QCSAP_GET_TSF, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,    "get_tsf" },
#endif
  { QCSAP_IOCTL_GET_STAWPAIE,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "get_staWPAIE" },
  { QCSAP_IOCTL_SETWPAIE,
      IW_PRIV_TYPE_BYTE | QCSAP_MAX_WSC_IE | IW_PRIV_SIZE_FIXED, 0, "setwpaie" },
  { QCSAP_IOCTL_STOPBSS,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED, 0, "stopbss" },
  { QCSAP_IOCTL_VERSION, 0,
      IW_PRIV_TYPE_CHAR | QCSAP_MAX_WSC_IE, "version" },
  { QCSAP_IOCTL_GET_STA_INFO, 0,
      IW_PRIV_TYPE_CHAR | WE_SAP_MAX_STA_INFO, "get_sta_info" },
  { QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES,
      IW_PRIV_TYPE_BYTE | sizeof(sQcSapreq_WPSPBCProbeReqIES_t) | IW_PRIV_SIZE_FIXED, 0, "getProbeReqIEs" },
  { QCSAP_IOCTL_GET_CHANNEL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getchannel" },
  { QCSAP_IOCTL_DISASSOC_STA,
        IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 6 , 0, "disassoc_sta" },
  { QCSAP_IOCTL_AP_STATS,
        IW_PRIV_TYPE_BYTE | QCSAP_MAX_WSC_IE, 0, "ap_stats" },
  { QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED,
        IW_PRIV_TYPE_CHAR | 18,
        IW_PRIV_TYPE_CHAR | 5, "getLinkSpeed" },
  { QCSAP_IOCTL_PRIV_GET_RSSI,
        IW_PRIV_TYPE_CHAR | 18,
        IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "getRSSI" },
  { QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "" },
   /* handlers for sub-ioctl */
   {   WE_SET_WLAN_DBG,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
       0,
       "setwlandbg" },

   /* handlers for main ioctl */
   {   QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0,
       "" },

   /* handlers for sub-ioctl */
   {   WE_LOG_DUMP_CMD,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0,
       "dump" },
   {   WE_P2P_NOA_CMD,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0,
       "SetP2pPs" },
     /* handlers for sub ioctl */
    {
        WE_MCC_CONFIG_CREDENTIAL,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setMccCrdnl" },

     /* handlers for sub ioctl */
    {
        WE_MCC_CONFIG_PARAMS,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setMccConfig" },

#ifdef MEMORY_DEBUG
    /* handlers for sub ioctl */
    {   WE_MEM_TRACE_DUMP,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "memTraceLog" },
#endif

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_MODIFY_ACL,
        IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 8,
        0,
        "modify_acl" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_GET_CHANNEL_LIST,
        0,
        IW_PRIV_TYPE_BYTE | sizeof(tChannelListInfo),
        "getChannelList" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_TX_POWER,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxPower" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_MAX_TX_POWER,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxMaxPower" },

    {   QCSAP_IOCTL_DATAPATH_SNAP_SHOT,
        IW_PRIV_TYPE_NONE | IW_PRIV_TYPE_NONE,
        0,
        "dataSnapshot" },

    /* Set HDD CFG Ini param */
    {   QCSAP_IOCTL_SET_INI_CFG,
        IW_PRIV_TYPE_CHAR | QCSAP_IOCTL_MAX_STR_LEN,
        0,
        "setConfig" },

    /* Get HDD CFG Ini param */
    {   QCSAP_IOCTL_GET_INI_CFG,
        0,
        IW_PRIV_TYPE_CHAR | QCSAP_IOCTL_MAX_STR_LEN,
        "getConfig" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_TRAFFIC_MONITOR,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTrafficMon" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_WOWL_CONFIG_PTRN,
        IW_PRIV_TYPE_CHAR | 512,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_WOWL_ADD_PTRN,
        IW_PRIV_TYPE_CHAR | 512,
        0,
        "wowlAddPtrn" },

    {   WE_WOWL_DEL_PTRN,
        IW_PRIV_TYPE_CHAR | 512,
        0,
        "wowlDelPtrn" },
};

static const iw_handler hostapd_private[] = {
   [QCSAP_IOCTL_SETPARAM - SIOCIWFIRSTPRIV] = iw_softap_setparam,  //set priv ioctl
   [QCSAP_IOCTL_GETPARAM - SIOCIWFIRSTPRIV] = iw_softap_getparam,  //get priv ioctl
   [QCSAP_IOCTL_SET_NONE_GET_THREE - SIOCIWFIRSTPRIV] = iw_softap_get_three,
   [QCSAP_IOCTL_GET_STAWPAIE - SIOCIWFIRSTPRIV] = iw_get_genie, //get station genIE
   [QCSAP_IOCTL_SETWPAIE - SIOCIWFIRSTPRIV] = iw_softap_setwpsie,
   [QCSAP_IOCTL_STOPBSS - SIOCIWFIRSTPRIV] = iw_softap_stopbss,       // stop bss
   [QCSAP_IOCTL_VERSION - SIOCIWFIRSTPRIV] = iw_softap_version,       // get driver version
   [QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES - SIOCIWFIRSTPRIV] = iw_get_WPSPBCProbeReqIEs,
   [QCSAP_IOCTL_GET_CHANNEL - SIOCIWFIRSTPRIV] = iw_softap_getchannel,
   [QCSAP_IOCTL_ASSOC_STA_MACADDR - SIOCIWFIRSTPRIV] = iw_softap_getassoc_stamacaddr,
   [QCSAP_IOCTL_DISASSOC_STA - SIOCIWFIRSTPRIV] = iw_softap_disassoc_sta,
   [QCSAP_IOCTL_AP_STATS - SIOCIWFIRSTPRIV] = iw_softap_ap_stats,
   [QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE - SIOCIWFIRSTPRIV]  = iw_set_three_ints_getnone,
   [QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE - SIOCIWFIRSTPRIV]     = iw_set_var_ints_getnone,
   [QCSAP_IOCTL_SET_CHANNEL_RANGE - SIOCIWFIRSTPRIV] = iw_softap_set_channel_range,
   [QCSAP_IOCTL_MODIFY_ACL - SIOCIWFIRSTPRIV]   = iw_softap_modify_acl,
   [QCSAP_IOCTL_GET_CHANNEL_LIST - SIOCIWFIRSTPRIV]   = iw_softap_get_channel_list,
   [QCSAP_IOCTL_GET_STA_INFO - SIOCIWFIRSTPRIV] = iw_softap_get_sta_info,
   [QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED - SIOCIWFIRSTPRIV]     = iw_get_softap_linkspeed,
   [QCSAP_IOCTL_PRIV_GET_RSSI - SIOCIWFIRSTPRIV] = iw_get_peer_rssi,
   [QCSAP_IOCTL_SET_TX_POWER - SIOCIWFIRSTPRIV]   = iw_softap_set_tx_power,
   [QCSAP_IOCTL_SET_MAX_TX_POWER - SIOCIWFIRSTPRIV]   = iw_softap_set_max_tx_power,
   [QCSAP_IOCTL_DATAPATH_SNAP_SHOT - SIOCIWFIRSTPRIV]  =   iw_display_data_path_snapshot,
   [QCSAP_IOCTL_SET_INI_CFG - SIOCIWFIRSTPRIV]  =  iw_softap_set_ini_cfg,
   [QCSAP_IOCTL_GET_INI_CFG - SIOCIWFIRSTPRIV]  =  iw_softap_get_ini_cfg,
   [QCSAP_IOCTL_SET_TRAFFIC_MONITOR - SIOCIWFIRSTPRIV]  =  iw_softap_set_trafficmonitor,
   [QCSAP_IOCTL_WOWL_CONFIG_PTRN - SIOCIWFIRSTPRIV] = iw_softap_wowl_config_pattern,
};
const struct iw_handler_def hostapd_handler_def = {
   .num_standard     = sizeof(hostapd_handler) / sizeof(hostapd_handler[0]),
#ifdef CONFIG_WEXT_PRIV
   .num_private      = sizeof(hostapd_private) / sizeof(hostapd_private[0]),
   .num_private_args = sizeof(hostapd_private_args) / sizeof(hostapd_private_args[0]),
   .private          = (iw_handler *)hostapd_private,
   .private_args     = hostapd_private_args,
#endif
   .standard         = (iw_handler *)hostapd_handler,
   .get_wireless_stats = NULL,
};

struct net_device_ops net_ops_struct  = {
    .ndo_open = hdd_hostapd_open,
    .ndo_stop = hdd_hostapd_stop,
    .ndo_uninit = hdd_hostapd_uninit,
    .ndo_start_xmit = hdd_softap_hard_start_xmit,
    .ndo_tx_timeout = hdd_softap_tx_timeout,
    .ndo_get_stats = hdd_softap_stats,
    .ndo_set_mac_address = hdd_hostapd_set_mac_address,
    .ndo_do_ioctl = hdd_hostapd_ioctl,
    .ndo_change_mtu = hdd_hostapd_change_mtu,
    .ndo_select_queue = hdd_hostapd_select_queue,
 };

int hdd_set_hostapd(hdd_adapter_t *pAdapter)
{
    return VOS_STATUS_SUCCESS;
}

void hdd_set_ap_ops( struct net_device *pWlanHostapdDev )
{
  pWlanHostapdDev->netdev_ops = &net_ops_struct;
}

VOS_STATUS hdd_init_ap_mode( hdd_adapter_t *pAdapter )
{
    hdd_hostapd_state_t * phostapdBuf;
#ifdef CONFIG_WIRELESS_EXT
    struct net_device *dev = pAdapter->dev;
#endif
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_STATUS status;
#ifdef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
    v_CONTEXT_t sapContext=NULL;
#endif
    int ret;

    ENTER();

    hdd_set_sap_auth_offload(pAdapter, TRUE);

    ret = hdd_set_client_block_info(pAdapter);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: set client block info failed %d",
            __func__, ret);
    }

#ifdef WLAN_FEATURE_MBSSID
    sapContext = WLANSAP_Open(pVosContext);
    if (sapContext == NULL)
    {
          VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: WLANSAP_Open failed!!"));
          return VOS_STATUS_E_FAULT;
    }

    pAdapter->sessionCtx.ap.sapContext = sapContext;

    status = WLANSAP_Start(sapContext);
    if ( ! VOS_IS_STATUS_SUCCESS( status ) )
    {
          VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: WLANSAP_Start failed!!"));
          WLANSAP_Close(sapContext);
          return status;
    }
#endif

    // Allocate the Wireless Extensions state structure
    phostapdBuf = WLAN_HDD_GET_HOSTAP_STATE_PTR( pAdapter );

    sme_SetCurrDeviceMode(pHddCtx->hHal, pAdapter->device_mode);

    // Zero the memory.  This zeros the profile structure.
    memset(phostapdBuf, 0,sizeof(hdd_hostapd_state_t));

    // Set up the pointer to the Wireless Extensions state structure
    // NOP
    status = hdd_set_hostapd(pAdapter);
    if(!VOS_IS_STATUS_SUCCESS(status)) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,("ERROR: hdd_set_hostapd failed!!"));
#ifdef WLAN_FEATURE_MBSSID
         WLANSAP_Close(sapContext);
#endif
         return status;
    }

    status = vos_event_init(&phostapdBuf->vosEvent);
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: Hostapd HDD vos event init failed!!"));
#ifdef WLAN_FEATURE_MBSSID
         WLANSAP_Close(sapContext);
#endif
         return status;
    }

    status = vos_event_init(&phostapdBuf->stop_bss_event);
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD,
                   VOS_TRACE_LEVEL_ERROR,
                   "ERROR: Hostapd HDD stop bss event init failed!!");
#ifdef WLAN_FEATURE_MBSSID
         WLANSAP_Close(sapContext);
#endif
         return status;
    }

    init_completion(&pAdapter->session_close_comp_var);
    init_completion(&pAdapter->session_open_comp_var);

    sema_init(&(WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->semWpsPBCOverlapInd, 1);

#ifdef CONFIG_WIRELESS_EXT
     // Register as a wireless device
    dev->wireless_handlers = (struct iw_handler_def *)& hostapd_handler_def;
#endif

    //Initialize the data path module
    status = hdd_softap_init_tx_rx(pAdapter);
    if ( !VOS_IS_STATUS_SUCCESS( status ))
    {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_softap_init_tx_rx failed", __func__);
    }

    status = hdd_wmm_adapter_init( pAdapter );
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
             "hdd_wmm_adapter_init() failed with status code %08d [x%08x]",
                             status, status );
       goto error_wmm_init;
    }

    set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

    ret = process_wma_set_command((int)pAdapter->sessionId,
                         (int)WMI_PDEV_PARAM_BURST_ENABLE,
                         (int)pHddCtx->cfg_ini->enableSifsBurst,
                         PDEV_CMD);

    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: WMI_PDEV_PARAM_BURST_ENABLE set failed %d",
                    __func__, ret);
    }

    wlan_hdd_set_monitor_tx_adapter( WLAN_HDD_GET_CTX(pAdapter), pAdapter );

    return status;

error_wmm_init:
    hdd_softap_deinit_tx_rx( pAdapter );
#ifdef WLAN_FEATURE_MBSSID
    WLANSAP_Close(sapContext);
#endif
    EXIT();
    return status;
}

hdd_adapter_t* hdd_wlan_create_ap_dev( hdd_context_t *pHddCtx, tSirMacAddr macAddr, tANI_U8 *iface_name )
{
    struct net_device *pWlanHostapdDev = NULL;
    hdd_adapter_t *pHostapdAdapter = NULL;
    v_CONTEXT_t pVosContext= NULL;

   hddLog(VOS_TRACE_LEVEL_DEBUG, "%s: iface_name = %s", __func__, iface_name);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0))
    pWlanHostapdDev = alloc_netdev_mq(sizeof(hdd_adapter_t), iface_name, ether_setup, NUM_TX_QUEUES);
#else
    pWlanHostapdDev = alloc_netdev_mq(sizeof(hdd_adapter_t), iface_name, NET_NAME_UNKNOWN, ether_setup, NUM_TX_QUEUES);
#endif
    if (pWlanHostapdDev != NULL)
    {
        pHostapdAdapter = netdev_priv(pWlanHostapdDev);

        //Init the net_device structure
        ether_setup(pWlanHostapdDev);

        //Initialize the adapter context to zeros.
        vos_mem_zero(pHostapdAdapter, sizeof( hdd_adapter_t ));
        pHostapdAdapter->dev = pWlanHostapdDev;
        pHostapdAdapter->pHddCtx = pHddCtx;
        pHostapdAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;

        hddLog(VOS_TRACE_LEVEL_DEBUG, "%s: pWlanHostapdDev = %p, "
                                      "pHostapdAdapter = %p, "
                                      "concurrency_mode=0x%x", __func__,
                                      pWlanHostapdDev,
                                      pHostapdAdapter,
                                      (int)vos_get_concurrency_mode());

        //Get the Global VOSS context.
        pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
        //Save the adapter context in global context for future.
        ((VosContextType*)(pVosContext))->pHDDSoftAPContext = (v_VOID_t*)pHostapdAdapter;

        //Init the net_device structure
        strlcpy(pWlanHostapdDev->name, (const char *)iface_name, IFNAMSIZ);

        hdd_set_ap_ops( pHostapdAdapter->dev );

        pWlanHostapdDev->watchdog_timeo = HDD_TX_TIMEOUT;
        pWlanHostapdDev->mtu = HDD_DEFAULT_MTU;

        vos_mem_copy(pWlanHostapdDev->dev_addr, (void *)macAddr,sizeof(tSirMacAddr));
        vos_mem_copy(pHostapdAdapter->macAddressCurrent.bytes, (void *)macAddr, sizeof(tSirMacAddr));

        pHostapdAdapter->offloads_configured = FALSE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0))
        pWlanHostapdDev->needs_free_netdev = true;
#else
        pWlanHostapdDev->destructor = free_netdev;
#endif
        pWlanHostapdDev->ieee80211_ptr = &pHostapdAdapter->wdev ;
        pHostapdAdapter->wdev.wiphy = pHddCtx->wiphy;
        pHostapdAdapter->wdev.netdev =  pWlanHostapdDev;
        init_completion(&pHostapdAdapter->tx_action_cnf_event);
        init_completion(&pHostapdAdapter->cancel_rem_on_chan_var);
        init_completion(&pHostapdAdapter->rem_on_chan_ready_event);
        init_completion(&pHostapdAdapter->ula_complete);
        init_completion(&pHostapdAdapter->offchannel_tx_event);
        init_completion(&pHostapdAdapter->scan_info.scan_req_completion_event);
        init_completion(&pHostapdAdapter->scan_info.abortscan_event_var);
        vos_event_init(&pHostapdAdapter->scan_info.scan_finished_event);
        pHostapdAdapter->scan_info.scan_pending_option = WEXT_SCAN_PENDING_GIVEUP;

        SET_NETDEV_DEV(pWlanHostapdDev, pHddCtx->parent_dev);
    }
    return pHostapdAdapter;
}

VOS_STATUS hdd_register_hostapd( hdd_adapter_t *pAdapter, tANI_U8 rtnl_lock_held )
{
   struct net_device *dev = pAdapter->dev;
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   ENTER();

   if( rtnl_lock_held )
   {
     if (strnchr(dev->name, strlen(dev->name), '%')) {
         if( dev_alloc_name(dev, dev->name) < 0 )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s:Failed:dev_alloc_name", __func__);
            return VOS_STATUS_E_FAILURE;
         }
      }
      if (register_netdevice(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s:Failed:register_netdevice", __func__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   else
   {
      if (register_netdev(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed:register_netdev", __func__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   set_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags);

   EXIT();
   return status;
}

VOS_STATUS hdd_unregister_hostapd(hdd_adapter_t *pAdapter)
{
#ifdef WLAN_FEATURE_MBSSID
   VOS_STATUS status;
   v_PVOID_t sapContext=WLAN_HDD_GET_SAP_CTX_PTR(pAdapter);
#endif

   ENTER();

   hdd_softap_deinit_tx_rx(pAdapter);

#ifdef CONFIG_WIRELESS_EXT
   /* if we are being called during driver unload, then the dev has already
      been invalidated.  if we are being called at other times, then we can
      detach the wireless device handlers */
   if (pAdapter->dev)
   {
      pAdapter->dev->wireless_handlers = NULL;
   }
#endif

#ifdef WLAN_FEATURE_MBSSID
   status = WLANSAP_Stop(sapContext);
   if ( ! VOS_IS_STATUS_SUCCESS( status ) ) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed:WLANSAP_Stop", __func__);
   }

   status = WLANSAP_Close(sapContext);
   if ( ! VOS_IS_STATUS_SUCCESS( status ) ) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed:WLANSAP_close", __func__);
   }
   pAdapter->sessionCtx.ap.sapContext = NULL;
#endif

   EXIT();
   return 0;
}
