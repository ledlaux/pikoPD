#pragma once

#if WEB

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/apps/fs.h"

// AP mode
#if (ACTIVE_MODE == 0)
#include "web/dhcpserver.h"
#endif

#ifdef __cplusplus
}
#endif

#include <stddef.h> 
#include "web/ssi.h"
#include "web/picoOSC.h"
#include "pico/cyw43_arch.h"

#ifndef WIFI_SSID
#define WIFI_SSID "pikoPD"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "12345678"
#endif
#ifndef MDNS_NAME
#define MDNS_NAME "pikopd"
#endif

#ifndef OSC_ENABLED
#define OSC_ENABLED 0
#endif

#ifndef OSC_PORT
#define OSC_PORT 8000
#endif

static ip_addr_t computer_ip;
static uint16_t computer_port = 9000; 
static bool computer_discovered = false;
static struct udp_pcb* osc_out_pcb = nullptr;

typedef void (*web_float_handler_t)(const char *param, float value);

extern web_float_handler_t web_float_handler;

// --- CGI HANDLER ---
const char * cgi_pd_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    for (int i = 0; i < iNumParams; i++) {

        float raw_val = atof(pcValue[i]);

        if (strcmp(pcParam[i], "v") == 0) {
            if (web_float_handler) {
                web_float_handler(pcParam[i], raw_val);
            }
        }
    }
    return NULL; 
}
static const tCGI cgi_handlers[] = {
    { "/update_pd.cgi", cgi_pd_handler },
};

typedef void (*osc_hv_float_handler_t)(const char *address, float value);

extern osc_hv_float_handler_t osc_hv_handler;


#if OSC_ENABLED

static void osc_internal_callback(void *arg,
                                  struct udp_pcb *pcb,
                                  struct pbuf *p,
                                  const ip_addr_t *addr,
                                  u16_t port)
{
    if (!p) return;

    if (!computer_discovered || !ip_addr_cmp(&computer_ip, addr)) {
        ip_addr_set(&computer_ip, addr);
        computer_discovered = true;
        computer_port = 9000;
        printf(">>> OSC Peer Discovered: %s\n", ipaddr_ntoa(addr));
    }

    char address[64];
    size_t len = (p->len < 63) ? p->len : 63;
    std::memcpy(address, p->payload, len);
    address[len] = '\0';

    if (address[0] != '/') {
        pbuf_free(p);
        return;
    }

    int tag_idx = (int)std::strlen(address) + 1;
    while (tag_idx % 4 != 0 && tag_idx < p->len) tag_idx++;

    char *tags = (char*)p->payload + tag_idx;

    if (tag_idx < p->len && tags[0] == ',' && tags[1] == 'f') {

        int data_idx = tag_idx + (int)std::strlen(tags) + 1;
        while (data_idx % 4 != 0 && data_idx < p->len) data_idx++;

        if (data_idx + 4 <= p->len) {

            uint32_t raw;
            std::memcpy(&raw, (uint8_t*)p->payload + data_idx, 4);

            union { uint32_t i; float f; } u;
            u.i = __builtin_bswap32(raw);

            float val = u.f;

            if (osc_hv_handler) {
                osc_hv_handler(address, val);
            }
        }
    }

    pbuf_free(p);
}


// ---------------- SEND ----------------

static inline void osc_send_float(const char *name, float value) {
    if (!computer_discovered || osc_out_pcb == nullptr) return;

    picoosc::OSCMessage msg;

    char address[64];
    snprintf(address, sizeof(address), "/%s", name);

    msg.addAddress(address);
    msg.add(value);

    struct pbuf* pb = pbuf_alloc(PBUF_TRANSPORT, msg.size(), PBUF_RAM);
    if (!pb) return;

    std::memcpy(pb->payload, msg.data(), msg.size());

    udp_sendto(osc_out_pcb, pb, &computer_ip, computer_port);
    pbuf_free(pb);
}

static inline void osc_send_bang(const char *name) {
    osc_send_float(name, 1.0f);
}

#endif


// --- UNIFIED INITIALIZATION ---
bool init_wifi() {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("Wi-Fi Hardware Init Failed\n");
        return false;
    }

#if (ACTIVE_MODE == 0)
    // --- MODE 0: ACCESS POINT (AP) ---
    cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

    struct netif *n = &cyw43_state.netif[CYW43_ITF_AP];
    ip4_addr_t ip, mask, gw;
    IP4_ADDR(&ip, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 4, 1);
    netif_set_addr(n, &ip, &mask, &gw);

    // Initialize DHCP server so clients get IPs automatically
    static dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &gw, &mask);
    
    const char* mode_str = "Access Point";
#else
    // --- MODE 1: STATION (STA) ---
    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi: %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK)) {
        printf("Failed to connect to %s\n", WIFI_SSID);
        return false;
    }
    const char* mode_str = "Station";
#endif

    // Common Web Services Setup
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    httpd_init();
    http_set_cgi_handlers(cgi_handlers, 1); 
    ssi_init();

    mdns_resp_init();
    mdns_resp_add_netif(netif_default, "pikopd");
    mdns_resp_add_service(netif_default, "piko-control", "_http", DNSSD_PROTO_UDP, 80, NULL, NULL);
    mdns_resp_announce(netif_default);

    osc_out_pcb = udp_new();
    static picoosc::OSCServer osc_receiver(OSC_PORT, osc_internal_callback);

    printf("\n========================================\n");
    printf("Web Status: ACTIVE\n");
    printf("Mode:       %s\n", mode_str);
    printf("SSID:       %s\n", WIFI_SSID);
    printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    printf("mDNS:       http://%s.local\n", MDNS_NAME);
    printf("OSC Port:   %d\n", OSC_PORT);
    printf("========================================\n\n");


    return true;
}

static inline void web_poll() {
    cyw43_arch_poll();
}

#endif // WEB_ENABLED
