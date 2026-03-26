#include "Eth_Init.h"
#include "main.h"
#include "string.h"

/* Handles defined here, declared extern in header */
ETH_HandleTypeDef heth;
ETH_TxPacketConfig TxConfig;

/* Ethernet DMA Descriptors - must be placed at specific memory locations */
#if defined ( __ICCARM__ )
#pragma location=0x2007c000
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
#pragma location=0x2007c0a0
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];

#elif defined ( __CC_ARM )
__attribute__((at(0x2007c000))) ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
__attribute__((at(0x2007c0a0))) ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];

#elif defined ( __GNUC__ )
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection")));
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")));
#endif

/**
 * @brief Ethernet peripheral initialization
 * MAC: 00:80:E1:00:00:00 (default ST MAC)
 * Mode: RMII
 */
void MX_ETH_Init(void)
{
    static uint8_t MACAddr[6];

    heth.Instance        = ETH;
    MACAddr[0] = 0x00;
    MACAddr[1] = 0x80;
    MACAddr[2] = 0xE1;
    MACAddr[3] = 0x00;
    MACAddr[4] = 0x00;
    MACAddr[5] = 0x00;
    heth.Init.MACAddr       = &MACAddr[0];
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc        = DMATxDscrTab;
    heth.Init.RxDesc        = DMARxDscrTab;
    heth.Init.RxBuffLen     = 1524;

    if (HAL_ETH_Init(&heth) != HAL_OK)
    {
        Error_Handler();
    }

    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes    = ETH_TX_PACKETS_FEATURES_CSUM |
                             ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl  = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl    = ETH_CRC_PAD_INSERT;
}
