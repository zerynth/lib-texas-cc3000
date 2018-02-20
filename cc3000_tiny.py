"""
.. module:: cc3000_tiny


***********
CC3000 Tiny
***********

This module implements the cc3000 wifi driver with stripped down functionalities for low resource devices. The following funtionalities are missing:

    * wifi network scanning
    * ping
    * smart_config

Refer to :mod:`cc3000` for usage info

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

@native_c("cc3000_init",["csrc/*","csrc/drv/*"],["VBL_SPI","VHAL_SPI","VIPER_CC3000_TINY_DRIVER"])
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


