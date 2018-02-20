/** @file
*   @brief Asynchronous event handler code. */
/*******************************************************************************
* Copyright (c) 2014, Alan Barr
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "cc3000_api.h"
#include "../hci.h"

#if 0
/** @brief Holds uint32_t values of the DHCP information. 
 *  @details May be a more convient way of accesing the information
 *           that TI stores in tNetappDhcpParams. */
typedef struct {
    uint32_t aucIp;
    uint32_t aucSubnetMask;
    uint32_t aucDefaultGateway;
    uint32_t aucDHCPServer;
    uint32_t aucDNSServer;
} netappDchpParamsInts;
#endif

/** @brief There is a status byte after the DHCP information in the data
 *         buffer. */
#define DHCP_INFO_STATUS_BYTE            sizeof(tNetappDhcpParams)

/** @brief Length of DHCP information and status byte. */
#define DHCP_INFO_LENGTH_STATUS         (sizeof(tNetappDhcpParams) + 1)

/** @brief Asynchronous information provided by the CC3000.
 *  @details This is updated whenever the asynchronous callback is fired.
 *           Its elements will require to be manually cleared in some 
 *           circumstances to ensure the information is still relevant. */
volatile cc3000AsynchronousData cc3000AsyncData;

extern int cc3000_handle_socket(int32_t sockvalue, int32_t replvalue);

/** @brief Asynchronous callback function.
 *  @details This function is registed to the host driver via wlan_start().
 *           It updates #cc3000AsyncData as required.
 *  @param eventType See TI doxygen API for sWlanCB parameter of wlan_init().
 *  @param data  See TI doxygen API doxygen API for sWlanCB parameter of wlan_init().
 *  @param length See TI doxygen API sWlanCB parameter of wlan_init().*/
void CC3000AsyncCb(long eventType, char * data, unsigned char length)
{
    (void)length;

    //debug("event %i\r\n",eventType);
    if (eventType == HCI_EVNT_WLAN_KEEPALIVE)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_KEEPALIVE", NULL);
    }

    else if (eventType == HCI_EVNT_WLAN_ASYNC_SIMPLE_CONFIG_DONE)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_ASYNC_SIMPLE_CONFIG_DONE", NULL);
        cc3000AsyncData.smartConfigFinished = TRUE;
    }

    else if (eventType == HCI_EVNT_WLAN_UNSOL_INIT)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_UNSOL_INIT", NULL);
    }

    else if (eventType == HCI_EVNT_WLAN_TX_COMPLETE)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_TX_COMPLETE", NULL);
    }

    else if (eventType == HCI_EVNT_WLAN_UNSOL_CONNECT)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_UNSOL_CONNECT", NULL);
        cc3000AsyncData.connected = TRUE;
    }
    
    else if (eventType == HCI_EVNT_WLAN_ASYNC_PING_REPORT)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_ASYNC_PING_REPORT",  NULL);
        if (length == sizeof(cc3000AsyncData.ping.report))
        {
            memcpy((void*)&cc3000AsyncData.ping.report, data,
                   sizeof(cc3000AsyncData.ping.report));
            cc3000AsyncData.ping.present = TRUE;
        }
    }

    else if (eventType == HCI_EVNT_WLAN_UNSOL_DISCONNECT)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_UNSOL_DISCONNECT", NULL);
        cc3000AsyncData.connected = FALSE;
        cc3000AsyncData.disconnected = TRUE;
        cc3000AsyncData.dhcp.present = FALSE;
    }

    else if (eventType == HCI_EVNT_WLAN_UNSOL_DHCP)
    {
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_WLAN_UNSOL_DHCP", NULL);

        if (length == DHCP_INFO_LENGTH_STATUS &&
            (data[DHCP_INFO_STATUS_BYTE] == 0))
        {
            memcpy((void*)&cc3000AsyncData.dhcp.info, data,
                   sizeof(cc3000AsyncData.dhcp.info));

            cc3000AsyncData.dhcp.present = TRUE;

            //CHIBIOS_CC3000_DBG_PRINT("aucIP: %x", *(uint32_t*)cc3000AsyncData.dhcp.info.aucIP);
            //CHIBIOS_CC3000_DBG_PRINT("aucSubnetMask: %x",*(uint32_t*)cc3000AsyncData.dhcp.info.aucSubnetMask);
            //CHIBIOS_CC3000_DBG_PRINT("aucDefaultGateway: %x", *(uint32_t*)cc3000AsyncData.dhcp.info.aucDefaultGateway);
            //CHIBIOS_CC3000_DBG_PRINT("aucDHCPServer: %x", *(uint32_t*)cc3000AsyncData.dhcp.info.aucDHCPServer);
            //CHIBIOS_CC3000_DBG_PRINT("aucDNSServer: %x", *(uint32_t*)cc3000AsyncData.dhcp.info.aucDNSServer);
        }
        else
        {
            cc3000AsyncData.dhcp.present = FALSE;
        }
    }

    else if (eventType == HCI_EVNT_BSD_TCP_CLOSE_WAIT)
    {
        //printf("TCP_CLOSE_WAIT for %i\r\n",data[0]);
        //signal socket close
        cc3000_handle_socket(data[0],-1);
        //CHIBIOS_CC3000_DBG_PRINT("HCI_EVNT_BSD_TCP_CLOSE_WAIT", NULL);
    }

    else if (eventType == HCI_EVENT_CC3000_CAN_SHUT_DOWN)
    {
        cc3000AsyncData.shutdownOk = TRUE;
    }
    
    else
    {
        //CHIBIOS_CC3000_DBG_PRINT("Unexpected Async Event. Type: %x.", eventType);
    }
}


