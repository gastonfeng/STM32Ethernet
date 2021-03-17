#include "STM32Ethernet.h"
#include "Dhcp.h"
#include <STM32FreeRTOS.h>

void thread_eth(void const *arg) {
    EthernetClass *eth = (EthernetClass *) arg;
    eth->thread();
}

#if DHCP
int EthernetClass::begin(unsigned long timeout, unsigned long responseTimeout)
{
  static DhcpClass s_dhcp;
  _dhcp = &s_dhcp;
  stm32_eth_init(macAddressDefault(), NULL, NULL, NULL);

  // Now try to get our config info from a DHCP server
  int ret = _dhcp->beginWithDHCP(mac_address, timeout, responseTimeout);
  if (ret == 1)
  {
    _dnsServerAddress = _dhcp->getDnsServerIp();
  }
  osThreadDef(EthMon, thread_eth, osPriorityNormal, 0, 128);
  osThreadCreate(osThread(EthMon), this);

  return ret;
}
#endif

void EthernetClass::begin(IPAddress local_ip) {
    IPAddress subnet(255, 255, 255, 0);
    begin(local_ip, subnet);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet) {
    // Assume the gateway will be the machine on the same network as the local IP
    // but with last octet being '1'
    IPAddress gateway = local_ip;
    gateway[3] = 1;
    begin(local_ip, subnet, gateway);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway) {
    // Assume the DNS server will be the machine on the same network as the local IP
    // but with last octet being '1'
    IPAddress dns_server = local_ip;
    dns_server[3] = 1;
    begin(local_ip, subnet, gateway, dns_server);
}

void EthernetClass::set_ip(const IPAddress local_ip) {
    extern struct netif gnetif;
    ip4_addr_t ip;
    ip.addr = uint32_t(local_ip);
    netif_set_ipaddr(&gnetif, &ip);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway, IPAddress dns_server) {
    this->local_ip = local_ip;
    stm32_eth_init(macAddressDefault(), local_ip.raw_address(), gateway.raw_address(), subnet.raw_address());
    /* If there is a local DHCP informs it of our manual IP configuration to
    prevent IP conflict */
#if DHCP
    stm32_DHCP_manual_config();
#endif
    _dnsServerAddress = dns_server;
    osThreadDef(EthMon, thread_eth, osPriorityNormal, 0, 128);
    osThreadCreate(osThread(EthMon), this);
}

#if DHCP
int EthernetClass::begin(uint8_t *mac_address, unsigned long timeout, unsigned long responseTimeout)
{

  static DhcpClass s_dhcp;
  _dhcp = &s_dhcp;

  stm32_eth_init(mac_address, NULL, NULL, NULL);

  // Now try to get our config info from a DHCP server
  int ret = _dhcp->beginWithDHCP(mac_address, timeout, responseTimeout);
  if (ret == 1)
  {
    _dnsServerAddress = _dhcp->getDnsServerIp();
  }
  macAddress(mac_address);
  osThreadDef(EthMon, thread_eth, osPriorityNormal, 0, 128);
  osThreadCreate(osThread(EthMon), this);

  return ret;
}
#endif

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip) {
    // Assume the DNS server will be the machine on the same network as the local IP
    // but with last octet being '1'
    IPAddress dns_server = local_ip;
    dns_server[3] = 1;
    begin(mac_address, local_ip, dns_server);
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server) {
    // Assume the gateway will be the machine on the same network as the local IP
    // but with last octet being '1'
    IPAddress gateway = local_ip;
    gateway[3] = 1;
    begin(mac_address, local_ip, dns_server, gateway);
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway) {
    IPAddress subnet(255, 255, 255, 0);
    begin(mac_address, local_ip, dns_server, gateway, subnet);
}

void EthernetClass::begin(uint8_t *mac, IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet) {
    this->local_ip = local_ip;
    stm32_eth_init(mac, local_ip.raw_address(), gateway.raw_address(), subnet.raw_address());
/* If there is a local DHCP informs it of our manual IP configuration to
  prevent IP conflict */
#if DHCP
    stm32_DHCP_manual_config();
#endif
    _dnsServerAddress = dns_server;
    macAddress(mac);
    osThreadDef(EthMon, thread_eth, osPriorityNormal, 0, 128);
    osThreadCreate(osThread(EthMon), this);
}

int EthernetClass::maintain() {
#if DHCP
    int rc = DHCP_CHECK_NONE;

    if (_dhcp != NULL)
    {
      //we have a pointer to dhcp, use it
      rc = _dhcp->checkLease();
      switch (rc)
      {
      case DHCP_CHECK_NONE:
        //nothing done
        break;
      case DHCP_CHECK_RENEW_OK:
      case DHCP_CHECK_REBIND_OK:
        _dnsServerAddress = _dhcp->getDnsServerIp();
        break;
      default:
        //this is actually a error, it will retry though
        break;
      }
    }
    return rc;
#endif
}

extern "C" void stm32_eth_scheduler();

/*
 * This function updates the LwIP stack and can be called to be sure to update
 * the stack (e.g. in case of a long loop).
 */
void EthernetClass::schedule(void) {
    stm32_eth_scheduler();
}

uint8_t *EthernetClass::macAddressDefault(void) {
    if ((mac_address[0] + mac_address[1] + mac_address[2] + mac_address[3] + mac_address[4] + mac_address[5]) == 0) {
        uint32_t baseUID = *(uint32_t *) UID_BASE;
        mac_address[0] = 0x00;
        mac_address[1] = 0x80;
        mac_address[2] = 0xE1;
        mac_address[3] = (baseUID & 0x00FF0000) >> 16;
        mac_address[4] = (baseUID & 0x0000FF00) >> 8;
        mac_address[5] = (baseUID & 0x000000FF);
    }
    return mac_address;
}

void EthernetClass::macAddress(uint8_t *mac) {
    mac_address[0] = mac[0];
    mac_address[1] = mac[1];
    mac_address[2] = mac[2];
    mac_address[3] = mac[3];
    mac_address[4] = mac[4];
    mac_address[5] = mac[5];
}

uint8_t *EthernetClass::macAddress() {
    return mac_address;
}

IPAddress EthernetClass::localIP() {
    return {stm32_eth_get_ipaddr()};
}

IPAddress EthernetClass::subnetMask() {
    return {stm32_eth_get_netmaskaddr()};
}

IPAddress EthernetClass::gatewayIP() {
    return {stm32_eth_get_gwaddr()};
}

IPAddress EthernetClass::dnsServerIP() {
    return _dnsServerAddress;
}

#include "ethernetif.h"

extern struct netif gnetif;

void setReboot();

void EthernetClass::reset() {
#ifndef CORE_DEBUG
    if (rstCount++ > 10) {
        setReboot();
    }
#endif

    ip4_addr_t ip;
    ip.addr = uint32_t(local_ip);
    stm32_eth_uninit();
//  memp_init();
    ethernetif_init(&gnetif);
    netif_set_ipaddr(&gnetif, &ip);
}

[[noreturn]] void EthernetClass::thread() {
    rstCount = 0;
    while (true) {
        //网络状态监控
        if ((millis() - net_tick) > 10000) {
            // Ethernet.reset();
            net_tick = millis();
        }
        vTaskDelay(1000);
    }
}

extern ETH_HandleTypeDef heth;

int EthernetClass::diag() {
#ifdef STM32H750xx
    bool IsRxDataAvailable = HAL_ETH_IsRxDataAvailable(&heth);
    if (__HAL_RCC_ETH1MAC_IS_CLK_DISABLED() || __HAL_RCC_ETH1TX_IS_CLK_DISABLED() ||
        __HAL_RCC_ETH1RX_IS_CLK_DISABLED()) {
        core_debug("ETH clk not configed!\n");
    }
    if (__HAL_RCC_ETH1MAC_IS_CLK_SLEEP_DISABLED() || __HAL_RCC_ETH1TX_IS_CLK_SLEEP_DISABLED() ||
        __HAL_RCC_ETH1RX_IS_CLK_SLEEP_DISABLED()) {
        core_debug("ETH SLEEP clk not configed!\n");
    }
#endif
#if defined(DUAL_CORE)
    if(__HAL_RCC_C1_ETH1MAC_CLK_DISABLE()||__HAL_RCC_C1_ETH1TX_CLK_DISABLE()||__HAL_RCC_C1_ETH1RX_CLK_DISABLE()){
        core_debug("ETH clk not configed!\n");
    }
    if(__HAL_RCC_C2_ETH1MAC_CLK_DISABLE()||__HAL_RCC_C2_ETH1TX_CLK_DISABLE()||__HAL_RCC_C2_ETH1RX_CLK_DISABLE()){
        core_debug("ETH clk not configed!\n");
    }
    if(__HAL_RCC_C2_ETH1MAC_CLK_SLEEP_DISABLE()||__HAL_RCC_C2_ETH1TX_CLK_SLEEP_DISABLE()||__HAL_RCC_C2_ETH1RX_CLK_SLEEP_DISABLE()){
        core_debug("ETH clk not configed!\n");
    }
#endif

    uint32_t State = HAL_ETH_GetState(&heth);
#ifdef STM32H750xx
    uint32_t Error = HAL_ETH_GetError(&heth);
    uint32_t DMAError = HAL_ETH_GetDMAError(&heth);
    uint32_t MACError = HAL_ETH_GetMACError(&heth);
    if ((State > HAL_ETH_STATE_ERROR) || Error || DMAError || MACError) {
        core_debug("ETH ERROR:State=0x%xError=0x%x,DMAError=0x%x,MACError=0x%x\n", State, Error, DMAError, MACError);
        return -1;
    }
#endif
    //@todo MPU保护检查
    return 0;
}

EthernetClass Ethernet;