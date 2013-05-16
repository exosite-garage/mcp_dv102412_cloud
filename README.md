============================================
About Microchip DV102412 Exosite Cloud Demo
============================================
This project is for Microchip DV102412 using a Microchip WiFi MRF24WG0MA module to demo Exosite's cloud<br>
connectivity to send and receive data to/from Exosite's Cloud Data Platform.<br>

Project functionality includes:<br>
1) New device activation on Exosite cloud and save CIK to NVMemory.<br>
2) Periodically sends 'Heart Beat' data to draw a triangle wave to Exosite cloud.<br>
3) Sends button press report to Exosite cloud to trigger an email alert.<br>
4) Reads 'LED' data source on Exosite cloud to turn device LED1 on and off.<br>

License is BSD, Copyright 2013, Exosite LLC (see LICENSE file)

Tested and developed on IDE MPLAB X IDE v1.80 for DV102412.<br>

========================================
Validate Firmware and Hardware Versions
========================================
1) Firmware - based on Microchip library "microchip_solutions_v2013-02-15" and TCP/IP Stack version "v5.42.06 Feb 2013".
NOTE: Find version information in "Microchip/TCPIP Stack/TCPIP Stack Version.txt".
2) Hardware - Microchip DV102412, and WiFi module MRF24WG0MA 124145D.

========================================
Quick Start
========================================
1) Install MPLAB XC32 Compiler v1.21.<br>
http://www.microchip.com/mplabxc32windows
2) Install MPLAB X IDE v1.80 for DV102412.<br>
http://www.microchip.com/mplabx-ide-windows-installer <br>
3) Download the latest project repository from "https://github.com/exosite-garage/mcp_dv102412_cloud.git", or download it from "git commands".<br>
4) Extract the project and open the project folder "mcp_dv102412_cloud/TCPIP/WiFiGDemo/WiFiGDemoBoard" with MPLAB X.<br>
5) Confirm power switch is ON and AAA batteries are installed, then plug in the PICKit3 and USB cable to your PC.<br>
6) Compile the project and download the program to the board (Run -> Run Project). This process might take several minutes.<br>
7) Use a smart phone or laptop that can select the WiFi Access Point labeled "MCHP_G_****" to provision device AP.<br>
8) Open http://192.168.1.3, to load Configure Network page, click "Display Scan Results", select your SSID, input your passphrase and save.<br>
NOTE: If you see a "Future home of Mchp error notification." message during your AP setup, you should reboot your device and scan for APs again.
9) Turn your device off and then back on. Make sure that LED2 blinks 4 times to signify that the board has the correct firmware version.<br>
NOTE: View 'Device Status' section below for more information about LED messages.
10) When LED0 blinks rapidly for a couple of seconds, the WiFi Access Point has been set successfully.<br>
11) Create a free account on microchip.exosite.com.<br>
12) Activate your account.<br>
13) Log in, and click the "Add Device" link from the home page.<br>
14) Add a "DV102412 - Wi-Fi G Demo Board" device and input the MAC address shown on the sticker of Microchip Wifi module.<br>
15) Wait up to 40 seconds for the device to activate on your account.<br>
16) Click the device on the "Your Devices" widget.<br>
17) Control the LED using the On/Off switch widget.<br>
18) When the module is connected and activated, the device will send a "ping" value periodically.<br>

========================================
Device Status
========================================
LED0
- Blinks continually = WiFi set successfully

LED2 - device bootup
- Blinks 2 times = WiFi module firmware version is 0x30.
- Blinks 4 times = WiFi module firmware version is 0x31.
- Blinks 6 times = WiFi module firmware version is unknown.

LED2 - runtime
- Blinks 1 time continually = Read, write and provision successful
- Blinks 2 times continually = No internet connection
- Blinks 3 times continually = Device not found on cloud
- Blinks 4 times continually = CIK invalid

========================================
Release Info
========================================
----------------------------------------
Release 2013-05-16
----------------------------------------
--) initial release<br>

