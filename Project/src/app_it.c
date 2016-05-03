/*******************************************************************************
* File:   app_it.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To initialize all interrupt call backs and to contain all the call
*         backs if not already defined somewhere else.
*******************************************************************************/
#include <includes.h>
#include <bsp.h>
#include "stm32f4_discovery_sdio_sd.h"
#include "app_it.h"

//extern const unsigned char olaf_wav[];
extern uint8_t KeyPressFlg;
extern OS_EVENT *emergencySem;
extern uint8_t appFlags;

void itInit()
{
  BSP_IntInit();

  BSP_IntVectSet(BSP_INT_ID_SDIO, SD_ProcessIRQSrc);
  BSP_IntVectSet(BSP_INT_ID_DMA2_CH3, SD_ProcessDMAIRQ);
  BSP_IntVectSet(BSP_INT_ID_TIM6, DAC_TIM_IRQ);
  BSP_IntVectSet(BSP_INT_ID_EXTI0, Button_Handler);
  BSP_IntVectSet(BSP_INT_ID_USART1, Modem_USART1_IRQ);
  BSP_IntVectSet(BSP_INT_ID_EXTI4, LSM303_Acc_ISR);
  BSP_IntVectSet(BSP_INT_ID_EXTI15_10, Button_Handler);

  //  BSP_IntVectSet(BSP_INT_ID_EXTI15_10, TouchIRQ);
}

//void TouchIRQ()
//{
//  EXTI_ClearITPendingBit(EXTI_Line13);
//  touched = 1;
//  IOE_ClearGITPending(IOE_1_ADDR, IOE_GIT_TOUCH | IOE_GIT_FTH);
//}

void Button_Handler(void)
{
  if(EXTI_GetITStatus(EXTI_Line0) != RESET || EXTI_GetITStatus(EXTI_Line11) != RESET)
  {
    // The Blue user button
    if(EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
      /* Clear the EXTI line 0 pending bit */
      EXTI_ClearITPendingBit(EXTI_Line0);
    }

    // The red pepper spray button
    if(EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
      /* Clear the EXTI line 11 pending bit */
      EXTI_ClearITPendingBit(EXTI_Line11);
    }

    appFlags |= EMER_BUTTON;
    if(emergencySem != NULL)
    {
      if(OSSemPost(emergencySem) != OS_ERR_NONE) {while(1);}
    }
    //KeyPressFlg = 1;
  }
}

void DAC_TIM_IRQ()
{
  static uint32_t count = 0;

  if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)
  {
    //Write the little endian sound data to the big endian DAC
    //DAC_SetChannel2Data(DAC_Align_12b_L, (olaf_wav[count+1]<<8) | olaf_wav[count]);

    count += 2;
    if(count >= 576078)
    {
      count = 0;
    }

    TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
  }
}

void LSM303_Acc_ISR()
{
  if(EXTI_GetITStatus(EXTI_Line4) != RESET)
  {
    /* Clear the EXTI line 4 pending bit */
    EXTI_ClearITPendingBit(EXTI_Line4);

    appFlags |= EMER_IMPACT;

    if(OSSemPost(emergencySem) != OS_ERR_NONE) {while(1);}
    // KeyPressFlg = 1;
  }
}
