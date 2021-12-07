#ifdef STM32H750xx

/**
 ******************************************************************************
 * File Name          : ethernetif.c
 * Description        : This file provides code for the configuration
 *                      of the ethernetif.c MiddleWare.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
//#include "main.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "ethernetif.h"
#include "lan8742.h"
#include <string.h>
#include "cmsis_os.h"
#include "lwip/tcpip.h"

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private define ------------------------------------------------------------*/
/* The time to block waiting for input. */
#define TIME_WAITING_FOR_INPUT (portMAX_DELAY)
/* Stack size of the interface thread */
#define INTERFACE_THREAD_STACK_SIZE (1500)
/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* ETH Setting  */
#define ETH_RX_BUFFER_SIZE (1536UL)
#define ETH_DMA_TRANSMIT_TIMEOUT (20U)

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Private variables ---------------------------------------------------------*/
/*
@Note: This interface is implemented to operate in zero-copy mode only:
        - Rx buffers are allocated statically and passed directly to the LwIP stack
          they will return back to DMA after been processed by the stack.
        - Tx Buffers will be allocated from LwIP stack memory heap,
          then passed to ETH HAL driver.

@Notes:
  1.a. ETH DMA Rx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_RX_DESC_CNT in ETH GUI (Rx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h
  1.b. ETH DMA Tx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_TX_DESC_CNT in ETH GUI (Tx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h

  2.a. Rx Buffers number must be between ETH_RX_DESC_CNT and 2*ETH_RX_DESC_CNT
  2.b. Rx Buffers must have the same size: ETH_RX_BUFFER_SIZE, this value must
       passed to ETH DMA in the init field (heth.Init.RxBuffLen)
*/

#if defined(__ICCARM__) /*!< IAR Compiler */

#pragma location = 0x30040000
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
#pragma location = 0x30040060
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */
#pragma location = 0x30040200
uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE]; /* Ethernet Receive Buffers */

#elif defined(__CC_ARM) /* MDK ARM Compiler */

__attribute__((at(0x30040000))) ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];     /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x30040060))) ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];     /* Ethernet Tx DMA Descriptors */
__attribute__((at(0x30040200))) uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE]; /* Ethernet Receive Buffer */

#elif defined(__GNUC__) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection")));    /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")));    /* Ethernet Tx DMA Descriptors */
uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE] __attribute__((section(".RxArraySection"))); /* Ethernet Receive Buffers */

#endif

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

osSemaphoreId RxPktSemaphore = NULL; /* Semaphore to signal incoming packets */

/* Global Ethernet handle */
ETH_HandleTypeDef heth;
ETH_TxPacketConfig TxConfig;

/* Memory Pool Declaration */
LWIP_MEMPOOL_DECLARE(RX_POOL,
                     32, sizeof(struct pbuf_custom), "Zero-copy RX PBUF pool");

/* Private function prototypes -----------------------------------------------*/
int32_t ETH_PHY_IO_Init(void);

int32_t ETH_PHY_IO_DeInit(void);

int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);

int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);

int32_t ETH_PHY_IO_GetTick(void);

lan8742_Object_t LAN8742;
lan8742_IOCtx_t LAN8742_IOCtx = {ETH_PHY_IO_Init,
                                 ETH_PHY_IO_DeInit,
                                 ETH_PHY_IO_WriteReg,
                                 ETH_PHY_IO_ReadReg,
                                 ETH_PHY_IO_GetTick};

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */

/* Private functions ---------------------------------------------------------*/
void pbuf_free_custom(struct pbuf *p);
// void Error_Handler(void);

/**
 * @brief  Ethernet Rx Transfer completed callback
 * @param  heth: ETH handle
 * @retval None
 */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *he) {
    osSemaphoreRelease(RxPktSemaphore);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
/**
 * @brief In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif) {
    HAL_StatusTypeDef hal_eth_init_status = HAL_OK;
    ETH_MACConfigTypeDef MACConf;
    int32_t PHYLinkState;
    uint32_t duplex;
    uint32_t speed = 0;
    /* Start ETH HAL Init */

    heth.Instance = ETH;
#if defined(MAC1) && defined(MAC2) && defined(MAC3) && defined(MAC4) && defined(MAC5) && defined(MAC6)
    netif->hwaddr[0] = MAC1;
    netif->hwaddr[1] = MAC2;
    netif->hwaddr[2] = MAC3;
    netif->hwaddr[3] = MAC4;
    netif->hwaddr[4] = MAC5;
    netif->hwaddr[5] = MAC6;
#else
    uint8_t id[12];
    HAL_GetUID((uint32_t *)id);
    netif->hwaddr[0] = id[0] & 0xfc;
    netif->hwaddr[1] = id[2];
    netif->hwaddr[2] = id[4];
    netif->hwaddr[3] = id[6];
    netif->hwaddr[4] = id[10];
    netif->hwaddr[5] = id[11];
#endif
    heth.Init.MACAddr = netif->hwaddr;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = DMATxDscrTab;
    heth.Init.RxDesc = DMARxDscrTab;
    heth.Init.RxBuffLen = 1524;

    /* USER CODE BEGIN MACADDRESS */

    /* USER CODE END MACADDRESS */
    hal_eth_init_status = HAL_ETH_Init(&heth);
    // mac层接收组播数据
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ETH_MACFilterConfigTypeDef pFilterConfig_test;
    HAL_ETH_GetMACFilterConfig(&heth, &pFilterConfig_test);
    pFilterConfig_test.ReceiveAllMode = ENABLE;
    pFilterConfig_test.PassAllMulticast = ENABLE;
    HAL_ETH_SetMACFilterConfig(&heth, &pFilterConfig_test);

    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    /* End ETH HAL Init */

    /* Initialize the RX POOL */
    LWIP_MEMPOOL_INIT(RX_POOL);

#if LWIP_ARP || LWIP_ETHERNET

    /* set MAC hardware address length */
    netif->hwaddr_len = ETH_HWADDR_LEN;

    /* set MAC hardware address */
    // netif->hwaddr[0] = heth.Init.MACAddr[0];
    // netif->hwaddr[1] = heth.Init.MACAddr[1];
    // netif->hwaddr[2] = heth.Init.MACAddr[2];
    // netif->hwaddr[3] = heth.Init.MACAddr[3];
    // netif->hwaddr[4] = heth.Init.MACAddr[4];
    // netif->hwaddr[5] = heth.Init.MACAddr[5];

    /* maximum transfer unit */
    netif->mtu = ETH_MAX_PAYLOAD;

    /* Accept broadcast address and ARP traffic */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
#if LWIP_ARP
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
#else
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_IGMP;
#endif /* LWIP_ARP */

    for (uint32_t idx = 0; idx < ETH_RX_DESC_CNT; idx++) {
        HAL_ETH_DescAssignMemory(&heth, idx, Rx_Buff[idx], NULL);
    }

    /* create a binary semaphore used for informing ethernetif of frame reception */
    osSemaphoreDef(SEM);
    RxPktSemaphore = osSemaphoreCreate(osSemaphore(SEM), 1);

    /* create the task that handles the ETH_MAC */
    osThreadDef(EthIf, ethernetif_input, osPriorityRealtime, 0, INTERFACE_THREAD_STACK_SIZE);
    osThreadCreate(osThread(EthIf), netif);
    /* USER CODE BEGIN PHY_PRE_CONFIG */

    /* USER CODE END PHY_PRE_CONFIG */
    /* Set PHY IO functions */
    LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);

    /* Initialize the LAN8742 ETH PHY */
    LAN8742_Init(&LAN8742);
    if (hal_eth_init_status == HAL_OK) {
        PHYLinkState = LAN8742_GetLinkState(&LAN8742);

        /* Get link state */
        if (PHYLinkState <= LAN8742_STATUS_LINK_DOWN) {
            netif_set_link_down(netif);
            netif_set_down(netif);
        } else {
            switch (PHYLinkState) {
                case LAN8742_STATUS_100MBITS_FULLDUPLEX:
                    duplex = ETH_FULLDUPLEX_MODE;
                    speed = ETH_SPEED_100M;
                    break;
                case LAN8742_STATUS_100MBITS_HALFDUPLEX:
                    duplex = ETH_HALFDUPLEX_MODE;
                    speed = ETH_SPEED_100M;
                    break;
                case LAN8742_STATUS_10MBITS_FULLDUPLEX:
                    duplex = ETH_FULLDUPLEX_MODE;
                    speed = ETH_SPEED_10M;
                    break;
                case LAN8742_STATUS_10MBITS_HALFDUPLEX:
                    duplex = ETH_HALFDUPLEX_MODE;
                    speed = ETH_SPEED_10M;
                    break;
                default:
                    duplex = ETH_FULLDUPLEX_MODE;
                    speed = ETH_SPEED_100M;
                    break;
            }

            /* Get MAC Config MAC */
            HAL_ETH_GetMACConfig(&heth, &MACConf);
            MACConf.DuplexMode = duplex;
            MACConf.Speed = speed;
            HAL_ETH_SetMACConfig(&heth, &MACConf);

            HAL_ETH_Start_IT(&heth);
            netif_set_up(netif);
            netif_set_link_up(netif);
            /* USER CODE BEGIN PHY_POST_CONFIG */

            /* USER CODE END PHY_POST_CONFIG */
        }
    }
    else
    {
        Error_Handler();
    }
#endif /* LWIP_ARP || LWIP_ETHERNET */

    /* USER CODE BEGIN LOW_LEVEL_INIT */

    /* USER CODE END LOW_LEVEL_INIT */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
    uint32_t i = 0;
    uint32_t framelen = 0;
    err_t errval = ERR_OK;
    ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];

    memset(Txbuffer, 0, ETH_TX_DESC_CNT * sizeof(ETH_BufferTypeDef));

    for (const struct pbuf *q = p; q != NULL; q = q->next) {
        if (i >= ETH_TX_DESC_CNT)
            return ERR_IF;

        Txbuffer[i].buffer = q->payload;
        Txbuffer[i].len = q->len;
        framelen += q->len;

        if (i > 0) {
            Txbuffer[i - 1].next = &Txbuffer[i];
        }

        if (q->next == NULL) {
            Txbuffer[i].next = NULL;
        }

        i++;
    }

    TxConfig.Length = framelen;
    TxConfig.TxBuffer = Txbuffer;
#ifndef D_CACHE_DISABLED
    SCB_CleanInvalidateDCache(); //无效化并清除Dcache
#endif
    HAL_ETH_Transmit(&heth, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT);

    return errval;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *low_level_input(struct netif *netif) {
    struct pbuf *p = NULL;
    ETH_BufferTypeDef RxBuff;
    uint32_t framelength = 0;
    struct pbuf_custom *custom_pbuf;
#ifndef D_CACHE_DISABLED
    SCB_CleanInvalidateDCache(); //无效化并且清除Dcache
#endif
    if (HAL_ETH_GetRxDataBuffer(&heth, &RxBuff) == HAL_OK) {
        HAL_ETH_GetRxDataLength(&heth, &framelength);

        /* Build Rx descriptor to be ready for next data reception */
        HAL_ETH_BuildRxDescriptors(&heth);

#if !defined(DUAL_CORE) || defined(CORE_CM7)
        /* Invalidate data cache for ETH Rx Buffers */
        SCB_InvalidateDCache_by_Addr((uint32_t *) RxBuff.buffer, framelength);
#endif

        custom_pbuf = (struct pbuf_custom *) LWIP_MEMPOOL_ALLOC(RX_POOL);
        LWIP_ASSERT("RX_POOL_alloc: custom_pbuf != NULL", custom_pbuf != NULL);
        custom_pbuf->custom_free_function = pbuf_free_custom;

        p = pbuf_alloced_custom(PBUF_RAW, (uint16_t) framelength, PBUF_REF, custom_pbuf, RxBuff.buffer,
                                ETH_RX_BUFFER_SIZE);
    }

    return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void ethernetif_input(void const *argument)
{
    struct pbuf *p;
    const struct netif *netif = (const struct netif *) argument;

    for (;;)
    {
        if (osSemaphoreWait(RxPktSemaphore, TIME_WAITING_FOR_INPUT) == osOK)
        {
            do
            {
                LOCK_TCPIP_CORE();

                p = low_level_input(netif);
                if (p != NULL)
                {
                    if (netif->input(p, netif) != ERR_OK)
                    {
                        pbuf_free(p);
                    }
                }

                UNLOCK_TCPIP_CORE();

            } while (p != NULL);
        }
    }
}

#if !LWIP_ARP

/**
 * This function has to be completed by user in case of ARP OFF.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if ...
 */
static err_t low_level_output_arp_off(struct netif *netif, struct pbuf *q, const ip4_addr_t *ipaddr)
{
    err_t errval;
    errval = ERR_OK;

    /* USER CODE BEGIN 5 */

    /* USER CODE END 5 */

    return errval;
}

#endif /* LWIP_ARP */

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */

#if LWIP_IPV4
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
    netif->output = etharp_output;
#else
    /* The user should write ist own code in low_level_output_arp_off function */
    netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */
#endif /* LWIP_ARP || LWIP_ETHERNET */
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

    netif->linkoutput = low_level_output;

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

/**
 * @brief  Custom Rx pbuf free callback
 * @param  pbuf: pbuf to be freed
 * @retval None
 */
void pbuf_free_custom(struct pbuf *p)
{
    struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;

#if !defined(DUAL_CORE) || defined(CORE_CM7)
    /* Invalidate data cache: lwIP and/or application may have written into buffer */
    SCB_InvalidateDCache_by_Addr((uint32_t *)p->payload, p->tot_len);
#endif

    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
}

/* USER CODE BEGIN 6 */

/**
 * @brief  Returns the current time in milliseconds
 *         when LWIP_TIMERS == 1 and NO_SYS == 1
 * @param  None
 * @retval Current Time value
 */
u32_t sys_jiffies(void)
{
    return HAL_GetTick();
}

/**
 * @brief  Returns the current time in milliseconds
 *         when LWIP_TIMERS == 1 and NO_SYS == 1
 * @param  None
 * @retval Current Time value
 */
u32_t sys_now(void)
{
    return HAL_GetTick();
}

/* USER CODE END 6 */

/*******************************************************************************
                       PHI IO Functions
*******************************************************************************/
/**
 * @brief  Initializes the MDIO interface GPIO and clocks.
 * @param  None
 * @retval 0 if OK, -1 if ERROR
 */
int32_t ETH_PHY_IO_Init(void)
{
    /* We assume that MDIO GPIO configuration is already done
       in the ETH_MspInit() else it should be done here
    */

    /* Configure the MDIO Clock */
    HAL_ETH_SetMDIOClockRange(&heth);

    return 0;
}

/**
 * @brief  De-Initializes the MDIO interface .
 * @param  None
 * @retval 0 if OK, -1 if ERROR
 */
int32_t ETH_PHY_IO_DeInit(void)
{
    return 0;
}

/**
 * @brief  Read a PHY register through the MDIO interface.
 * @param  DevAddr: PHY port address
 * @param  RegAddr: PHY register address
 * @param  pRegVal: pointer to hold the register value
 * @retval 0 if OK -1 if Error
 */
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
    if (HAL_ETH_ReadPHYRegister(&heth, DevAddr, RegAddr, pRegVal) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  Write a value to a PHY register through the MDIO interface.
 * @param  DevAddr: PHY port address
 * @param  RegAddr: PHY register address
 * @param  RegVal: Value to be written
 * @retval 0 if OK -1 if Error
 */
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
    if (HAL_ETH_WritePHYRegister(&heth, DevAddr, RegAddr, RegVal) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  Get the time in millisecons used for internal PHY driver process.
 * @retval Time value
 */
int32_t ETH_PHY_IO_GetTick(void)
{
    return HAL_GetTick();
}

/**
 * @brief  Check the ETH link state then update ETH driver and netif link accordingly.
 * @param  argument: netif
 * @retval None
 */
void ethernet_link_thread(void const *argument) {
    ETH_MACConfigTypeDef MACConf;
    uint32_t PHYLinkState;
    uint32_t duplex = 0;
    uint32_t linkchanged = 0;
    uint32_t speed = 0;

    const struct netif *netif = (const struct netif *) argument;

    for (;;) {
        PHYLinkState = LAN8742_GetLinkState(&LAN8742);

        if (netif_is_link_up(netif) && (PHYLinkState <= LAN8742_STATUS_LINK_DOWN)) {
            HAL_ETH_Stop_IT(&heth);
            netif_set_down(netif);
            netif_set_link_down(netif);
        } else if (!netif_is_link_up(netif) && (PHYLinkState > LAN8742_STATUS_LINK_DOWN)) {
            switch (PHYLinkState) {
                case LAN8742_STATUS_100MBITS_FULLDUPLEX:
                    duplex = ETH_FULLDUPLEX_MODE;
                    speed = ETH_SPEED_100M;
                    linkchanged = 1;
                    break;
                case LAN8742_STATUS_100MBITS_HALFDUPLEX:
                    duplex = ETH_HALFDUPLEX_MODE;
                    speed = ETH_SPEED_100M;
                    linkchanged = 1;
                    break;
                case LAN8742_STATUS_10MBITS_FULLDUPLEX:
                    duplex = ETH_FULLDUPLEX_MODE;
                    speed = ETH_SPEED_10M;
                    linkchanged = 1;
                    break;
                case LAN8742_STATUS_10MBITS_HALFDUPLEX:
                    duplex = ETH_HALFDUPLEX_MODE;
                    speed = ETH_SPEED_10M;
                    linkchanged = 1;
                    break;
                default:
                    break;
            }

            if (linkchanged) {
                /* Get MAC Config MAC */
                HAL_ETH_GetMACConfig(&heth, &MACConf);
                MACConf.DuplexMode = duplex;
                MACConf.Speed = speed;
                HAL_ETH_SetMACConfig(&heth, &MACConf);

                HAL_ETH_Start_IT(&heth);
                netif_set_up(netif);
                netif_set_link_up(netif);
            }

            /* USER CODE BEGIN ETH link code for User BSP */

            /* USER CODE END ETH link code for User BSP */
        }

        /* USER CODE BEGIN ETH link Thread core code for User BSP */

        /* Restart MAC interface */

        osDelay(100);
        /* Stop MAC interface */
    }
}

void ethernetif_update_config(struct netif *netif) {

    if (netif_is_link_up(netif)) {
        /* Restart MAC interface */
        HAL_ETH_Start(&heth);
    } else {
        /* Stop MAC interface */
        HAL_ETH_Stop(&heth);
    }

    ethernetif_notify_conn_changed(netif);
}

/* USER CODE BEGIN 8 */
void stm32_eth_uninit()
{
    HAL_ETH_Stop(&heth);
    eth_reset();
    //  if (t_xThread)
    //    osThreadTerminate(t_xThread);
    //  if (s_xSemaphore)
    //  {
    //    osSemaphoreDelete(s_xSemaphore);
    //    s_xSemaphore = NULL;
    //  }
}

void ETH_IRQHandler(void)
{
    /* USER CODE BEGIN ETH_IRQn 0 */

    /* USER CODE END ETH_IRQn 0 */
    HAL_ETH_IRQHandler(&heth);
    /* USER CODE BEGIN ETH_IRQn 1 */

    /* USER CODE END ETH_IRQn 1 */
}
/* USER CODE END 8 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

#endif