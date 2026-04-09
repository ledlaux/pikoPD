#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/apps/fs.h"
#include <stddef.h> 
#include "web/ssi.h"
#include "web/picoOSC.h"

// Peer Discovery Variables
static ip_addr_t computer_ip;
static uint16_t computer_port = 9000; 
static bool computer_discovered = false;
static struct udp_pcb* osc_out_pcb = nullptr;


// Define your credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""



const char * cgi_pd_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    for (int i = 0; i < iNumParams; i++) {
        float raw_val = atof(pcValue[i]);
        
        if (strcmp(pcParam[i], "v") == 0) {
            hv_sendFloatToReceiver(&pd_prog, 0x65400F82, raw_val);
        // } else if (strcmp(pcParam[i], "t") == 0) {
        //     hv_sendFloatToReceiver(&pd_prog, 0x99AABBCC, raw_val);
        // } else if (strcmp(pcParam[i], "s") == 0) {
        //     hv_sendFloatToReceiver(&pd_prog, 0x22FF3344, raw_val);
        // }
    }
    
    return NULL; 
}
}
static const tCGI cgi_handlers[] = {
    { "/update_pd.cgi", cgi_pd_handler },
};


static void osc_internal_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    // 1. Safety Guard
    if (p == NULL) return;

    // 2. PEER DISCOVERY
    // Using ip_addr_set to safely copy the address structure
    if (!computer_discovered || !ip_addr_cmp(&computer_ip, addr)) {
        ip_addr_set(&computer_ip, addr);
        computer_discovered = true;
        // We force the port to 9000 for the Mac Listener (oscdump)
        computer_port = 9000; 
        printf(">>> OSC Peer Discovered: %s\n", ipaddr_ntoa(addr));
    }

    // 3. RECEIVER LOGIC
    char* payload = (char*)p->payload;
    
    // Check if it's a valid OSC path (starts with /)
    if (p->len > 4 && payload[0] == '/') {
        // Find Type Tags
        int tag_idx = (int)std::strlen(payload) + 1;
        while (tag_idx % 4 != 0 && tag_idx < p->len) tag_idx++; 
        
        if (tag_idx < p->len) {
            char* tags = payload + tag_idx;

            // Only process if it's a float (",f")
            if (tags[0] == ',' && tags[1] == 'f') {
                int data_idx = tag_idx + (int)std::strlen(tags) + 1;
                while (data_idx % 4 != 0 && data_idx < p->len) data_idx++;

                if (data_idx + 4 <= p->len) {
                    uint32_t raw;
                    std::memcpy(&raw, payload + data_idx, 4);
                    
                    // OSC is Big-Endian, Pico is Little-Endian
                    union { uint32_t i; float f; } u;
                    u.i = __builtin_bswap32(raw);
                    float val = u.f;

                    // Route to Heavy/PD (web1 receiver)
                    if (strcmp(payload, "/v") == 0) {
                        hv_sendFloatToReceiver(&pd_prog, 0x65400F82, val);
                    }
                }
            }
        }
    }
    
    // 4. THE ONLY FREE POINT
    // Explicitly free the pbuf provided by the callback
    pbuf_free(p); 
}



bool init_wifi() {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("Failed to initialize hardware\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    int connect_result = cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    
    if (connect_result != 0) {
        printf("Failed to connect: %d\n", connect_result);
        return false;
    }

    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    httpd_init();
    http_set_cgi_handlers(cgi_handlers, 1); 
    ssi_init();

    mdns_resp_init();
    mdns_resp_add_netif(netif_default, "pikopd");
    mdns_resp_add_service(netif_default, "piko-control", "_http", DNSSD_PROTO_UDP, 80, NULL, NULL);

    osc_out_pcb = udp_new();
    static picoosc::OSCServer osc_receiver(8000, osc_internal_callback);
    
    printf("\n========================================\n");
    printf("Wi-Fi Connected!\n");
    printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    printf("mDNS: http://pikopd.local\n");
    printf("OSC Server listening on port 8000\n");
    printf("========================================\n\n");

    return true;
}


bool init_wifi_hotspot() {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) return false;

    // SSID and Password for the Pico's own network
    cyw43_arch_enable_ap_mode("PikoHotspot", "password123", CYW43_AUTH_WPA2_AES_PSK);

    // Setup AP Interface IP (192.168.4.1)
    struct netif *n = &cyw43_state.netif[CYW43_ITF_AP];
    ip4_addr_t ip, mask, gw;
    IP4_ADDR(&ip, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 4, 1);
    netif_set_addr(n, &ip, &mask, &gw);

    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    // Initialize Services
    httpd_init();
    http_set_cgi_handlers(cgi_handlers, 1); 
    
    mdns_resp_init();
    mdns_resp_add_netif(n, "pikopd"); 
    mdns_resp_add_service(n, "piko-control", "_http", DNSSD_PROTO_UDP, 80, NULL, NULL);

    osc_out_pcb = udp_new();
    static picoosc::OSCServer osc_receiver(8000, osc_internal_callback);

    printf("AP Mode Active: http://pikopd.local (192.168.4.1)\n");
    return true;
}
