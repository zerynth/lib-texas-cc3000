/** @file
*   @brief Contains the key API required to use this library.
*   @details Included here are only what this library uses. See TI's
*            doxygen documentation for the rest of the API for the CC3000. */
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

#ifndef __CC3000_API__
#define __CC3000_API__

//#include "cc3000_chibios_config.h"
#include "../wlan.h"
#include "../netapp.h"
#include <stdint.h>

/** @defgroup api API
 *  @brief The API which will need to be used in order to correctly use this
 *         library with ChibiOS-RT.
 *  @{ */

/** @brief Format of the callback function used to print debug information.
 *  @details @p fmt is a chprintf style formatted string, and the remaining
 *           arguments are variables for the string place holders. */
typedef void (*cc3000PrintCb)(const char *fmt, ...);

int cc3000WlanInit(uint16_t spi_prph, uint16_t nss,uint16_t wen, uint16_t irq);

void cc3000Shutdown(void);


/** @brief Holds ping report information. */
typedef struct {
    uint8_t present;                       ///< If a ping report is present.
    netapp_pingreport_args_t report;    ///< Ping report. See TI doxygen API.
} pingInformation;

/** @brief Holds DHCP information. */
typedef struct {
    uint8_t present;                   ///< If DHCP information is present
    tNetappDhcpParams info;         ///< DHCP information. See TI doxygen API.
} dhcpInformation;

/** @brief Structure holding information updated by the asynchronous callback.*/
typedef struct {
    uint8_t shutdownOk;
    uint8_t smartConfigFinished;
    uint8_t connected; //received a connect
    uint8_t disconnected; //received a disconnect
    /** @brief   Holds DHCP information from the event
     *           HCI_EVNT_WLAN_UNSOL_DHCP. */
    dhcpInformation dhcp;
    /** @brief Holds ping report information from the event
     *  HCI_EVNT_WLAN_ASYNC_PING_REPORT. */
    pingInformation ping;
} cc3000AsynchronousData;

extern volatile cc3000AsynchronousData cc3000AsyncData;

/** @} */

#endif /*__CC3000_API__*/

