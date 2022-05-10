#ifndef ethernet_h
#define ethernet_h

#include <cinttypes>
#include "IPAddress.h"
#include "EthernetClient.h"
#include "EthernetServer.h"
#include "Dhcp.h"
#include"smodule.h"

class EthernetClass : public smodule {
private:
    IPAddress _dnsServerAddress;
    IPAddress local_ip;
    uint32_t net_tick{}, rstCount{};
#if DHCP
    DhcpClass *_dhcp;
#endif
    uint8_t mac_address[6]{};

    uint8_t *macAddressDefault();

public:
    // Initialise the Ethernet with the internal provided MAC address and gain the rest of the
    // configuration through DHCP.
    // Returns 0 if the DHCP configuration failed, and 1 if it succeeded
#if DHCP
    int begin(unsigned long timeout = 60000, unsigned long responseTimeout = 4000);
#endif

    void begin(IPAddress &local_ip);

    void begin(IPAddress &local_ip, IPAddress &subnet);

    void begin(IPAddress &local_ip, IPAddress &subnet, IPAddress &gateway);

    void begin(IPAddress &local_ip_, IPAddress &subnet, IPAddress &gateway, IPAddress &dns_server);
    // Initialise the Ethernet shield to use the provided MAC address and gain the rest of the
    // configuration through DHCP.
    // Returns 0 if the DHCP configuration failed, and 1 if it succeeded
#if DHCP
    int begin(uint8_t *mac_address, unsigned long timeout = 60000, unsigned long responseTimeout = 4000);
#endif

    void begin(uint8_t *mac_address, IPAddress &local_ip);

    void begin(uint8_t *macAddress, IPAddress &local_ip, IPAddress &dns_server);

    void begin(uint8_t *mac_address, IPAddress &local_ip, IPAddress &dns_server, IPAddress &gateway);

    void
    begin(uint8_t *mac_address, IPAddress &local_ip_, IPAddress &dns_server, IPAddress &gateway, IPAddress &subnet);

    int maintain();

    void schedule();

    void macAddress(uint8_t *mac);

    uint8_t *macAddress();

    static IPAddress localIP();

    static IPAddress subnetMask();

    static IPAddress gatewayIP();

    IPAddress dnsServerIP();

    void set_ip(IPAddress &local_ip);

    friend class EthernetClient;

    friend class EthernetServer;

    int run(u32 tick) override { return 0; }

    int begin(u32 tick) override { return 0; }

    int diag(u32 tick) override;

    void end();
};

extern EthernetClass Ethernet;

#endif
