/** @file
*   @brief     Header file to allow the CC3000HostDriver to use SPI functions.
*   @details   Needed by the CC3000HostDriver.                              */
/*****************************************************************************
*  Renamed to cc3000_spi.h and adapted for ChibiOS/RT by Alan Barr 2014.
*
*  spi.h  - CC3000 Host Driver Implementation.
*  Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __CHIBIOS_CC3000_SPI_H__
#define __CHIBIOS_CC3000_SPI_H__

#include "../cc3000_common.h"

/** @brief Function pointer to a function for processing received SPI data.
 *  @details This function is registered in #SpiOpen() and will be registered
 *  (in host driver 1.11.1) to SpiReceiveHandler() in wlan.c. */
typedef void (*gcSpiHandleRx)(void *p);

void SpiOpen(gcSpiHandleRx pfRxHandler);
void SpiClose(void);
void SpiWrite(unsigned char *pUserBuffer, unsigned short usLength);

void SpiResumeSpi(void);

extern unsigned char wlan_tx_buffer[CC3000_TX_BUFFER_SIZE];

#endif /*__CHIBIOS_CC3000_SPI_H__*/

