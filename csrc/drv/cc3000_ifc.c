#include "cc3000_api.h"
#include "../nvmem.h"
#include "../socket.h"
#include "../error_codes.h"
#include "viper.h"

//#define printf(...) vbl_printf_stdout(__VA_ARGS__)
#define printf(...)

int32_t errno;
int32_t net_info_set = 0;
NetAddress net_ip;
NetAddress net_mask;
NetAddress net_gw;
NetAddress net_dns;

static VSemaphore sem;

void cc3000_prepare_addr(sockaddr *vmSocketAddr, NetAddress *addr) {
    vmSocketAddr->sa_family = AF_INET;
    memcpy(vmSocketAddr->sa_data, &addr->port, 2);
    memcpy(vmSocketAddr->sa_data + 2, &addr->ip, 4);
}


#define MAX_SOCKETS 4
static int32_t sockets[MAX_SOCKETS] = { -1, -1, -1, -1};

const uint32_t const cc3000_wifi_sec[] = { WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA, WLAN_SEC_WPA2};

/** LOW LEVEL **/
int cc3000_is_socket_valid(int32_t sock) {
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i] == sock)
            return 1;
    }
    return 0;
}

int cc3000_handle_socket(int32_t sockvalue, int32_t replvalue) {
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i] == sockvalue) {
            sockets[i] = replvalue;
            return i;
        }
    }
    return -1;
}

int cc3000_net_send(int32_t sock, uint8_t *buf, uint32_t len, uint32_t flags) {
    int res = 0, tsnd, wrt = 0;
    vosSemWait(sem);
    printf("cc3000 sending %i bytes to %i\r\n", len, sock);

    while (wrt < len) {
        tsnd = len - wrt;
        tsnd = tsnd < 64 ? tsnd : 64;
        res = send(sock, buf + wrt, tsnd, flags);
        if (res <= 0)
            break;
        printf("cc3000 sent %i of %i of %i/%i\n", res, tsnd, wrt, len);
        wrt += res;
    }
    vosSemSignal(sem);
    return wrt;
}

int cc3000_net_sendto(int32_t sock, uint8_t *buf, uint32_t len, uint32_t flags, NetAddress *addr) {
    sockaddr vmSocketAddr;
    int res;
    printf("sending_to: %i.%i.%i.%i:%i\r\n", OAL_IP_AT(addr->ip, 0), OAL_IP_AT(addr->ip, 1), OAL_IP_AT(addr->ip, 2),
           OAL_IP_AT(addr->ip, 3), OAL_GET_NETPORT(addr->port));
    cc3000_prepare_addr(&vmSocketAddr, addr);
    vosSemWait(sem);
    res = sendto(sock, buf, len, flags, &vmSocketAddr, sizeof(sockaddr));
    vosSemSignal(sem);
    printf("out of sendto\n");
    return res;
}

int cc3000_net_recv(int32_t sock, uint8_t *buf, uint32_t len, uint32_t flags) {
    int rb = 0;

    printf("recv!\n");
    vosSemWait(sem);
    int rrt = 0, tbr = 0;
    while (rrt < len) {
        tbr = ((len-rrt)>32) ? 32:(len-rrt);
        rb = recv(sock, buf+rrt, tbr, flags);
        if (rb < 0) {
            if (rb != RECV_TIMED_OUT) {
                closesocket(sock);
                cc3000_handle_socket(sock, -1);
            } else rrt=-rb;
            break;
        }
        rrt+=rb;
    }
    vosSemSignal(sem);
    printf("recv: %i read %i\r\n", rrt, buf[0]);
    return rrt;
}

int cc3000_net_available(int32_t sock, int32_t timeout) {
    fd_set readfd;
    struct timeval tm;
    FD_ZERO ( &readfd );
    FD_SET  ( (sock), &readfd);

    tm.tv_sec = 0;
    tm.tv_usec = timeout * 1000; //TODO: add correct timeout

    vosSemWait(sem);
    int retval = select( (sock + 1), &readfd, NULL, NULL, &tm );
    vosSemSignal(sem);

    if ( (retval > 0) && (FD_ISSET((sock), &readfd)) ) {
        return 1;
    } else if (!cc3000_is_socket_valid(sock)) {
        return -1;
    } else {
        return 0;
    }
}

int cc3000_net_recvfrom(int32_t sock, uint8_t *buf, uint32_t len, uint32_t flags, NetAddress *addr) {
    sockaddr vmSocketAddr;
    socklen_t tlen;
    int rb;

    printf("recvfrom %i, %x,%i\n", sock, buf, len);
    printf("recvfrom: %i %i %i %i %x\n", sockets[0], sockets[1], sockets[2], sockets[3], sockets);
    while ((rb = cc3000_net_available(sock, 0)) == 0) {
        vosThSleep(TIME_U(1, MILLIS));
        //printf("recvfrtom: %i available %i\r\n", sock, rb);
    }
    vosSemWait(sem);
    rb = recvfrom(sock, buf, len, flags, &vmSocketAddr, &tlen);
    printf("recvfrom read %i\n", rb);
    if (rb < 0) {
        if (rb != RECV_TIMED_OUT) { // rb is 0 even on timeout...so...how am I supposed to differentiate the two cases? (closed socket/timeout)
            closesocket(sock);
            cc3000_handle_socket(sock, -1);
            rb = 0;
        }
    }
    vosSemSignal(sem);
    printf("out of recvfrom\n");
    memcpy(&addr->ip, vmSocketAddr.sa_data + 2, 4);
    memcpy(&addr->port, vmSocketAddr.sa_data, 2);
    return rb;
}

/* =============================================================================
     CNATIVES
   ============================================================================= */


C_NATIVE(cc3000_init) {
    C_NATIVE_UNWARN();
    int32_t spi_prph;
    int32_t nss;
    int32_t wen;
    int32_t irq;

    printf("cc3000_init: parsing parameters\n");
    if (parse_py_args("iiii", nargs, args, &spi_prph, &nss, &wen, &irq) != 4)
        return ERR_TYPE_EXC;

    //init vhal spi driver
    vhalInitSPI(NULL);

    printf("cc3000_init: calling init wlan\n");
    if (cc3000WlanInit(spi_prph, nss, wen, irq) < 0)
        return ERR_PERIPHERAL_ERROR_EXC;
    printf("cc3000_init: creating semaphore\n");
    sem = vosSemCreate(1);
    RELEASE_GIL();

    printf("cc3000 wlan init...\r\n");
    vosSemWait(sem);
    printf("cc3000 wlan init.....\r\n");
    wlan_start(0);
    printf("cc3000 wlan init......\r\n");

    wlan_ioctl_set_connection_policy(0, 0, 0);

    vosSemSignal(sem);

    //wlan_disconnect();
    printf("cc3000 wlan init.......\r\n");
    ACQUIRE_GIL();
    return ERR_OK;
}

const unsigned long intervalTime[16] = { 2000, 2000, 2000, 2000,  2000,
                                         2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000

                                       };

_wlan_full_scan_results_args_t scan_res;


#ifndef VIPER_CC3000_TINY_DRIVER
C_NATIVE(cc3000_scan) {
    C_NATIVE_UNWARN();
    int32_t time;
    if (parse_py_args("i", nargs, args, &time) != 1)
        return ERR_TYPE_EXC;

    printf("cc3000_scan %i\n", time);

    scan_res.num_networks = 0;

    RELEASE_GIL();
    vosSemWait(sem);
    printf("Before scan params\n");

    int cr = 0;
    wlan_ioctl_set_scan_params(time, 20, 100, 5, 0x1FFF, -120, 0, 300, (unsigned long * ) &intervalTime);
    printf("cr %i\n", cr);
    vosThSleep(TIME_U(time + 500, MILLIS));

    cr = wlan_ioctl_get_scan_results(0, (uint8_t*)&scan_res);
    int nn = scan_res.num_networks;
    int i, cc = 0;
    printf("res %i nn %i\n", cr, nn);

    PList *tpl = plist_new(nn, NULL);
    for (i = 0; i < nn; i++) {
        printf("net %i valid %i\n", i, scan_res.scan_status);
        if (scan_res.scan_status > 1 || !(scan_res.rssi & 1))
            continue;
        PTuple *itpl = ptuple_new(4, NULL);
        PString *ssid = pstring_new(scan_res.sec_ssidlen >> 2, scan_res.ssid_name);
        PBytes *bssid = pbytes_new(6, scan_res.bssid);
        PTUPLE_SET_ITEM(itpl, 0, ssid);
        PTUPLE_SET_ITEM(itpl, 1, PSMALLINT_NEW(scan_res.sec_ssidlen & 0x3));
        PTUPLE_SET_ITEM(itpl, 2, PSMALLINT_NEW(scan_res.rssi >> 1));
        PTUPLE_SET_ITEM(itpl, 3, bssid);
        PLIST_SET_ITEM(tpl, cc, itpl);
        cc++;
        wlan_ioctl_get_scan_results(0, (uint8_t*)&scan_res);
    }
    PSEQUENCE_ELEMENTS(tpl) = cc;
    *res = tpl;
    wlan_ioctl_set_scan_params(0, 20, 100, 5, 0x1FFF, -120, 0, 300, (unsigned long * ) &intervalTime);

    vosSemSignal(sem);
    ACQUIRE_GIL();
    //*res=MAKE_NONE();
    return ERR_OK;
}
#endif

C_NATIVE(cc3000_done) {
    C_NATIVE_UNWARN();
    RELEASE_GIL();
    vosSemWait(sem);
    wlan_stop();
    vosSemSignal(sem);
    ACQUIRE_GIL();
    return ERR_OK;
}


C_NATIVE(cc3000_wifi_link) {
    C_NATIVE_UNWARN();
    uint8_t *ssid;
    int sidlen;
    int sec;
    uint8_t *password;
    int passlen;
    int cloop;


    if (parse_py_args("sis", nargs, args, &ssid, &sidlen, &sec, &password, &passlen) != 3)
        return ERR_TYPE_EXC;

    RELEASE_GIL();
    vosSemWait(sem);
    cloop = 0;


    if (!net_info_set) {
        //no static info set!
        //handle dhcp bug: reset to zero
        netapp_dhcp(&cloop, &cloop, &cloop, &cloop);
        printf("dhcp on\n");
    } else {
        netapp_dhcp(&net_ip.ip, &net_mask.ip, &net_gw.ip, &net_dns.ip);
        printf("dhcp off\n");
    }
    /*
    wlan_stop();
    vosThSleep(TIME_U(200,MILLIS));
    wlan_start(0);
    wlan_ioctl_set_connection_policy(0, 0, 0);
    */

    if (wlan_connect(cc3000_wifi_sec[sec], ssid, sidlen, NULL, password, passlen) != 0) {
        vosSemSignal(sem);
        ACQUIRE_GIL();
        return ERR_IOERROR_EXC;
    }
    printf("cc3000 wlan link...\n");

    cloop = 0;
    while (cc3000AsyncData.connected != 1) {
        vosThSleep(TIME_U(5, MILLIS));
        cloop++;
        if (cloop > 1000) {
            vosSemSignal(sem);
            ACQUIRE_GIL();
            return ERR_IOERROR_EXC;
        }
    }

    cloop = 0;
    printf("cc3000 wlan link.....\n");

    while (cc3000AsyncData.dhcp.present != 1) {
        vosThSleep(TIME_U(5, MILLIS));
        cloop++;
        if (cloop > 1000) {
            vosSemSignal(sem);
            ACQUIRE_GIL();
            return ERR_IOERROR_EXC;

        }
    }
    printf("cc3000 init...ok\r\n");
    //vosThSleep(TIME_U(100,MILLIS));

    unsigned long aucDHCP       = 14400;
    unsigned long aucARP        = 3600;
    unsigned long aucKeepalive  = 30;
    unsigned long aucInactivity = 0;
    if (netapp_timeout_values(&aucDHCP, &aucARP, &aucKeepalive, &aucInactivity) != 0) {
        printf("cc3000_init: can't set timeouts\r\n");
        vosSemSignal(sem);
        ACQUIRE_GIL();
        return ERR_TYPE_EXC;
    }
    //stupid workaround: it seems that calling gethostbyname twice now, fixes both gethostbyname errors and udp errors...go figure -_-
    NetAddress addr;
    gethostbyname("localhost", 9, &addr.ip);
    gethostbyname("localhost", 9, &addr.ip);

    vosSemSignal(sem);
    ACQUIRE_GIL();
    //vbl_printf_stdout("wifi_link\n");
    return ERR_OK;
}

C_NATIVE(cc3000_wifi_unlink) {

    RELEASE_GIL();
    vosSemWait(sem);
    wlan_disconnect();
    vosSemSignal(sem);
    ACQUIRE_GIL();

    return ERR_OK;
}


C_NATIVE(cc3000_sendto) {
    C_NATIVE_UNWARN();
    uint8_t *buf;
    int32_t len;
    int32_t flags;
    int32_t sock;
    NetAddress addr;
    printf("In cc3000_sendto\n");
    if (parse_py_args("isni", nargs, args,
                      &sock,
                      &buf, &len,
                      &addr,
                      &flags) != 4) return ERR_TYPE_EXC;

    printf("In cc3000_sendto2\n");
    RELEASE_GIL();

    sock = cc3000_net_sendto(sock, buf, len, flags, &addr);

    ACQUIRE_GIL();
    printf("In cc3000_sendto3\n");

    if (sock < 0) {
        return ERR_IOERROR_EXC;
    }
    *res = PSMALLINT_NEW(sock);


    return ERR_OK;
}

C_NATIVE(cc3000_send) {
    C_NATIVE_UNWARN();
    uint8_t *buf;
    int32_t len;
    int32_t flags;
    int32_t sock;
    if (parse_py_args("isi", nargs, args,
                      &sock,
                      &buf, &len,
                      &flags) != 3) return ERR_TYPE_EXC;
    RELEASE_GIL();
    sock = cc3000_net_send(sock, buf, len, flags);
    ACQUIRE_GIL();
    if (sock < 0) {
        return ERR_IOERROR_EXC;
    }
    *res = PSMALLINT_NEW(sock);
    return ERR_OK;
}


C_NATIVE(cc3000_sendall) {
    C_NATIVE_UNWARN();
    uint8_t *buf;
    int32_t len;
    int32_t flags;
    int32_t sock;
    int32_t tres = 0;
    if (parse_py_args("isi", nargs, args,
                      &sock,
                      &buf, &len,
                      &flags) != 3) return ERR_TYPE_EXC;
    RELEASE_GIL();
    while (len > 0) {
        printf("sendall: remaining %i at %x\n", len, buf);
        tres = cc3000_net_send(sock, buf, len, flags);
        printf("sendall: sent %i\n", tres);
        if (tres < 0) {
            break;
        }
        len -= tres;
        buf += tres;
    }

    ACQUIRE_GIL();
    if (tres < 0) {
        return ERR_IOERROR_EXC;
    }
    *res = MAKE_NONE();
    return ERR_OK;
}

C_NATIVE(cc3000_recv_into) {
    C_NATIVE_UNWARN();
    uint8_t *buf;
    int32_t len;
    int32_t sz;
    int32_t flags;
    int32_t ofs;
    int32_t sock;
    if (parse_py_args("isiiI", nargs, args,
                      &sock,
                      &buf, &len,
                      &sz,
                      &flags,
                      0,
                      &ofs
                     ) != 5) return ERR_TYPE_EXC;
    buf += ofs;
    len -= ofs;
    len = (sz < len) ? sz : len;
    RELEASE_GIL();
    sock = cc3000_net_recv(sock, buf, len, flags);
    ACQUIRE_GIL();

    if (sock < 0) {
        if (sock == RECV_TIMED_OUT)
            return ERR_TIMEOUT_EXC;
        return ERR_IOERROR_EXC;
    }
    *res = PSMALLINT_NEW(sock);

    return ERR_OK;
}



C_NATIVE(cc3000_recvfrom_into) {
    C_NATIVE_UNWARN();
    uint8_t *buf;
    int32_t len;
    int32_t sz;
    int32_t flags;
    int32_t ofs;
    int32_t sock;
    NetAddress addr;
    if (parse_py_args("isiiI", nargs, args,
                      &sock,
                      &buf, &len,
                      &sz,
                      &flags,
                      0,
                      &ofs
                     ) != 5) return ERR_TYPE_EXC;
    buf += ofs;
    len -= ofs;
    len = (sz < len) ? sz : len;

    RELEASE_GIL();
    addr.ip = 0;
    sock = cc3000_net_recvfrom(sock, buf, len, flags, &addr);
    ACQUIRE_GIL();

    if (sock < 0) {
        if (sock == RECV_TIMED_OUT)
            return ERR_TIMEOUT_EXC;
        return ERR_IOERROR_EXC;
    }
    PTuple *tpl = (PTuple *) psequence_new(PTUPLE, 2);
    PTUPLE_SET_ITEM(tpl, 0, PSMALLINT_NEW(sock));
    PObject *ipo = netaddress_to_object(&addr);
    PTUPLE_SET_ITEM(tpl, 1, ipo);
    *res = tpl;

    return ERR_OK;
}

C_NATIVE(cc3000_select) {
    C_NATIVE_UNWARN();
    int32_t timeout;
    int32_t tmp, i, j, sock = -1;

    if (nargs < 4)
        return ERR_TYPE_EXC;

    fd_set rfd;
    fd_set wfd;
    fd_set xfd;
    struct timeval tms;
    struct timeval *ptm;
    PObject *rlist = args[0];
    PObject *wlist = args[1];
    PObject *xlist = args[2];
    fd_set *fdsets[3] = {&rfd, &wfd, &xfd};
    PObject *slist[3] = {rlist, wlist, xlist};
    PObject *tm = args[3];


    if (timeout == MAKE_NONE()) {
        ptm = NULL;
    } else if (IS_PSMALLINT(tm)) {
        timeout = PSMALLINT_VALUE(tm);
        if (timeout < 0)
            return ERR_TYPE_EXC;
        tms.tv_sec = timeout / 1000;
        tms.tv_usec = (timeout % 1000) * 1000;
        ptm = &tms;
    } else return ERR_TYPE_EXC;

    for (j = 0; j < 3; j++) {
        tmp = PTYPE(slist[j]);
        if (!IS_OBJ_PSEQUENCE_TYPE(tmp))
            return ERR_TYPE_EXC;
        FD_ZERO (fdsets[j]);
        for (i = 0; i < PSEQUENCE_ELEMENTS(slist[j]); i++) {
            PObject *fd = PSEQUENCE_OBJECTS(slist[j])[i];
            if (IS_PSMALLINT(fd)) {
                FD_SET(PSMALLINT_VALUE(fd), fdsets[j]);
                if (PSMALLINT_VALUE(fd) > sock)
                    sock = PSMALLINT_VALUE(fd);
            } else return ERR_TYPE_EXC;
        }
    }

    RELEASE_GIL();

    vosSemWait(sem);
    tmp = select( (sock + 1), fdsets[0], fdsets[1], fdsets[2], ptm );
    vosSemSignal(sem);

    if (tmp < 0) {
        ACQUIRE_GIL();
        return ERR_IOERROR_EXC;
    }

    PTuple *tpl = (PTuple *) psequence_new(PTUPLE, 3);
    for (j = 0; j < 3; j++) {
        tmp = 0;
        for (i = 0; i < sock; i++) {
            if (FD_ISSET(i, fdsets[j])) tmp++;
        }
        PTuple *rtpl = psequence_new(PTUPLE, tmp);
        tmp = 0;
        for (i = 0; i < sock; i++) {
            if (FD_ISSET(i, fdsets[j])) {
                PTUPLE_SET_ITEM(rtpl, tmp, PSMALLINT_NEW(i));
                tmp++;
            }
        }
        PTUPLE_SET_ITEM(tpl, j, rtpl);
    }
    *res = tpl;
    ACQUIRE_GIL();
    return ERR_OK;
}


#define DRV_SOCK_DGRAM 1
#define DRV_SOCK_STREAM 0
#define DRV_AF_INET 0

C_NATIVE(cc3000_socket) {
    C_NATIVE_UNWARN();
    int32_t family;
    int32_t type;
    int32_t proto;
    if (parse_py_args("III", nargs, args, DRV_AF_INET, &family, DRV_SOCK_STREAM, &type, IPPROTO_TCP, &proto) != 3)
        return ERR_TYPE_EXC;
    if (type != DRV_SOCK_DGRAM && type != DRV_SOCK_STREAM)
        return ERR_TYPE_EXC;
    if (family != DRV_AF_INET)
        return ERR_UNSUPPORTED_EXC;
    printf("cc3000_socket %i %i %i %i %i %i\n", family, type, proto, args[0], args[1], args[2]);
    RELEASE_GIL();
    vosSemWait(sem);
    int32_t sock = socket(AF_INET, (type == DRV_SOCK_DGRAM) ? SOCK_DGRAM : SOCK_STREAM,
                          (type == DRV_SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP);
    vosSemSignal(sem);
    ACQUIRE_GIL();
    printf("CMD_SOCKET: %i\r\n", sock);
    if (sock < 0)
        return ERR_IOERROR_EXC;
    cc3000_handle_socket(-1, sock);
    printf("CMD_SOCKET: %i %i %i %i\n", sockets[0], sockets[1], sockets[2], sockets[3]);
    *res = PSMALLINT_NEW(sock);
    return ERR_OK;
}


C_NATIVE(cc3000_bind) {
    C_NATIVE_UNWARN();
    int32_t sock;
    NetAddress addr;
    if (parse_py_args("in", nargs, args, &sock, &addr) != 2)
        return ERR_TYPE_EXC;
    sockaddr serverSocketAddr;
    printf("binding_to: %i.%i.%i.%i:%i\r\n", OAL_IP_AT(addr.ip, 0), OAL_IP_AT(addr.ip, 1), OAL_IP_AT(addr.ip, 2),
           OAL_IP_AT(addr.ip, 3), OAL_GET_NETPORT(addr.port));
    addr.ip = 0;
    cc3000_prepare_addr(&serverSocketAddr, &addr);
    printf("binding to: %i.%i.%i.%i: %i-%i\r\n", serverSocketAddr.sa_data[2], serverSocketAddr.sa_data[3],
           serverSocketAddr.sa_data[4], serverSocketAddr.sa_data[5], serverSocketAddr.sa_data[0], serverSocketAddr.sa_data[1]);
    RELEASE_GIL();
    vosSemWait(sem);
    sock = bind(sock, &serverSocketAddr, sizeof(sockaddr));
    vosSemSignal(sem);
    ACQUIRE_GIL();
    printf("binding: %i\r\n", sock);
    if (sock < 0)
        return ERR_IOERROR_EXC;
    *res = MAKE_NONE();
    return ERR_OK;
}


C_NATIVE(cc3000_sockopt) {
    C_NATIVE_UNWARN();
    int32_t sock;
    int32_t level;
    int32_t optname;
    int32_t optvalue;

    if (parse_py_args("iiii", nargs, args, &sock, &level, &optname, &optvalue) != 4)
        return ERR_TYPE_EXC;

    vosSemWait(sem);
    sock = setsockopt(sock, level, optname, &optvalue, sizeof(optvalue));
    vosSemSignal(sem);
    if (sock < 0)
        return ERR_IOERROR_EXC;

    *res = MAKE_NONE();
    return ERR_OK;
}

C_NATIVE(cc3000_listen) {
    C_NATIVE_UNWARN();
    int32_t maxlog;
    int32_t sock;
    if (parse_py_args("ii", nargs, args, &sock, &maxlog) != 2)
        return ERR_TYPE_EXC;
    RELEASE_GIL();
    vosSemWait(sem);
    maxlog = listen(sock, maxlog);
    vosSemSignal(sem);
    ACQUIRE_GIL();
    if (maxlog)
        return ERR_IOERROR_EXC;
    *res = MAKE_NONE();
    return ERR_OK;
}


C_NATIVE(cc3000_accept) {
    C_NATIVE_UNWARN();
    int32_t sock;
    NetAddress addr;
    if (parse_py_args("i", nargs, args, &sock) != 1)
        return ERR_TYPE_EXC;
    sockaddr clientaddr;
    socklen_t addrlen;
    memset(&clientaddr, 0, sizeof(sockaddr));
    addrlen = sizeof(sockaddr);
    RELEASE_GIL();
    vosSemWait(sem);
    char arg = SOCK_ON;
    if (setsockopt(sock, SOL_SOCKET, SOCKOPT_ACCEPT_NONBLOCK, &arg, sizeof(arg)) < 0) {
        printf("CMD_ACCEPT: no sockopt");
        sock = -1;
    }
    vosSemSignal(sem);
    if (sock >= 0) {
        int32_t ecd = SOC_IN_PROGRESS;
        while (ecd <= -1) {
            vosSemWait(sem);
            ecd = accept(sock, &clientaddr, &addrlen);
            vosSemSignal(sem);
            if (ecd <= -1) vosThSleep(TIME_U(200, MILLIS));
            printf("CMD_ACCEPT: accept state %i\r\n", ecd);
        }
        sock = ecd;
    }
    ACQUIRE_GIL();
    if (sock < 0)
        return ERR_IOERROR_EXC;

    memcpy(&addr.ip, clientaddr.sa_data + 2, 4);
    memcpy(&addr.port, clientaddr.sa_data, 2);
    addr.ip = BLTSWAP32(addr.ip);
    printf("CMD_ACCEPT: %i\r\n", sock);
    PTuple *tpl = (PTuple *) psequence_new(PTUPLE, 2);
    PTUPLE_SET_ITEM(tpl, 0, PSMALLINT_NEW(sock));
    PObject *ipo = netaddress_to_object(&addr);
    PTUPLE_SET_ITEM(tpl, 1, ipo);
    *res = tpl;
    return ERR_OK;
}

C_NATIVE(cc3000_connect) {
    C_NATIVE_UNWARN();
    int32_t sock;
    NetAddress addr;

    if (parse_py_args("in", nargs, args, &sock, &addr) != 2)
        return ERR_TYPE_EXC;
    sockaddr vmSocketAddr;
    printf("connecting_to: %i.%i.%i.%i:%i\r\n", OAL_IP_AT(addr.ip, 0), OAL_IP_AT(addr.ip, 1), OAL_IP_AT(addr.ip, 2),
           OAL_IP_AT(addr.ip, 3), OAL_GET_NETPORT(addr.port));
    cc3000_prepare_addr(&vmSocketAddr, &addr);
    RELEASE_GIL();
    vosSemWait(sem);
    sock = connect(sock, &vmSocketAddr, sizeof(vmSocketAddr));
    vosSemSignal(sem);
    ACQUIRE_GIL();
    printf("CMD_OPEN: %i\r\n", sock);
    if (sock < 0) {
        return ERR_IOERROR_EXC;
    }
    *res = PSMALLINT_NEW(sock);
    return ERR_OK;
}

C_NATIVE(cc3000_close) {
    C_NATIVE_UNWARN();
    int32_t sock;
    if (parse_py_args("i", nargs, args, &sock) != 1)
        return ERR_TYPE_EXC;
    RELEASE_GIL();
    vosSemWait(sem);
    closesocket(sock);
    vosSemSignal(sem);
    ACQUIRE_GIL();
    cc3000_handle_socket(sock, -1);
    *res = PSMALLINT_NEW(sock);
    return ERR_OK;
}

C_NATIVE(cc3000_resolve) {
    C_NATIVE_UNWARN();
    uint8_t *url;
    uint32_t len;
    NetAddress addr;
    int32_t sock;
    if (parse_py_args("s", nargs, args, &url, &len) != 1)
        return ERR_TYPE_EXC;
    addr.ip = 0;
    RELEASE_GIL();
    vosSemWait(sem);
    sock = gethostbyname(url, len, &addr.ip);
    vosSemSignal(sem);
    ACQUIRE_GIL();
    printf("resolve %i %s\n", sock, url);
    if (sock < 0)
        return ERR_IOERROR_EXC;
    addr.port = 0;
    addr.ip = BLTSWAP32(addr.ip);
    *res = netaddress_to_object(&addr);
    return ERR_OK;

}

C_NATIVE(cc3000_set_info) {
    C_NATIVE_UNWARN();

    NetAddress ip;
    NetAddress mask;
    NetAddress gw;
    NetAddress dns;

    if (parse_py_args("nnnn", nargs, args,
                      &ip,
                      &mask,
                      &gw,
                      &dns) != 4) return ERR_TYPE_EXC;

    if (dns.ip == 0) {
        OAL_MAKE_IP(dns.ip, 8, 8, 8, 8);
    }
    if (mask.ip == 0) {
        OAL_MAKE_IP(mask.ip, 255, 255, 255, 255);
    }
    if (gw.ip == 0) {
        OAL_MAKE_IP(gw.ip, OAL_IP_AT(ip.ip, 0), OAL_IP_AT(ip.ip, 1), OAL_IP_AT(ip.ip, 2), 1);
    }

    net_ip.ip = ip.ip;
    net_gw.ip = gw.ip;
    net_dns.ip = dns.ip;
    net_mask.ip = mask.ip;
    if (ip.ip != 0)
        net_info_set = 1;
    else
        net_info_set = 0;

    *res = MAKE_NONE();
    return ERR_OK;

}

C_NATIVE(cc3000_info) {
    C_NATIVE_UNWARN();
    tNetappIpconfigRetArgs ipConfig;
    NetAddress addr;

    printf("before ipconfig\n");
    RELEASE_GIL();
    vosSemWait(sem);
    netapp_ipconfig(&ipConfig);
    vosSemSignal(sem);
    ACQUIRE_GIL();

    PTuple *tpl = psequence_new(PTUPLE, 5);

    addr.port = 0;
    uint8_t *tb = ipConfig.aucIP;
    printf("   >ip: %i.%i.%i.%i\r\n", tb[0], tb[1], tb[2], tb[3]);
    memcpy(&addr.ip, ipConfig.aucIP, 4);
    addr.ip = BLTSWAP32(addr.ip);
    printf("   >ip: %i.%i.%i.%i\r\n", OAL_IP_AT(addr.ip, 0), OAL_IP_AT(addr.ip, 1), OAL_IP_AT(addr.ip, 2), OAL_IP_AT(addr.ip,
            3));

    PTUPLE_SET_ITEM(tpl, 0, netaddress_to_object(&addr));

    tb = ipConfig.aucSubnetMask;
    printf(" >mask: %i.%i.%i.%i\r\n", tb[0], tb[1], tb[2], tb[3]);
    memcpy(&addr.ip, ipConfig.aucSubnetMask, 4);
    addr.ip = BLTSWAP32(addr.ip);
    printf(" >mask: %i.%i.%i.%i\r\n", OAL_IP_AT(addr.ip, 0), OAL_IP_AT(addr.ip, 1), OAL_IP_AT(addr.ip, 2), OAL_IP_AT(addr.ip,
            3));
    PTUPLE_SET_ITEM(tpl, 1, netaddress_to_object(&addr));

    tb = ipConfig.aucDefaultGateway;
    printf("   >gw: %i.%i.%i.%i\r\n", tb[0], tb[1], tb[2], tb[3]);
    memcpy(&addr.ip, ipConfig.aucDefaultGateway, 4);
    addr.ip = BLTSWAP32(addr.ip);
    printf("   >gw: %i.%i.%i.%i\r\n", OAL_IP_AT(addr.ip, 0), OAL_IP_AT(addr.ip, 1), OAL_IP_AT(addr.ip, 2), OAL_IP_AT(addr.ip,
            3));
    PTUPLE_SET_ITEM(tpl, 2, netaddress_to_object(&addr));

    tb = ipConfig.aucDNSServer;
    printf("  >dns: %i.%i.%i.%i\r\n", tb[0], tb[1], tb[2], tb[3]);
    memcpy(&addr.ip, ipConfig.aucDNSServer, 4);
    addr.ip = BLTSWAP32(addr.ip);
    printf("  >dns: %i.%i.%i.%i\r\n", OAL_IP_AT(addr.ip, 0), OAL_IP_AT(addr.ip, 1), OAL_IP_AT(addr.ip, 2), OAL_IP_AT(addr.ip,
            3));
    PTUPLE_SET_ITEM(tpl, 3, netaddress_to_object(&addr));

    PObject *mac = psequence_new(PBYTES, 6);
    memcpy(PSEQUENCE_BYTES(mac), ipConfig.uaMacAddr, 6);
    PTUPLE_SET_ITEM(tpl, 4, mac);
    *res = tpl;
    return ERR_OK;
}