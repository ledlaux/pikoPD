#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Common settings for Pico W
#define NO_SYS                      1
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_TCP_PCB_LISTEN     2
#define MEMP_NUM_PBUF               10
#define PBUF_POOL_SIZE              8

// IP/TCP Settings
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define TCP_MSS                     1460
#define TCP_WND                     (2 * TCP_MSS)
#define TCP_SND_BUF                 (2 * TCP_MSS)
#define TCP_SND_QUEUELEN            (4 * TCP_SND_BUF/TCP_MSS)
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0


// --- HTTPD SERVER SETTINGS ---
#define LWIP_HTTPD                  1
#define LWIP_HTTPD_CGI              1
#define LWIP_HTTPD_SSI              0
#define LWIP_HTTPD_SUPPORT_POST     0

#define LWIP_HTTPD_CUSTOM_FILESYSTEM 1
#define HTTPD_USE_CUSTOM_FSDATA      0

#endif