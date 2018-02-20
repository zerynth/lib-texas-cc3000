/** @file
*   @brief     CC3000 SPI driver for ChibiOS/RT.
*   @details   This bridges TI's CC3000 Host Driver to ChibiOS/RT to
*              facilitate communications with a CC3000 module.              */
/*****************************************************************************
*  Renamed to cc3000_spi.c and adapted for ChibiOS/RT by Alan Barr 2014.
*
*  spi.c - CC3000 Host Driver Implementation.
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
#include "viper.h"
#include "async_handler.h"
#include "cc3000_spi.h"
#include "../hci.h"
#include "../wlan.h"

//#define printf(...) vbl_printf_stdout(__VA_ARGS__)
#define printf(...)


/* OP Codes for CC3000_SPI_INDEX_OP */
#define CC3000_SPI_OP_WRITE         1
#define CC3000_SPI_OP_READ          3
#define CC3000_SPI_BUSY             0


#define HI(value)                       (((value) & 0xFF00) >> 8)
#define LO(value)                       ((value) & 0x00FF)

#define CC3000_SPI_INDEX_OP         0
#define CC3000_SPI_INDEX_LEN_MSB    1
#define CC3000_SPI_INDEX_LEN_LSB    2
#define CC3000_SPI_INDEX_BUSY_1     3
#define CC3000_SPI_INDEX_BUSY_2     4

#define CC3000_HEADERS_SIZE_EVNT    (SPI_HEADER_SIZE + 5)
#define CC3000_SPI_MAGIC_NUMBER     (0xDE)
#define CC3000_SPI_TX_MAGIC_INDEX   (CC3000_TX_BUFFER_SIZE - 1)
#define CC3000_SPI_RX_MAGIC_INDEX   (CC3000_RX_BUFFER_SIZE - 1)

#define CC3000_SPI_MIN_READ_B       (10)



#define SPI_STATE_POWERUP              (0)
#define SPI_STATE_INITIALIZED          (1)
#define SPI_STATE_IDLE                 (2)
#define SPI_STATE_WRITE_REQUESTED      (3)
#define SPI_STATE_WRITE_PERMITTED      (4)
#define SPI_STATE_READ_REQUESTED       (5)
#define SPI_STATE_READ_PERMITTED       (6)

typedef struct {
    gcSpiHandleRx rxHandlerCb;      ///< Handler function for received data.
    unsigned short txPacketLength;  ///< Number of bytes to transmit.
    unsigned short rxPacketLength;  ///< Number of bytes received. (DEBUG)
    unsigned long spiState;              ///< Current state of the driver.
    unsigned char *pTxPacket;       ///< Points to data to be transmitted.
    unsigned char *pRxPacket;       ///< Points to where to store received data.
} tSpiInformation;

static volatile tSpiInformation spiInformation;

unsigned char wlan_tx_buffer[CC3000_TX_BUFFER_SIZE];
static unsigned char spi_buffer[CC3000_RX_BUFFER_SIZE];

static const unsigned char spiReadCommand[] = {CC3000_SPI_OP_READ, CC3000_SPI_BUSY, CC3000_SPI_BUSY};

static volatile uint32_t spi_prph;
static volatile vhalSpiConf spi_conf;
static volatile uint16_t irqpin;
static volatile uint16_t wenpin;
static volatile char ccspi_is_in_irq = 0;
static volatile char ccspi_int_enabled = 0;
static volatile uint8_t _write_asked = 0;


static VSemaphore irqWriteSem;
static VSemaphore irqReadSem;
static VThread *pSignalHandlerThd = NULL;


void SpiWriteDataSynchronous(unsigned char *data, unsigned short size);
void selectCC3000(void);
void unselectCC3000(void);
void SpiOpen(gcSpiHandleRx pfRxHandler);
void SpiClose(void);
void SpiFirstWrite(unsigned char *pUserBuffer, unsigned short usLength);
void SpiWrite(unsigned char *pUserBuffer, unsigned short usLength);
void SpiWriteDataSynchronous(unsigned char *data, unsigned short size);
void SpiReadDataSynchronous(unsigned char *data, unsigned short size);
void SpiReadHeader(void);
void SpiReadAfterHeader(void);
void SpiPauseSpi(void);
void SpiResumeSpi(void);
void SpiTriggerRxProcessing(void);
void SSIContReadOperation(void);



void selectCC3000(void) {
    //int ret;
   //printf("+selectCC3000 %i\n", spi_prph);
    vhalSpiLock(spi_prph);
    vhalSpiInit(spi_prph, &spi_conf);
    vhalSpiSelect(spi_prph);
    //printf("-selectCC3000 %i\n", ret);

}

void unselectCC3000(void) {
   //printf("+unselectCC3000 %i\n", spi_prph);
    vhalSpiUnselect(spi_prph);
    vhalSpiDone(spi_prph);
    vhalSpiUnlock(spi_prph);
   //printf("-unselectCC3000\n");
}


#define setSpiStateI(x) (spiInformation.spiState = (x))
void setSpiState(uint32_t x) {
    vosSysLock();
    spiInformation.spiState = (x);
    vosSysUnlock();

}
#define getSpiState() (spiInformation.spiState)


void delay_poll(uint32_t ms) {
    volatile uint32_t *now = vosTicks();
    uint32_t time = *now;
    ms = ms * 1000 * (_system_frequency / 1000000);
   //printf("waiting %i\n", ms);
    while (*now - time < ms);
}



// void cc3000ExtCb(int slot, int dir) {
//     (void)slot;
//     (void)dir;

//     ccspi_is_in_irq = 1;

//    //printf("\tCC3000: Entering SPI_IRQ %i\n\r", getSpiState());

//     if (getSpiState() == SPI_STATE_POWERUP) {
//         /* IRQ line was low ... perform a callback on the HCI Layer */
//         setSpiState(SPI_STATE_INITIALIZED);
//     } else if (getSpiState() == SPI_STATE_IDLE) {
//         //DEBUGPRINT_F("IDLE\n\r");
//         setSpiState(SPI_STATE_READ_IRQ);

//         /* IRQ line goes down - start reception */

//         selectCC3000();

//         // Wait for TX/RX Compete which will come as DMA interrupt
//         SpiReadHeader();
//         setSpiState(SPI_STATE_READ_EOT);
//         //DEBUGPRINT_F("SSICont\n\r");
//         SSIContReadOperation();
//     }
//     else if (getSpiState() == SPI_STATE_WRITE_IRQ) {
//         SpiWriteDataSynchronous(spiInformation.pTxPacket, spiInformation.txPacketLength);
//         setSpiState(SPI_STATE_IDLE);
//         unselectCC3000();
//     }

//    //printf("\tCC3000: Leaving SPI_IRQ %i\n\r", getSpiState());

//     ccspi_is_in_irq = 0;
// }




void cc3k_int_poll()
{
   //printf("cc3k_poll\n");
    // if (vhalPinRead(irqpin) == 0 && ccspi_is_in_irq == 0 && ccspi_int_enabled != 0) {
    //    //printf("++cc3k_poll\n");
    //     cc3000ExtCb(0, 0);
    // }
}

void cc3000ExtCb(int slot, int dir) {
    (void)slot;
    (void)dir;

    vosSysLockIsr();
    if (getSpiState() == SPI_STATE_POWERUP) {
        setSpiStateI(SPI_STATE_INITIALIZED);
    } else if (getSpiState() == SPI_STATE_IDLE) {
        setSpiStateI(SPI_STATE_READ_REQUESTED);
        vosSemSignalIsr(irqReadSem);
    } else if (getSpiState() == SPI_STATE_WRITE_REQUESTED) {
        setSpiStateI(SPI_STATE_WRITE_PERMITTED);
        vosSemSignalIsr(irqWriteSem);
    } else {
        //uhh?!?
    }
    vosSysUnlockIsr();
}


int irqSignalHandlerThread(void *arg) {
    (void)arg;

    vhalPinSetMode(D9, PINMODE_OUTPUT_PUSHPULL);
    vhalPinWrite(D9, 0);
    while (1) {

        vhalPinWrite(D9, 1);
        vosSemWait(irqReadSem);
        vhalPinWrite(D9, 0);

        //state must be read requested
        if (getSpiState() == SPI_STATE_READ_REQUESTED) {
            setSpiState(SPI_STATE_READ_PERMITTED);

            selectCC3000();
            SpiReadHeader();
            SpiReadAfterHeader();
            SpiTriggerRxProcessing();
            setSpiState(SPI_STATE_IDLE);
            unselectCC3000();
        }

    }

    return 0;
}


void SpiOpen(gcSpiHandleRx pfRxHandler) {

   //printf("SpiOpen\n");
    //prepare buffers
    memset(spi_buffer, 0, CC3000_RX_BUFFER_SIZE);
    memset(wlan_tx_buffer, 0, CC3000_TX_BUFFER_SIZE);
    memset((void *)&cc3000AsyncData, 0, sizeof(cc3000AsyncData));
    spi_buffer[CC3000_SPI_RX_MAGIC_INDEX] = CC3000_SPI_MAGIC_NUMBER;
    wlan_tx_buffer[CC3000_SPI_TX_MAGIC_INDEX] = CC3000_SPI_MAGIC_NUMBER;


    setSpiState(SPI_STATE_POWERUP);
    spiInformation.rxHandlerCb = pfRxHandler;
    spiInformation.txPacketLength = 0;
    spiInformation.pTxPacket = NULL;
    spiInformation.pRxPacket = (unsigned char *)spi_buffer;
    spiInformation.rxPacketLength = 0;

    //vhalPinAttachInterrupt(irqpin, PINMODE_EXT_FALLING, cc3000ExtCb);

    tSLInformation.WlanInterruptEnable();

    //delay_poll(100);
    vosThSleep(TIME_U(100, MILLIS));

}

void SpiClose(void) {
   //printf("SpiClose\n");
    if (spiInformation.pRxPacket) {
        spiInformation.pRxPacket = 0;
    }
    tSLInformation.WlanInterruptDisable();

    vhalPinAttachInterrupt(irqpin, 0, NULL,TIME_U(0,MILLIS));
    vhalSpiDone(spi_prph);
    vhalSpiUnlock(spi_prph);
}


void SpiFirstWrite(unsigned char *pUserBuffer, unsigned short usLength) {
   //printf("+SpiFirstWrite\n");
    selectCC3000();
    //delay_poll(1);
    vosThSleep(TIME_U(5, MILLIS));

    SpiWriteDataSynchronous(pUserBuffer, 4);

    //delay_poll(1);
    vosThSleep(TIME_U(5, MILLIS));

    SpiWriteDataSynchronous(pUserBuffer + 4, usLength - 4);

    setSpiState(SPI_STATE_IDLE);

    unselectCC3000();
   //printf("-SpiFirstWrite\n");

}

void SpiWrite(unsigned char *pUserBuffer, unsigned short usLength) {
   //printf("+SpiWrite\n");

    /* If usLength is even, we need to add padding byte */
    if (!(usLength & 0x01)) {
        usLength++;
    }

    /* @todo TI issue: Why can't altering pUserBuffer and usLength be done
     *       in the host driver? */

    /* Fill in SPI header */
    pUserBuffer[0] = CC3000_SPI_OP_WRITE;
    pUserBuffer[1] = HI(usLength);//((usLength) & 0xFF00) >> 8;
    pUserBuffer[2] = LO(usLength);//((usLength) & 0x00FF);
    pUserBuffer[3] = CC3000_SPI_BUSY;
    pUserBuffer[4] = CC3000_SPI_BUSY;

    usLength += SPI_HEADER_SIZE;

    if (wlan_tx_buffer[CC3000_SPI_TX_MAGIC_INDEX] != CC3000_SPI_MAGIC_NUMBER) {
        //CHIBIOS_CC3000_DBG_PRINT("Buffer overflow detected.", NULL);
        while (1);
    }

   //printf("SpiWrite: state1 %i\n", spiInformation.spiState);
    if (getSpiState() == SPI_STATE_POWERUP) {
        while (getSpiState() != SPI_STATE_INITIALIZED) {
            vosThSleep(TIME_U(10, MILLIS));
        }
    }

   //printf("SpiWrite: state2 %i\n", spiInformation.spiState);
    if (getSpiState() == SPI_STATE_INITIALIZED) {
        SpiFirstWrite(pUserBuffer, usLength);
    } else {

        vosSysLock();
        while (getSpiState() != SPI_STATE_IDLE) {
            vosSysUnlock();
            vosThSleep(TIME_U(500, MICROS));
            vosSysLock();
        }
        setSpiState(SPI_STATE_WRITE_REQUESTED);
        vosSysUnlock();

        //can't be a race condition here, since state is WRITE_REQUESTED

        selectCC3000();
        vosSemWait(irqWriteSem);

        spiInformation.pTxPacket = pUserBuffer;
        spiInformation.txPacketLength = usLength;

        SpiWriteDataSynchronous(spiInformation.pTxPacket, spiInformation.txPacketLength);
        setSpiState(SPI_STATE_IDLE);
        unselectCC3000();
    }

}


void SpiWriteDataSynchronous(unsigned char *data, unsigned short size) {
    // int i;
    // for (i = 0; i < size; i++)
    //    //printf("<< %x\n", data[i]);
    vhalSpiExchange(spi_prph, (void *)data, NULL, size);
}

void SpiReadDataSynchronous(unsigned char *data, unsigned short size) {
    memcpy(data, spiReadCommand, sizeof(spiReadCommand));
    vhalSpiExchange(spi_prph, (void *)data, (void *)data, size);
    // for (i = 0; i < size; i++)
    //    //printf(">> %x\n", data[i]);

    //spiInformation.rxPacketLength += size;
}

void SpiReadHeader(void) {
   //printf("ReadHeader\n");
    SpiReadDataSynchronous(spiInformation.pRxPacket, CC3000_HEADERS_SIZE_EVNT);
}



void SpiReadAfterHeader(void) {
    long data_to_recv = 0;
    unsigned char *evnt_buff, type;
   //printf("ReadHeaderAfter\n");

    /* Determine what type of packet we have */
    evnt_buff =  spiInformation.pRxPacket;
    STREAM_TO_UINT8((char *)(evnt_buff + SPI_HEADER_SIZE),
                    HCI_PACKET_TYPE_OFFSET, type);

    switch (type) {
        case HCI_TYPE_DATA: {
            /* We need to read the rest of data.. */
            STREAM_TO_UINT16((char *)(evnt_buff + SPI_HEADER_SIZE), HCI_DATA_LENGTH_OFFSET, data_to_recv);

            if (!((CC3000_HEADERS_SIZE_EVNT + data_to_recv) & 1)) {
                data_to_recv++;
            }

            if (data_to_recv) {
                SpiReadDataSynchronous(evnt_buff + CC3000_HEADERS_SIZE_EVNT, data_to_recv);
            }
            break;
        }
        case HCI_TYPE_EVNT: {
            /* Calculate the rest length of the data*/
            STREAM_TO_UINT8((char *)(evnt_buff + SPI_HEADER_SIZE),
                            HCI_EVENT_LENGTH_OFFSET, data_to_recv);
            data_to_recv -= 1;

            /* Add padding byte if needed */
            if ((CC3000_HEADERS_SIZE_EVNT + data_to_recv) & 1) {
                data_to_recv++;
            }

            if (data_to_recv) {
                SpiReadDataSynchronous(evnt_buff + CC3000_HEADERS_SIZE_EVNT, data_to_recv);
            }
            break;
        }
    }
}



void SpiPauseSpi(void) {
   //printf("SpiPauseSpi\n");
    ccspi_int_enabled = 0;
    //vhalPinAttachInterrupt(irqpin, 0, NULL);
}

void SpiResumeSpi(void) {
   //printf("SpiResumeSpi\n");
    ccspi_int_enabled = 1;
    //vhalPinAttachInterrupt(irqpin, PINMODE_EXT_FALLING | PINMODE_INPUT_PULLUP, cc3000ExtCb);
}


/** @brief Responsible for calling into TI's host driver with received data. */
void SpiTriggerRxProcessing(void) {
   //printf("SpiTriggerRx\n");
    //SpiPauseSpi();

    /** @todo TI Issue: This is where it is in their example.
     * Can we not just hold this low, until we are done? i.e. move it until
     * just before we return from this function? This should mean the CC3000
     * won't produce another interrupt until we are done processing this one. */

    if (spi_buffer[CC3000_SPI_RX_MAGIC_INDEX] != CC3000_SPI_MAGIC_NUMBER) {
        //CHIBIOS_CC3000_DBG_PRINT("Buffer overflow detected.", NULL);
        while (1);
    }
    spiInformation.rxHandlerCb(spiInformation.pRxPacket + SPI_HEADER_SIZE);

}



void SSIContReadOperation(void) {
   //printf("\tCC3000: SpiContReadOperation\n\r");
    /* The header was read - continue with  the payload read */
    SpiReadAfterHeader();
    SpiTriggerRxProcessing();

}


/** @brief Registered callback to wlan_init() to read from IRQ pin.*/
long ReadWlanInterruptPin(void) {
    return vhalPinRead(irqpin);//palReadPad(CHIBIOS_CC3000_IRQ_PORT, CHIBIOS_CC3000_IRQ_PAD);
}


/** @brief Registered callback to wlan_init() to write to WLAN EN pin.
 *  @param val Value to set Wlan pin. */
void WriteWlanPin(unsigned char val) {
    if (val) {
        vhalPinWrite(wenpin, 1);
       //printf("WEN up\n");
        //palSetPad(CHIBIOS_CC3000_WLAN_EN_PORT, CHIBIOS_CC3000_WLAN_EN_PAD);
    } else {
        vhalPinWrite(wenpin, 0);
       //printf("WEN dn\n");
        //palClearPad(CHIBIOS_CC3000_WLAN_EN_PORT, CHIBIOS_CC3000_WLAN_EN_PAD);
    }
}


void WlanInterruptEnable() {
    //DEBUGPRINT_F("\tCC3000: WlanInterruptEnable.\n\r");
    // delay(100);
    ccspi_int_enabled = 1;
    vhalPinAttachInterrupt(irqpin, PINMODE_EXT_FALLING | PINMODE_INPUT_PULLUP, cc3000ExtCb,TIME_U(0,MILLIS));
}

void WlanInterruptDisable() {
    //DEBUGPRINT_F("\tCC3000: WlanInterruptDisable\n\r");
    ccspi_int_enabled = 0;
    vhalPinAttachInterrupt(irqpin, 0, NULL,TIME_U(0,MILLIS));
}


int cc3000WlanInit(uint16_t spi_ph, uint16_t nss, uint16_t wen, uint16_t irq) {
    /* Hold the SPI Driver to be used */
    int ret;

    irqpin = irq;
    wenpin = wen;

    spi_prph = spi_ph;

   //printf("cc3000WlanInit: got spi_prh %i\n", spi_prph);
    spi_conf.clock = 14000000;
    spi_conf.miso = _vm_spi_pins[spi_prph].miso;
    spi_conf.mosi = _vm_spi_pins[spi_prph].mosi;
    spi_conf.sclk = _vm_spi_pins[spi_prph].sclk;
    spi_conf.nss = nss;
    spi_conf.mode = SPI_MODE_LOW_SECOND;
    spi_conf.bits = SPI_BITS_8;
    spi_conf.master = 1;

    vhalSpiLock(spi_prph);
    ret = vhalSpiInit(spi_prph, &spi_conf);
    vhalSpiUnselect(spi_prph);
    vhalSpiDone(spi_prph);
    vhalSpiUnlock(spi_prph);

    if (ret < 0) return -1;


    vhalPinSetMode(irqpin, PINMODE_INPUT_PULLUP);
    vhalPinAttachInterrupt(irqpin, PINMODE_EXT_FALLING | PINMODE_INPUT_PULLUP, cc3000ExtCb,TIME_U(0,MILLIS));

    irqReadSem = vosSemCreate(0);
    irqWriteSem = vosSemCreate(0);
    //prio must be as high as vm irqthread, otherwise interrupts are not handled correctly (thread body is not executed with vm_irqthread forever in the hci while -_-)
    pSignalHandlerThd = vosThCreate(640, VOS_PRIO_HIGHER, irqSignalHandlerThread, NULL, NULL);
    vosThResume(pSignalHandlerThd);

    vhalPinSetMode(wenpin, PINMODE_OUTPUT_PUSHPULL);
    WriteWlanPin(0);
    vosThSleep(TIME_U(1000, MILLIS));



    wlan_init(CC3000AsyncCb, 0, 0,
              0, ReadWlanInterruptPin,
              WlanInterruptEnable, WlanInterruptDisable, WriteWlanPin);
}
