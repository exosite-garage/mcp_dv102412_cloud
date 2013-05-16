/*****************************************************************************
*
*  exosite.c - Exosite cloud communications.
*  Copyright (C) 2013 Exosite LLC
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*****************************************************************************/
#include "../../exosite/exosite.h"
#include "MainDemo.h"

//local defines
int ping = 0;  // global ping value
int button_state = 0;  // monitor the button status
int once_update = 1;  // When you push button, you still need to recover the old status of button to cloud
int Command_Reading = 0;  // handle the flow for the Command reading, heart beat writing and button status
int HeartBeat_posting = 0; // handle the flow for the Command reading, heart beat writing and button status
int nvmram_verify = 0;  // before you write data into internal flash, you should erase flash
int dhcp_renew = 1;  // record DHCP need to renew ip? or not, if dhcp is renew ip, it'll send new local ip to cloud
int once_init = 0;  // it's remember the firmware detect times, but we hope that is once
int rw_fail_cnt = 0;  // Counting the Read/Write fails
int cik_broken = 0;  // maintain the CIK status for application layer
int setup_wifi = 1;  // Recover the WiFi setting after default WiFi ready, then we don't need to setup too many set
int delay_count = 0; // global counting value for handle to count the blinking times
int blinking_times = 0;  // global value for user define the blinking times of indicate cloud status
int delay_timming = 0; // global value for user define the delay timming of indicate cloud status
int Demo_Status_Changed = 1; // global value for monitor the cloud status
DWORD exostatus_time_tick = 0;  // Time tick for WiFi setting and LED2 blinking
DWORD exobutton_time_tick = 0;  // Help to handle the Button status
DWORD exowrite_time_tick = 0;   // Help to handle the timing gap of Exosite_Write
DWORD exostatus_handler_tick = 0;  // for handle the cloud status indicate timing

typedef struct{
  BYTE MySSID[32];             // Wireless SSID (if using MRF24W)
  BYTE SsidLength;             // number of bytes in SSID
  BYTE SecurityMode;           // WF_SECURITY_OPEN or one of the other security modes
  BYTE SecurityKey[64];        // WiFi Security key, or passphrase.
  BYTE SecurityKeyLength;      // number of bytes in security key (can be 0)
  BYTE WepKeyIndex;            // WEP key index (only valid for WEP)
  BYTE dataValid;
  BYTE networkType;
  BYTE saveSecurityInfo;       // Save 32-byte PSK
} _store_app_temp;

// local functions

// global functions
void Store_App_Config(void);
void Load_App_Config(void);

// externs

// global variables


/*****************************************************************************
*
*  button_monitor
*
*  \param  None
*
*  \return  1 = working, 0 = not working
*
*  \brief  Monitors the button status
*
*****************************************************************************/
int button_monitor(void)
{
  if (button_state == 1)
  {
    char inbuf_01[50];
    sprintf(inbuf_01, "push_button=%d",button_state);
    if(Exosite_Write(inbuf_01,strlen(inbuf_01)))
    {
      once_update = 1;
      button_state = 0;
      exobutton_time_tick = TickGet();
      rw_fail_cnt = 0;
      cik_broken = 0;
    }
    else
    {
      rw_fail_cnt++;
      if (rw_fail_cnt > 100)
        cik_broken = 1;
    }
    return 1;
  }

  if (button_state == 0 && once_update == 1)
  {
    char inbuf_02[50];
    if (TickGet() - exobutton_time_tick > TICK_SECOND / 4)
      exobutton_time_tick = TickGet();
    else
      return 1;

    sprintf(inbuf_02, "push_button=%d",button_state);
    if(Exosite_Write(inbuf_02,strlen(inbuf_02)))
    {
      once_update = 0;
      rw_fail_cnt = 0;
      cik_broken = 0;
    }
    else
    {
      rw_fail_cnt++;
      if (rw_fail_cnt > 100)
        cik_broken = 1;

      return 1;
    }
  }

  return 0;
}


/*****************************************************************************
*
*  read_command
*
*  \param  None
*
*  \return  1 = Read command done, 0 = Reading ...
*
*  \brief  Reads command from cloud
*
*****************************************************************************/
int read_command(void)
{
  char pbuf[50];

  // Read LED status from cloud
  if (HeartBeat_posting == 0)
  {
    Command_Reading++;
    if(Exosite_Read("led", pbuf, 200))
    {
      if (!strncmp(pbuf, "0", 1)) 
        LED1_IO = 0;
      else if (!strncmp(pbuf, "1", 1)) 
        LED1_IO = 1;

      rw_fail_cnt = 0;
      cik_broken = 0;

      if (Command_Reading > 5)
        Command_Reading = 0;
    }
    else
    {
      rw_fail_cnt++;
      if (rw_fail_cnt > 100)
        cik_broken = 1;
    }
  }

  if (Command_Reading == 0)
    return 1;

  return 0;
}


/*****************************************************************************
*
*  heart_beat_report
*
*  \param  None
*
*  \return  None
*
*  \brief  Post ping value to cloud
*
*****************************************************************************/
void heart_beat_report(void)
{
  char str_heartbeat[50];

  if (Command_Reading == 0)
  {
    if (TickGet() - exowrite_time_tick > TICK_SECOND)
      exowrite_time_tick = TickGet();
    else
      return;

    HeartBeat_posting = 1;
    sprintf(str_heartbeat, "ping=%d",ping);
    if(Exosite_Write(str_heartbeat,strlen(str_heartbeat)))
    {
      ping++;
      if (ping == 100)
        ping = 0;

      HeartBeat_posting = 0;
      rw_fail_cnt = 0;
      cik_broken = 0;
    }
    else
    {
      rw_fail_cnt++;
      if (rw_fail_cnt > 100)
        cik_broken = 1;
    }
  }

  return;
}


/*****************************************************************************
*
*  update_dev_ip
*
*  \param  None
*
*  \return  0 - successful, 1 - failure
*
*  \brief  Update the DHCP IP of device
*
*****************************************************************************/
int update_dev_ip(void)
{
  char str_ip[50];

  sprintf(str_ip, "local_ip=%d.%d.%d.%d",AppConfig.MyIPAddr.v[0], AppConfig.MyIPAddr.v[1], AppConfig.MyIPAddr.v[2], AppConfig.MyIPAddr.v[3]);
  if(Exosite_Write(str_ip,strlen(str_ip)))
  {
    rw_fail_cnt = 0;
    cik_broken = 0;
    return 0;
  }
  else
    rw_fail_cnt++;

  if (rw_fail_cnt > 100)
    cik_broken = 1;
  return 1;
}


/*****************************************************************************
*
*  status_light_handler
*
*  \param  int count, int delay
*
*  \return  1 - count end, 0 - counting
*
*  \brief  handles the led2
*
*****************************************************************************/
int status_light_handler(int count, int delay)
{
  if (Demo_Status_Changed == 0)  return 1;

  if (delay == 0)
    delay = 5;

  if (TickGet() - exostatus_time_tick > TICK_SECOND / delay)
  {
    exostatus_time_tick = TickGet();
    if (delay_count < (count * 2))
    {
      LED2_INV();
      delay_count++;
    }
  }

  if (delay_count >= (count * 2))
  {
    delay_count = 0;
    LED2_IO = 0;
    Demo_Status_Changed = 0;
    return 1;
  }

  return 0;
}


/*****************************************************************************
*
*  show_exo_status
*
*  \param  None
*
*  \return  1 - count end, 0 - counting
*
*  \brief  Indicate Exosite status
*
*****************************************************************************/
void show_exo_status(void)
{
  int exo_code = EXO_STATUS_END;
  if (TickGet() - exostatus_handler_tick > TICK_SECOND)
  {
    exostatus_handler_tick = TickGet();
    if (Demo_Status_Changed == 0)
      exo_code = Exosite_StatusCode();
  }

  if (EXO_STATUS_OK == exo_code)
  {
    Demo_Status_Changed = 1;
    blinking_times = 1;
    delay_timming = 0;
  }
  else if (EXO_STATUS_BAD_TCP == exo_code)
  {
    Demo_Status_Changed = 1;
    blinking_times = 2;
    delay_timming = 0;
  }
  else if (EXO_STATUS_BAD_SN == exo_code)
  {
    Demo_Status_Changed = 1;
    blinking_times = 3;
    delay_timming = 0;
  }
  else if (EXO_STATUS_BAD_CIK == exo_code || EXO_STATUS_NOAUTH == exo_code)
  {
    Demo_Status_Changed = 1;
    blinking_times = 4;
    delay_timming = 0;
  }

  if (1 == Demo_Status_Changed)
    status_light_handler(blinking_times, delay_timming);

  return;
}


/*****************************************************************************
*
*  Exosite_Service
*
*  \param  None
*
*  \return  None
*
*  \brief  Exosite major service and handles the cloud work sequence
*
*****************************************************************************/
void Exosite_Service(void)
{
  int code = Exosite_StatusCode();
  if (EXO_STATUS_OK == code || EXO_STATUS_BAD_TCP == code)
  {
    if (dhcp_renew == 1)
    {
      dhcp_renew = update_dev_ip();
    }
    else
    {
      // when you want to provision again, you should verify the nvram
      nvmram_verify = 1;
      if (!button_monitor())
      {
        heart_beat_report();
        read_command();
      }
    }
  }
  else if (EXO_STATUS_BAD_CIK == code || EXO_STATUS_NOAUTH == code || cik_broken == 1)
  {
    if (nvmram_verify == 1)
    {
      Exosite_Init("microchip","dv102412",IF_WIFI, 1);
      nvmram_verify = 0;
      dhcp_renew = 1;
    } else {
      if(!Exosite_Activate())
        cik_broken = 1;
      else
        cik_broken = 0;
    }
  }

  show_exo_status();

  return;
}


/*****************************************************************************
*
*  wifi_fw_detect
*
*  \param  None
*
*  \return  1 - WiFi Firmware check done, 0 - WiFi Firmware check not done
*
*  \brief  Check and indicate the WiFi Firmware version.
*
*****************************************************************************/
int wifi_fw_detect(void)
{
#if defined(MRF24WG)
  if (1 == Demo_Status_Changed && once_init == 0)
  {
    tWFDeviceInfo deviceInfo;
    int state = 0;
    WF_GetDeviceInfo(&deviceInfo);  // only call this once, not continually
    if (deviceInfo.romVersion == 0x30)
      state = status_light_handler(2, 0);
    else if (deviceInfo.romVersion == 0x31)
      state = status_light_handler(4, 0);
    else
      state = status_light_handler(6, 0);
    if (state == 1)
    {
      Demo_Status_Changed = 0;
      once_init = 1;
    }
  }
#endif

  return once_init;
}


/*****************************************************************************
*
*  Store_App_Config
*
*  \param  None
*
*  \return  None
*
*  \brief  Stores the AppConfig into internal Flash
*
*****************************************************************************/
void Store_App_Config(void)
{
  _store_app_temp app_temp;
  _store_app_temp app_temp_verify;
  memcpy(app_temp.MySSID, AppConfig.MySSID, 32);
  memcpy(app_temp.SecurityKey, AppConfig.SecurityKey, 64);
  app_temp.SecurityKeyLength = AppConfig.SecurityKeyLength;
  app_temp.SecurityMode = AppConfig.SecurityMode;
  app_temp.dataValid = AppConfig.dataValid;
  app_temp.networkType = AppConfig.networkType;
  app_temp.saveSecurityInfo = AppConfig.saveSecurityInfo;

  Exosite_GetMRF((char *)&app_temp_verify, sizeof(_store_app_temp));
  if (strncmp((char *)app_temp_verify.MySSID, (char *)app_temp.MySSID, 32) &&
     strncmp((char *)app_temp_verify.SecurityKey, (char *)app_temp.SecurityKey, 64))
  {
    char tmpCIK[40];
    if (Exosite_GetCIK(tmpCIK))
    {
      Exosite_Init("microchip","dv102412",IF_WIFI, 1);
      Exosite_SetCIK(tmpCIK);
    }
    Exosite_SetMRF((char *)&app_temp, sizeof(_store_app_temp));
  }
  return;
}


/*****************************************************************************
*
*  Load_App_Config
*
*  \param  None
*
*  \return  None
*
*  \brief  Copies the AppConfig from internal Flash
*
*****************************************************************************/
void Load_App_Config(void)
{
  _store_app_temp temp;

  Exosite_GetMRF((char *)&temp, sizeof(_store_app_temp));
  if (temp.networkType == CFG_WF_INFRASTRUCTURE)
  {
    memcpy(AppConfig.MySSID, temp.MySSID, 32);
    memcpy(AppConfig.SecurityKey, temp.SecurityKey, 64);
    AppConfig.SecurityKeyLength = temp.SecurityKeyLength;
    AppConfig.SecurityMode = temp.SecurityMode;
    AppConfig.dataValid = temp.dataValid;
    AppConfig.networkType = temp.networkType;
    AppConfig.saveSecurityInfo = 0;
    CFGCXT.security = temp.SecurityMode;
    memcpy(CFGCXT.key, temp.SecurityKey, 64);
    memcpy(CFGCXT.ssid, temp.MySSID, 32);
    CFGCXT.type = temp.networkType;
    CFGCXT.cfg_state = cfg_stopped;

    WF_START_EASY_CONFIG();
  }

  return;
}


/*****************************************************************************
*
*  Exosite_DEMO
*
*  \param  None
*
*  \return  None
*
*  \brief  Main function of Exosite Demo code
*
*****************************************************************************/
void Exosite_DEMO(void)
{
  int fw_check_done = 0;
  fw_check_done = wifi_fw_detect();

  if (!(WFisConnected() == TRUE))
  {
    dhcp_renew = 1;
    return;
  } else {
    if (setup_wifi == 1)
    {
      if (TickGet() - exostatus_time_tick > TICK_SECOND * 2)
      {
        exostatus_time_tick = TickGet();
        Load_App_Config();
        setup_wifi = 0;
      }
    }
  }

  if (DHCPIsBound(0))
  {
    Exosite_Service();
  }
}

