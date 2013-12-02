/******************************************************************************

 MRF24W Driver 
 Module for Microchip TCP/IP Stack
  -Provides access to MRF24W WiFi controller
  -Reference: MRF24W Data sheet, IEEE 802.11 Standard

*******************************************************************************
 FileName:      WFEasyConfig.c
 Dependencies:  TCP/IP Stack header files
 Processor:     PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 Compiler:      Microchip C32 v1.10b or higher
                Microchip C30 v3.22 or higher
                Microchip C18 v3.34 or higher
 Company:       Microchip Technology, Inc.

 Software License Agreement

 Copyright (C) 2002-2010 Microchip Technology Inc.  All rights reserved.

 Microchip licenses to you the right to use, modify, copy, and distribute:
 (i)  the Software when embedded on a Microchip microcontroller or digital 
      signal controller product ("Device") which is integrated into 
      Licensee's product; or
 (ii) ONLY the Software driver source files ENC28J60.c, ENC28J60.h,
      ENCX24J600.c and ENCX24J600.h ported to a non-Microchip device used in 
      conjunction with a Microchip ethernet controller for the sole purpose 
      of interfacing with the ethernet controller.

 You should refer to the license agreement accompanying this Software for 
 additional information regarding your rights and obligations.

 THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
 KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 NON-INFRINGEMENT. IN NO EVENT SHALL MICROCHIP BE LIABLE FOR ANY INCIDENTAL,
 SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST
 OF PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY OR SERVICES, ANY CLAIMS BY
 THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), ANY CLAIMS
 FOR INDEMNITY OR CONTRIBUTION, OR OTHER SIMILAR COSTS, WHETHER ASSERTED ON
 THE BASIS OF CONTRACT, TORT (INCLUDING NEGLIGENCE), BREACH OF WARRANTY, OR
 OTHERWISE.


 Author             Date        Comment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Steve Collmeyer    24 Sep 2009 Initial
 Brad Rex           09 Feb 2010 Update for MRF24WB
******************************************************************************/
#include "HardwareProfile.h"
#include "TCPIPConfig.h"

#if defined(WF_CS_TRIS)

#include <string.h> /* for memcpy */

#include "TCPIP Stack/WFEasyConfig.h"
#include "TCPIP Stack/WFApi.h"
#include "TCPIP Stack/WFConsole.h"

#if defined ( EZ_CONFIG_SCAN )
tWFScanCtx  g_ScanCtx;
#endif /* EZ_CONFIG_SCAN */

#if defined(STACK_USE_EZ_CONFIG)
/* Easy Config Globals */
extern UINT8 ConnectionProfileID;

tWFEasyConfigCtx g_easyConfigCtx;

#if (MY_DEFAULT_NETWORK_TYPE == WF_SOFT_AP)	|| defined(WF_PRE_SCAN_IN_ADHOC)		
tWFScanResult preScanResult[50];      //WF_PRESCAN  May change struct later for memory optimization
#endif

#if (MY_DEFAULT_NETWORK_TYPE == WF_SOFT_AP)
extern tWFHibernate WF_hibernate;
#endif

/* Easy Config Private Functions */
static int WFEasyConfigProcess(void);

void WFEasyConfigInit()
{
    CFGCXT.ssid[0] = 0;
    CFGCXT.security = WF_SECURITY_OPEN;
    CFGCXT.key[0] = 0;
    CFGCXT.defaultWepKey = WF_WEP_KEY_INVALID;
    CFGCXT.type = WF_INFRASTRUCTURE;
    CFGCXT.cfg_state = cfg_stopped;
    CFGCXT.isWifiNeedToConfigure = 0;

    #if defined (EZ_CONFIG_STORE)
    CFGCXT.isWifiDoneConfigure = AppConfig.dataValid;
    #endif
    return;
}

void WFEasyConfigMgr()
{
    if (CFGCXT.isWifiNeedToConfigure) {
        if (WFEasyConfigProcess()) {
            //Has been configured, clear flag
            CFGCXT.isWifiNeedToConfigure = 0;
            CFGCXT.isWifiDoneConfigure = 1;
        }
    }
    return;
}

static int WFEasyConfigProcess(void)
{
    UINT8 ConnectionProfileID;
    UINT8 ConnectionState;
    
    #if defined (EZ_CONFIG_STALL)
    if (CFGCXT.cfg_state == cfg_stopped)
    {
        /* State machine just started get current time stamp */
        CFGCXT.cfg_state = cfg_stalled;
        CFGCXT.timeStart = TickGet();
        return 0;
    }
    
    /* Wait for stall time to expire */
    if (CFGCXT.cfg_state == cfg_stalled)
    {
        UINT32 time = TickGet();
        if ((time - CFGCXT.timeStart) < WF_STALL_TIME_MS)
            return 0;
    }
    
    #endif //EZ_CONFIG_STALL
  
    /* We will re-use the current profile */
    WF_CMGetConnectionState(&ConnectionState, &ConnectionProfileID);

    /* Need to disconnect */
    WF_CMDisconnect();

    /* Delete profile */
    WF_CPDelete(ConnectionProfileID);

    /* Create and prepare new profile */
    WF_CPCreate(&ConnectionProfileID);

    /* Now set the various connection profile parameters */

    /* Set SSID... */
    if (CFGCXT.ssid)
#if defined(__18CXX)
        WF_CPSetSsid(ConnectionProfileID, 
            //(ROM char *)CFGCXT.ssid,   Note (VMH): fixed compile warning - not sure why this is necessary.
            CFGCXT.ssid, 
            strlen(CFGCXT.ssid));  
#else
        WF_CPSetSsid(ConnectionProfileID, 
            CFGCXT.ssid, 
            strlen((char*)CFGCXT.ssid));  
#endif       

#if defined(DERIVE_KEY_FROM_PASSPHRASE_IN_HOST)
        if ((BYTE)CFGCXT.security == WF_SECURITY_WPA_WITH_PASS_PHRASE
            || (BYTE)CFGCXT.security == WF_SECURITY_WPA2_WITH_PASS_PHRASE
            || (BYTE)CFGCXT.security == WF_SECURITY_WPA_AUTO_WITH_PASS_PHRASE) 
        {
            WF_ConvPassphrase2Key(strlen((char *)CFGCXT.key), CFGCXT.key, strlen((char*)CFGCXT.ssid), CFGCXT.ssid);
            CFGCXT.security--;
        }
#endif    /* defined(DERIVE_KEY_FROM_PASSPHRASE_IN_HOST) */

    /* Now deal with security... */
    switch ((BYTE)CFGCXT.security) {
        case WF_SECURITY_OPEN: /* No security */
            WF_CPSetSecurity(ConnectionProfileID, WF_SECURITY_OPEN, 0, 0, 0);
            break; 

        case WF_SECURITY_WPA_AUTO_WITH_PASS_PHRASE:
            if (CFGCXT.key) {
                WF_CPSetSecurity(ConnectionProfileID, WF_SECURITY_WPA_AUTO_WITH_PASS_PHRASE, 0, CFGCXT.key, strlen((char *)CFGCXT.key));
            }
            break;

        case WF_SECURITY_WPA_AUTO_WITH_KEY:
            if (CFGCXT.key) {
                WF_CPSetSecurity(ConnectionProfileID, WF_SECURITY_WPA_AUTO_WITH_KEY, 0, CFGCXT.key, 32);
            }
            break;

        case WF_SECURITY_WEP_40:
            {
                BYTE  keys[20];
                int   i;

                if (CFGCXT.key) {
                    /* Clear key */
                    for (i = 0; i < 20; i++)
                        keys[i] = 0;
                    memcpy(keys, (void*)CFGCXT.key, 20);
                    WF_CPSetSecurity(ConnectionProfileID, WF_SECURITY_WEP_40, CFGCXT.defaultWepKey, keys, 20);
                }
            }
            break;

        case WF_SECURITY_WEP_104:
            {
                BYTE  keys[52];
                int   i;

                if (CFGCXT.key) {
                    /* Clear key */
                    for (i = 0; i < 52; i++)
                        keys[i] = 0;
                    memcpy(keys, (void*)CFGCXT.key, 52);
                    WF_CPSetSecurity(ConnectionProfileID, WF_SECURITY_WEP_104, CFGCXT.defaultWepKey, keys, 52);
                }
            }
            break;
    }
 
    #if defined (EZ_CONFIG_STORE)
    SaveAppConfig(&AppConfig);
    #endif

    /* Set wlan mode */
    WF_CPSetNetworkType(ConnectionProfileID, CFGCXT.type);

    if (AppConfig.networkType == WF_INFRASTRUCTURE) 
        WF_CASetListRetryCount(MY_DEFAULT_LIST_RETRY_COUNT_INFRASTRUCTURE);
    
#if defined(DISABLE_MODULE_FW_CONNECT_MANAGER_IN_INFRASTRUCTURE)
        WF_DisableModuleConnectionManager();
#endif
    
#if MY_DEFAULT_NETWORK_TYPE == WF_SOFT_AP
    // SoftAP: To allow redirection, need to hibernate before changing network type. Module FW has SoftAP flag and therefore hibernate mode
    //             is needed to clear this indication and allow proper network change. 
    //             This should work for non-SoftAP. But these have not been tested yet.

    // Operation of hibernate mode
    //      Turned off LDO of the MRF24W module, which is turning off the power completely. Has same effect of resetting the module.
    //
    
    // FW flow of hibernate mode: 
    // Ensure WF_USE_POWER_SAVE_FUNCTIONS is enabled.
    // In main() loop, StackTask() -> MACProcess() will be called. It will invoke CheckHibernate(), which executes/handles 
    // hibernate mode based on WF_hibernate.state and  WF_hibernate.wakeup_notice.
    WF_hibernate.state = WF_HB_ENTER_SLEEP;
    WF_hibernate.wakeup_notice = FALSE;
    //WFConsolePrintRomStr("SoftAP redirection: Put Wi-Fi module into hibernate mode.", TRUE);

    DelayMs(50);  // SOFTAP_ZEROCONF_SUPPORT. Timing reduced from 200 to 50. 

    WF_hibernate.wakeup_notice = TRUE;
    //WFConsolePrintRomStr("Wakeup Wi-Fi module.", TRUE);
#else    
    /* Kick off connection now... */
    WF_CMConnect(ConnectionProfileID);
#endif

    /* Change state and return TRUE to show we are done! */
    CFGCXT.cfg_state = cfg_stopped;

    return 1;
}
#endif /* STACK_USE_EZ_CONFIG */

#if defined ( EZ_CONFIG_SCAN )
void WFInitScan(void)
{
    SCANCXT.scanState = 0;
    SCANCXT.numScanResults = 0;
    SCANCXT.displayIdx = 0;

    return;
}

//UINT8 state_SavedBeforeScan = 0, ID_SavedBeforeScan = 0; 

UINT16 WFStartScan(void)
{
    // save the state and ID
//    WF_CMCheckConnectionState(&state_SavedBeforeScan, &ID_SavedBeforeScan);

    /* If scan already in progress bail out */
    if (IS_SCAN_IN_PROGRESS(SCANCXT.scanState)) 
        return WF_ERROR_OPERATION_CANCELLED;
   
    if (WF_Scan(WF_SCAN_ALL) != WF_SUCCESS)
        return WF_ERROR_OPERATION_CANCELLED;

    SCAN_SET_IN_PROGRESS(SCANCXT.scanState);
 
    return WF_SUCCESS;
}

UINT16 WFRetrieveScanResult(UINT8 Idx, tWFScanResult *p_ScanResult)
{
    if (Idx >= SCANCXT.numScanResults)
        return WF_ERROR_INVALID_PARAM;

    WF_ScanGetResult(Idx, p_ScanResult);
    if (p_ScanResult->ssidLen < WF_MAX_SSID_LENGTH)
        p_ScanResult->ssid[p_ScanResult->ssidLen] = 0; /* Terminate */
   // if (p_ScanResult->ssidLen == WF_MAX_SSID_LENGTH)
   //     p_ScanResult->ssidLen -= 1;
    return WF_SUCCESS;
}

void WFScanEventHandler(UINT16 scanResults)
{
    /* Cache number APs found in scan. Max 60 */
    SCANCXT.numScanResults = scanResults;

    /* Clear the scan in progress */
    SCAN_CLEAR_IN_PROGRESS(SCANCXT.scanState);
    SCAN_SET_VALID(SCANCXT.scanState);

    return;
}
#endif /* EZ_CONFIG_SCAN */

//#if defined ( WF_CONSOLE ) && defined ( EZ_CONFIG_SCAN ) && !defined(__18CXX)
#if defined ( EZ_CONFIG_SCAN ) && !defined(__18CXX)
#if defined(WF_PRE_SCAN_IN_ADHOC)

void WFGetScanResults(void)
{
	int    id,counts;

	if (SCANCXT.numScanResults == 0)
	   return;

	if (IS_SCAN_IN_PROGRESS(SCANCXT.scanState))
	   return;

	if (!IS_SCAN_STATE_VALID(SCANCXT.scanState))
	   return;

	counts = SCANCXT.numScanResults>50? 50:SCANCXT.numScanResults;
	for(id = 0;id<SCANCXT.numScanResults;id++)
	{
		WFRetrieveScanResult(id, &preScanResult[id]);
	}
	SCAN_CLEAR_DISPLAY(SCANCXT.scanState);
	return;
}
#endif // #if defined(WF_PRE_SCAN_IN_ADHOC)
void WFDisplayScanMgr()
{
    tWFScanResult   bssDesc;
    char ssid[WF_MAX_SSID_LENGTH+1];
    char rssiChan[48];
    int    i;
    char    st[80];

    if (SCANCXT.numScanResults == 0)
       return;
    if (!IS_SCAN_STATE_DISPLAY(SCANCXT.scanState))
       return;

    if (IS_SCAN_IN_PROGRESS(SCANCXT.scanState))
       return;

    if (!IS_SCAN_STATE_VALID(SCANCXT.scanState))
       return;

    WFRetrieveScanResult(SCANCXT.displayIdx, &bssDesc);
    sprintf(st,"%3d ",SCANCXT.displayIdx);
    putrsUART(st);

    if (bssDesc.bssType == 1)
        sprintf(st,"NetType: Infra.");
    else if (bssDesc.bssType == 2)
        sprintf(st,"NetType: Ad-hoc");
    putrsUART(st);
    
    sprintf(st,", ESSID:");
    putrsUART(st);

    /* Display SSID */
    for(i=0;i<bssDesc.ssidLen;i++) ssid[i] = bssDesc.ssid[i];
    ssid[bssDesc.ssidLen] = 0;
    putsUART(ssid);
    putrsUART("\r\n");

    /* Display SSID  & Channel */
#ifdef STACK_USE_CERTIFICATE_DEBUG	
	sprintf(rssiChan, "\tRSSI: %3u, Channel: %2u", bssDesc.rssi, bssDesc.channel);
        putsUART(rssiChan);
	/* Display BSSID */
	sprintf(rssiChan, ", BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
				 bssDesc.bssid[0],bssDesc.bssid[1],bssDesc.bssid[2],
				 bssDesc.bssid[3],bssDesc.bssid[4],bssDesc.bssid[5]);
	putsUART(rssiChan);
	/* Display Security Mode */
	if((bssDesc.apConfig & 0x10) == 0)	  // bit4==0:	open (no security)
	{
		sprintf(rssiChan, ", SecMode: %s\r\n", "Open");
	}
	else	// bit4== 1:	security
	{			 
		if ((bssDesc.apConfig & 0x80) == 0x80) // bit7 ==  1: WPA2
		{
			sprintf(rssiChan, ", SecMode: %s\r\n", "WPA2");
		}
		else if((bssDesc.apConfig & 0x40) == 0x40)//bit6==1: WPA
		{
			sprintf(rssiChan, ", SecMode: %s\r\n", "WPA");
		}
		else	// bit7==0, bit6 ==0, WEP
		{
			sprintf(rssiChan, ", SecMode: %s\r\n", "WEP");
		}
	}
	putsUART(rssiChan);
#else
	sprintf(rssiChan, ", RSSI: %u, Channel: %u\r\n", bssDesc.rssi, bssDesc.channel);
        putsUART(rssiChan);
#endif

#if (MY_DEFAULT_NETWORK_TYPE == WF_SOFT_AP) 		
	preScanResult[SCANCXT.displayIdx]= bssDesc;    // WF_PRESCAN
#endif

    if (++SCANCXT.displayIdx == SCANCXT.numScanResults)  {
        SCAN_CLEAR_DISPLAY(SCANCXT.scanState);
        SCANCXT.displayIdx = 0;
#if defined(WF_CONSOLE) & defined(STACK_USE_UART)
        WFConsoleReleaseConsoleMsg();
#endif
    }

    return;
}
#endif /* WF_CONSOLE */

#endif /* WF_CS_TRIS */
