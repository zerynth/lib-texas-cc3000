"""
.. module:: cc3000


*************
CC3000 Module
*************

This module implements the cc3000 wifi driver. At the moment some functionalities are missing:

    * smart config
    * ping

For low resource devices please consider the stripped down module :mod:`cc3000_tiny`.

It can be used for every kind of cc3000 breakout, going from the Adafruit shield to the CC3000 on the Particle Core.

It is important to remark that Texas Instrument itself has deprecated the CC3000 chip, and recommends CC3200. Also, CC3000 low level drivers are not fully compliant with the BSD socket standard, therefore some functionalities may be absent or different.

The CC3000 is based on spi and also needs two additional pins to function; one which is called WEN (Wireless Enable) used as turn on/shutdown; one which is called IRQ and is used by the CC3000 to signal that data is ready.

The CC3000 has an onboard firmware which is currently at version 1.14. **This modules supports CC3000 firmwares
from version 1.13 (included) only.** Refer to your CC3000 product instructions to update the CC3000 firmware if needed
(`Wiki <http://processors.wiki.ti.com/index.php/CC3000>`_).

CC3000 based products:  

    * `Adafruit HUZZAH Breakout <https://www.adafruit.com/products/1469>`_
    * `Adafruit HUZZAH Shield <http://www.adafruit.com/product/1491>`_
    * `SparkFun WiFi Breakout <https://www.sparkfun.com/products/12072>`_
    * `SparkFun WiFi Shield <https://www.sparkfun.com/products/12071>`_

    """

def auto_init():
    """
.. function:: auto_init()        

        Tries to automatically init the CC3000 driver by looking at the device type.
        The automatic configuration is possible for all the Arduino compatible devices
        and for the Particle Core.

        Otherwise an exception is raised        
    """
    if __defined(LAYOUT,"arduino_uno"):
        init(SPI0,D10,D5,D3)
    elif __defined(BOARD,"particle_core"):
        init(SPI1,D13,D11,D12)
    else:
        raise UnsupportedError

def init(spi,nss,wen,irq):
    """
.. function:: init(spi,nss,wen,irq)        
            
        Tries to init the CC3000 driver. *spi* is the name of the spi driver the CC3000 is connected to.
        *nss* is the pin used as Chip Select (CS). *wen* is the pin used as Wireless Enable. *irq* is the pin used by
        the CC3000 to generate an interrupt.

    """
    _hwinit(spi&0xff,nss,wen,irq)
    __builtins__.__default_net["wifi"] = __module__
    __builtins__.__default_net["sock"][0] = __module__ #AF_INET

@native_c("cc3000_init",["csrc/*","csrc/drv/*"],["VBL_SPI","VHAL_SPI"])
def _hwinit(spi,nss,wen,irq):
    pass


@native_c("cc3000_wifi_link",["csrc/*"])
def link(ssid,sec,password):
    pass

@native_c("cc3000_wifi_unlink",["csrc/*"])
def unlink():
    pass


@native_c("cc3000_info",["csrc/*"])
def link_info():
    pass

@native_c("cc3000_set_info",["csrc/*"])
def set_link_info(ip,mask,gw,dns):
    pass


@native_c("cc3000_scan",["csrc/*"])
def scan(duration):
    pass

@native_c("cc3000_done",["csrc/*"])
def done():
    pass

@native_c("cc3000_socket",["csrc/*"])
def socket(family,type,proto):
    pass

@native_c("cc3000_sockopt",["csrc/*"])
def setsockopt(sock,level,optname,value):
    pass


@native_c("cc3000_close",["csrc/*"])
def close(sock):
    pass


@native_c("cc3000_sendto",["csrc/*"])
def sendto(sock,buf,addr,flags=0):
    pass

@native_c("cc3000_send",["csrc/*"])
def send(sock,buf,flags=0):
    pass

@native_c("cc3000_sendall",["csrc/*"])
def sendall(sock,buf,flags=0):
    pass


@native_c("cc3000_recv_into",["csrc/*"])
def recv_into(sock,buf,bufsize,flags=0,ofs=0):
    pass


@native_c("cc3000_recvfrom_into",["csrc/*"])
def recvfrom_into(sock,buf,bufsize,flags=0):
    pass


@native_c("cc3000_bind",["csrc/*"])
def bind(sock,addr):
    pass

@native_c("cc3000_listen",["csrc/*"])
def listen(sock,maxlog=2):
    pass

@native_c("cc3000_accept",["csrc/*"])
def accept(sock):
    pass

@native_c("cc3000_connect",["csrc/*"])
def connect(sock,addr):
    pass


@native_c("cc3000_resolve",["csrc/*"])
def gethostbyname(hostname):
    pass


