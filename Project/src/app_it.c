/*******************************************************************************
* File:   app_it.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To initialize all interrupt call backs and to contain all the call
*         backs if not already defined somewhere else.
*******************************************************************************/

#include <bsp.h>
#include "stm32f4_discovery_sdio_sd.h"
#include "app_it.h"

void itInit()
{
  BSP_IntInit();

  BSP_IntVectSet(BSP_INT_ID_SDIO, SD_ProcessIRQSrc);
  BSP_IntVectSet(BSP_INT_ID_DMA2_CH3, SD_ProcessDMAIRQ);
}