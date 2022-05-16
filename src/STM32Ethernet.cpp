#include "STM32Ethernet.h"

int EthernetClass::begin(unsigned long timeout, unsigned long responseTimeout)
{
  static DhcpClass s_dhcp;
    _dhcp = &s_dhcp;
    stm32_eth_init(MACAddressDefault(), nullptr, nullptr, nullptr);

  // Now try to get our config info from a DHCP server
  int ret = _dhcp->beginWithDHCP(mac_address, timeout, responseTimeout);
  if (ret == 1) {
    _dnsServerAddress = _dhcp->getDnsServerIp();
  }

  return ret;
}

void EthernetClass::begin(IPAddress local_ip)
{
  IPAddress subnet(255, 255, 255, 0);
  begin(local_ip, subnet);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet)
{
  // Assume the gateway will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress gateway = local_ip;
  gateway[3] = 1;
  begin(local_ip, subnet, gateway);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway) {
    // Assume the DNS server will be the same machine than gateway
    begin(local_ip, subnet, gateway, gateway);
}

void EthernetClass::set_ip(IPAddress &local_ip_) {
    extern struct netif gnetif;
    ip4_addr_t ip;
    ip.addr = uint32_t(local_ip_);
    netif_set_ipaddr(&gnetif, &ip);
}

void EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway, IPAddress dns_server) {
    stm32_eth_init(MACAddressDefault(), local_ip.raw_address(), gateway.raw_address(), subnet.raw_address());
    /* If there is a local DHCP informs it of our manual IP configuration to
    prevent IP conflict */
    stm32_DHCP_manual_config();
    _dnsServerAddress = dns_server;
}

int EthernetClass::begin(uint8_t *mac_address, unsigned long timeout, unsigned long responseTimeout)
{
  static DhcpClass s_dhcp;
  _dhcp = &s_dhcp;

    stm32_eth_init(mac_address, nullptr, nullptr, nullptr);

  // Now try to get our config info from a DHCP server
  int ret = _dhcp->beginWithDHCP(mac_address, timeout, responseTimeout);
  if (ret == 1) {
    _dnsServerAddress = _dhcp->getDnsServerIp();
  }
  MACAddress(mac_address);
  return ret;
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip)
{
  // Assume the DNS server will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress dns_server = local_ip;
  dns_server[3] = 1;
  begin(mac_address, local_ip, dns_server);
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server)
{
  // Assume the gateway will be the machine on the same network as the local IP
  // but with last octet being '1'
  IPAddress gateway = local_ip;
  gateway[3] = 1;
  begin(mac_address, local_ip, dns_server, gateway);
}

void EthernetClass::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway)
{
  IPAddress subnet(255, 255, 255, 0);
  begin(mac_address, local_ip, dns_server, gateway, subnet);
}

void EthernetClass::begin(uint8_t *mac, IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet)
{
  stm32_eth_init(mac, local_ip.raw_address(), gateway.raw_address(), subnet.raw_address());
  /* If there is a local DHCP informs it of our manual IP configuration to
  prevent IP conflict */
  stm32_DHCP_manual_config();
  _dnsServerAddress = dns_server;
  MACAddress(mac);
}

EthernetLinkStatus EthernetClass::linkStatus()
{
  return (!stm32_eth_is_init()) ? Unknown : (stm32_eth_link_up() ? LinkON : LinkOFF);
}

int EthernetClass::maintain()
{
  int rc = DHCP_CHECK_NONE;

    if (_dhcp != nullptr) {
        //we have a pointer to dhcp, use it
        rc = _dhcp->checkLease();
        switch (rc) {
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
}

/*
 * This function updates the LwIP stack and can be called to be sure to update
 * the stack (e.g. in case of a long loop).
 */
void EthernetClass::schedule(void)
{
  stm32_eth_scheduler();
}

uint8_t *EthernetClass::MACAddressDefault(void)
{
  if ((mac_address[0] + mac_address[1] + mac_address[2] + mac_address[3] + mac_address[4] + mac_address[5]) == 0) {
    uint32_t baseUID = *(uint32_t *)UID_BASE;
    mac_address[0] = 0x00;
    mac_address[1] = 0x80;
    mac_address[2] = 0xE1;
    mac_address[3] = (baseUID & 0x00FF0000) >> 16;
    mac_address[4] = (baseUID & 0x0000FF00) >> 8;
    mac_address[5] = (baseUID & 0x000000FF);
  }
  return mac_address;
}

void EthernetClass::MACAddress(uint8_t *mac)
{
  mac_address[0] = mac[0];
  mac_address[1] = mac[1];
  mac_address[2] = mac[2];
  mac_address[3] = mac[3];
  mac_address[4] = mac[4];
  mac_address[5] = mac[5];
}

uint8_t *EthernetClass::MACAddress(void)
{
  return mac_address;
}

IPAddress EthernetClass::localIP()
{
  return IPAddress(stm32_eth_get_ipaddr());
}

IPAddress EthernetClass::subnetMask()
{
  return IPAddress(stm32_eth_get_netmaskaddr());
}

IPAddress EthernetClass::gatewayIP()
{
  return IPAddress(stm32_eth_get_gwaddr());
}

IPAddress EthernetClass::dnsServerIP()
{
  return _dnsServerAddress;
}

extern struct netif gnetif;

extern ETH_HandleTypeDef heth;

#include "logger_rte.h"

int EthernetClass::diag(u32 tick)
{
    int res = 0;
#ifdef STM32H750xx
    bool IsRxDataAvailable = HAL_ETH_IsRxDataAvailable(&heth);
    if (__HAL_RCC_ETH1MAC_IS_CLK_DISABLED() || __HAL_RCC_ETH1TX_IS_CLK_DISABLED() ||
        __HAL_RCC_ETH1RX_IS_CLK_DISABLED())
    {
        logger.error("ETH clk not configed!\n");
    }
    if (__HAL_RCC_ETH1MAC_IS_CLK_SLEEP_DISABLED() || __HAL_RCC_ETH1TX_IS_CLK_SLEEP_DISABLED() ||
        __HAL_RCC_ETH1RX_IS_CLK_SLEEP_DISABLED())
    {
        logger.error("ETH SLEEP clk not configed!\n");
    }
#endif
#if defined(DUAL_CORE)
    if (__HAL_RCC_C1_ETH1MAC_CLK_DISABLE() || __HAL_RCC_C1_ETH1TX_CLK_DISABLE() || __HAL_RCC_C1_ETH1RX_CLK_DISABLE())
    {
        logger.error("ETH clk not configed!\n");
    }
    if (__HAL_RCC_C2_ETH1MAC_CLK_DISABLE() || __HAL_RCC_C2_ETH1TX_CLK_DISABLE() || __HAL_RCC_C2_ETH1RX_CLK_DISABLE())
    {
        logger.error("ETH clk not configed!\n");
    }
    if (__HAL_RCC_C2_ETH1MAC_CLK_SLEEP_DISABLE() || __HAL_RCC_C2_ETH1TX_CLK_SLEEP_DISABLE() || __HAL_RCC_C2_ETH1RX_CLK_SLEEP_DISABLE())
    {
        logger.error("ETH clk not configed!\n");
    }
#endif
#ifdef STM32F4xx
    if ((heth.Instance->MACDBGR & ETH_MACDBGR_MMRPEA) == 0)
    {
        logger.error("MAC MII receive protocol engine not active");
    }
    if ((heth.Instance->MACDBGR & ETH_MACDBGR_RFWRA) == 0)
    {
        logger.error("Rx FIFO write controller not active");
    }
    if ((heth.Instance->MACDBGR & ETH_MACDBGR_RFFL_FULL) == ETH_MACDBGR_RFFL_FULL)
    {
        logger.error("RxFIFO full");
    }
    if ((heth.Instance->MACDBGR & ETH_MACDBGR_MTP) == ETH_MACDBGR_MTP)
    {
        logger.error("MAC transmitter in pause");
    }
    if ((heth.Instance->MACDBGR & ETH_MACDBGR_TFF) == ETH_MACDBGR_TFF)
    {
        logger.error("Tx FIFO full");
    }
    u32 irq_act = HAL_NVIC_GetActive(ETH_IRQn);
    if (irq_act)
    {
        logger.error("ETH ERROR:HAL_NVIC_GetActive=0x%x\n", irq_act);
    }
    u32 irq_mask = HAL_NVIC_GetPendingIRQ(ETH_IRQn);
    if (irq_mask)
    {
        logger.error("ETH ERROR:HAL_NVIC_GetPendingIRQ=0x%x\n", irq_mask);
    }
#endif
    uint32_t State = HAL_ETH_GetState(&heth);

    if (State > HAL_ETH_STATE_ERROR)
    {
        logger.error("ETH ERROR:State=0x%x\n", State);
        res = -1;
    }

#ifdef STM32H750xx
    uint32_t Error = HAL_ETH_GetError(&heth);
    uint32_t DMAError = HAL_ETH_GetDMAError(&heth);
    uint32_t MACError = HAL_ETH_GetMACError(&heth);
    if ((State > HAL_ETH_STATE_ERROR) || Error || DMAError || MACError)
    {
        logger.error("ETH ERROR:State=0x%x ,Error=0x%x,DMAError=0x%x,MACError=0x%x\n", State, Error, DMAError,
                     MACError);
        res = -2;
    }
#endif
    return res;
}

void EthernetClass::end()
{
    // HAL_ETH_Stop_IT(&heth);
    // netif_set_down(&gnetif);
    netif_set_link_down(&gnetif);
}

EthernetClass Ethernet;