/*****************************************************************************
*
*  exosite_hal.c - Exosite hardware & environmenat adapation layer.
*  Copyright (C) 2012 Exosite LLC
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
*    Neither the name of Exosite LLC nor the names of its contributors may
*    be used to endorse or promote products derived from this software
*    without specific prior written permission.
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
#include "exosite.h"
#include "exosite_hal.h"
#include "exosite_meta.h"
#include "TCPIPConfig.h"
#include "TCPIP Stack/TCPIP.h"
#include "TCPIP Stack/Helpers.h"

// local variables
#define EXOMETA_ADDR 0xbd050000
TCP_SOCKET exSocket = INVALID_SOCKET;
int wait_count = 0;
int socket_crush = 0;
int send_count = 0;
static enum _GenericTCPState
{
  EX_HOME = 0,
  EX_SOCKET_OBTAINED,
  EX_PACKAGE_SEND,
  EX_PROCESS_RESPONSE,
  EX_DISCONNECT,
  EX_DONE
} GenericTCPState = EX_DONE;

// local functions

// externs
extern APP_CONFIG AppConfig;

// global variables

/*****************************************************************************
*
*  exoHAL_ReadUUID
*
*  \param  if_nbr - Interface Number (1 - WiFi)
*          UUID_buf - buffer to return hexadecimal MAC
*
*  \return 0 if failure; len of UUID if success;
*
*  \brief  Reads the MAC address from the hardware
*
*****************************************************************************/
int
exoHAL_ReadUUID(unsigned char if_nbr, char * UUID_buf)
{
  int retval = 0;
  MAC_ADDR macbuf;
  unsigned char macstr[20];

  switch (if_nbr) {
    case IF_GPRS:
      break;
    case IF_ENET:
      break;
    case IF_WIFI:
      WF_GetMacAddress(macbuf.v);
      sprintf((char *)macstr,"%02X%02X%02X%02X%02X%02X", macbuf.v[0], macbuf.v[1], macbuf.v[2], macbuf.v[3], macbuf.v[4], macbuf.v[5]);
      retval = strlen((char *)macstr);
      memcpy(UUID_buf, macstr, retval);
      UUID_buf[retval] = 0;
      break;
    default:
      break;
  }

  return retval;
}


/*****************************************************************************
*
* exoHAL_EnableNVMeta
*
*  \param  None
*
*  \return None
*
*  \brief  Enables meta non-volatile memory, if any
*
*****************************************************************************/
void
exoHAL_EnableMeta(void)
{
  return;
}


/*****************************************************************************
*
*  exoHAL_EraseNVMeta
*
*  \param  None
*
*  \return None
*
*  \brief  Wipes out meta information - replaces with 0's
*
*****************************************************************************/
void
exoHAL_EraseMeta(void)
{
  NVMErasePage((void *)EXOMETA_ADDR);

  return;
}


/*****************************************************************************
*
*  exoHAL_WriteMetaItem
*
*  \param  buffer - string buffer containing info to write to meta; len -
*          size of string in bytes; offset - offset from base of meta
*          location to store the item
*
*  \return None
*
*  \brief  Stores information to the NV meta structure
*
*****************************************************************************/
void
exoHAL_WriteMetaItem(unsigned char * buffer, unsigned char len, int offset)
{
  int i;
  DWORD address = EXOMETA_ADDR + offset;
  DWORD input_tmp;

  if (len % 2 > 0)
    len += 1;
  for(i = 0; i < len;)
  {
    input_tmp = (buffer[i + 3] << 24 & 0xff000000) | 
                (buffer[i + 2] << 16 & 0xff0000) |
                (buffer[i + 1] << 8 & 0xff00) |
                (buffer[i] & 0xff);
    i += 4;
    NVMWriteWord((void *)(address), input_tmp);
    address += 4;
  }

  return;
}


/*****************************************************************************
*
*  exoHAL_ReadMetaItem
*
*  \param  buffer - buffer we can read meta info into; len - size of the
*          buffer (max 256 bytes); offset - offset from base of meta to begin
*          reading from;
*
*  \return None
*
*  \brief  Reads information from the NV meta structure
*
*****************************************************************************/
void
exoHAL_ReadMetaItem(unsigned char * buffer, unsigned char len, int offset)
{
  DWORD address = EXOMETA_ADDR + offset;
  memcpy(buffer, (int *)(address), len);

  return;
}


/*****************************************************************************
*
*  exoHAL_SocketClose
*
*  \param  socket - socket handle
*
*  \return None
*
*  \brief  Closes a socket
*
*****************************************************************************/
void
exoHAL_SocketClose(long socket)
{
  // Send everything immediately
  if (GenericTCPState == EX_DISCONNECT)
  {
    if (TCPIsConnected((TCP_SOCKET)exSocket) && socket_crush == 1)
    {
      TCPClose((TCP_SOCKET)exSocket);
      socket_crush = 0;
      exSocket = INVALID_SOCKET;
      socket = exSocket;
    }
    send_count = 0;
    GenericTCPState = EX_DONE;
  }
  return;
}


/*****************************************************************************
*
*  exoHAL_SocketOpenTCP
*
*  \param  server - server's ip
*
*  \return -1: failure; Other: socket handle
*
*  \brief  Opens a TCP socket
*
*****************************************************************************/
long
exoHAL_SocketOpenTCP(unsigned char *server)
{
  DWORD serverip = 0;
  int server_port = server[5] & 0xff;

  serverip = (server[3] << 24 & 0xff000000) | (server[2] << 16 & 0xff0000)
          | (server[1] << 8 & 0xff00) | (server[0] & 0xff);

  if (GenericTCPState == EX_DONE)
    GenericTCPState = EX_HOME;

  if (GenericTCPState == EX_HOME)
  {
    if (exSocket == INVALID_SOCKET)
    {
      exSocket = TCPOpen(serverip, TCP_OPEN_IP_ADDRESS, server_port, TCP_PURPOSE_GENERIC_TCP_CLIENT);
      if (exSocket == INVALID_SOCKET)
      {
        return -1;
      }
    }
    GenericTCPState = EX_SOCKET_OBTAINED;
  }
  return (long)exSocket;
}


/*****************************************************************************
*
*  exoHAL_ServerConnect
*
*  \param  None
*
*  \return socket - socket handle
*
*  \brief  The function opens a TCP socket
*
*****************************************************************************/
long
exoHAL_ServerConnect(long sock)
{
  if (GenericTCPState == EX_SOCKET_OBTAINED)
  {
    // Wait for the remote server to accept our connection request
    if (!TCPIsConnected(sock))
    {
      wait_count++;
      if (wait_count > 10)
      {
        socket_crush = 1;
        GenericTCPState = EX_DISCONNECT;
        wait_count = 0;
        return -1;
      }
    }
    GenericTCPState = EX_PACKAGE_SEND;
  }

  return (long)sock;
}

/*****************************************************************************
*
*  exoHAL_SocketSend
*
*  \param  socket - socket handle; buffer - string buffer containing info to
*          send; len - size of string in bytes;
*
*  \return Number of bytes sent
*
*  \brief  Sends data out to the internet
*
*****************************************************************************/
unsigned char
exoHAL_SocketSend(long socket, char * buffer, unsigned char len)
{
  int send_len = 0;

  if (GenericTCPState == EX_PACKAGE_SEND)
  {
    if (TCPIsPutReady((TCP_SOCKET)exSocket) < len)
        return -1;
    send_len = TCPPutArray((TCP_SOCKET)exSocket, (BYTE *)buffer, len);
    send_count ++;
    wait_count = 0;
    socket = (long)exSocket;
  }

  return send_len;
}


/*****************************************************************************
*
*  exoHAL_SocketRecv
*
*  \param  socket - socket handle; buffer - string buffer to put info we
*          receive; len - size of buffer in bytes;
*
*  \return Number of bytes received
*
*  \brief  Receives data from the internet
*
*****************************************************************************/
unsigned char
exoHAL_SocketRecv(long socket, char * buffer, unsigned char len)
{
  WORD w, wGetLen;

  if (GenericTCPState == EX_PACKAGE_SEND && send_count >= 4)
  {
    TCPFlush((TCP_SOCKET)exSocket);
    GenericTCPState++;
  }

  if (GenericTCPState == EX_PROCESS_RESPONSE)
  {
    if (!TCPIsConnected(socket))
    {
      return 0;
    }

    w = TCPIsGetReady((TCP_SOCKET)exSocket);
    if (!w)
        return 0;
    buffer[0] = 0;
    if (w)
    {
      wGetLen = w;
      TCPGetArray((TCP_SOCKET)exSocket, (BYTE *)buffer, len);
      if (send_count >= 4 && w < len)
      {
        GenericTCPState = EX_DISCONNECT;
      }
      socket = (long)exSocket;
      return len;
    }
  }

  return 0;
}

