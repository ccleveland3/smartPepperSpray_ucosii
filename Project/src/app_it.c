/*******************************************************************************
* File:   app_it.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To initialize all interrupt call backs and to contain all the call
*         backs if not already defined somewhere else.
*******************************************************************************/

#include <bsp.h>
#include "stm32f4_discovery_sdio_sd.h"
#include "app_it.h"

extern const unsigned char olaf_wav[];

void itInit()
{
  BSP_IntInit();

  BSP_IntVectSet(BSP_INT_ID_SDIO, SD_ProcessIRQSrc);
  BSP_IntVectSet(BSP_INT_ID_DMA2_CH3, SD_ProcessDMAIRQ);
  BSP_IntVectSet(BSP_INT_ID_TIM6, DAC_TIM_IRQ);
}

void DAC_TIM_IRQ()
{
  static uint32_t count = 0;

  if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)
  {
    //Write the little endian sound data to the big endian DAC
    DAC_SetChannel2Data(DAC_Align_12b_L, (olaf_wav[count+1]<<8) | olaf_wav[count]);

    count += 2;
    if(count >= 576078)
    {
      count = 0;
    }

    TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
  }

}