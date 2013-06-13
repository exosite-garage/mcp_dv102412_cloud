
#ifndef _EXOSITEDEMO_H
#define _EXOSITEDEMO_H
enum {
    CLOUD_HOME,
    CLOUD_READING,
    CLOUD_WRITING,
    CLOUD_BUTTON_STATUS_UPDATE,
    CLOUD_CUSTOMIZE,
    CLOUD_END
};

typedef struct {
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

int ping = 0;  // global ping value
int button_state = 0;  // monitor button status
int nvmram_verify = 0;  // before you write data into internal flash, you should erase flash
int once_init = 0;  // confirm firmware once
int setup_wifi = 1;  // recover the WiFi setting after default WiFi ready
int led2_delay_count = 0; // global value to count the number of led blinks
DWORD cloudstatus_time_tick = 0; // global value to record the cloud status indicate time tick
DWORD demo_delay_time_tick = 0; // global value to record the demo task time tick.
DWORD led2_time_tick = 0;  // global value to record the LED2 time tick
int latest_exo_code = EXO_STATUS_END; // records Exosite latest status
int demo_delay_count = 0; // global value to record the demo delay count
int cloud_status = 0; // helps for swicth the next cloud status
int once_update = 1;  // overwrite push button status on cloud on push button release
int badcik = 0; // CIK is invalid
int wifi_fail_count = 0; // counts the WiFi fail times
int tcp_fail_count = 0; // counts the TCP fail times


// global functions
void Store_App_Config(void);
void Load_App_Config(void);
void Erase_App_Config(void);

#endif // _EXOSITEDEMO_H

