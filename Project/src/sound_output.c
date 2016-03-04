/*******************************************************************************
* File:    sound_output.c
* Purpose: Provide functionality to output sound.
* Authors: Christian Cleveland and Natalie Astorga
*******************************************************************************/
#include <includes.h>

#define DAC_DHR12R2_ADDRESS    0x40007414

DAC_InitTypeDef  DAC_InitStructure;

const uint16_t Sine12bit[32] = {
                      2047, 2447, 2831, 3185, 3498, 3750, 3939, 4056, 4095, 4056,
                      3939, 3750, 3495, 3185, 2831, 2447, 2047, 1647, 1263, 909,
                      599, 344, 155, 38, 0, 38, 155, 344, 599, 909, 1263, 1647};

extern const unsigned char olaf_wav[];

static void soundOutput_GPIO_Init(void);
static void TIM6_Config(void);
static void DAC_Ch2_SineWaveConfig(void);

void sineWave_init(void)
{
  soundOutput_GPIO_Init();
  TIM6_Config();
  DAC_Ch2_SineWaveConfig();
}

static void soundOutput_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  /* DMA1 clock and GPIOA clock enable (to be used with DAC) */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_GPIOA, ENABLE);

  /* DAC Periph clock enable */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

  /* DAC channel 2 (DAC_OUT2 = PA.5) configuration */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  DAC  Channel2 SineWave Configuration
  * @param  None
  * @retval None
  */
static void DAC_Ch2_SineWaveConfig(void)
{
  //DMA_InitTypeDef DMA_InitStructure;

  /* DAC channel2 Configuration */
  //DAC_InitStructure.DAC_Trigger = DAC_Trigger_T6_TRGO;
  DAC_InitStructure.DAC_Trigger = DAC_Trigger_None;
  DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
  DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
  DAC_Init(DAC_Channel_2, &DAC_InitStructure);



  /* DMA1_Stream6 channel7 configuration **************************************/
  //DMA_DeInit(DMA1_Stream6);
  //DMA_InitStructure.DMA_Channel = DMA_Channel_7;
  //DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)DAC_DHR12R2_ADDRESS;
  ////DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Sine12bit;
  //DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&olaf_wav;
  //DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
  ////DMA_InitStructure.DMA_BufferSize = 32;
  //DMA_InitStructure.DMA_BufferSize = 576078/2;
  //DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  //DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  //DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  //DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  //DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
  //DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  //DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
  //DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
  //DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
  //DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
  //DMA_Init(DMA1_Stream6, &DMA_InitStructure);

  /* Enable DMA1_Stream5 */
  //DMA_Cmd(DMA1_Stream6, ENABLE);

  /* Enable DAC Channel2 */
  DAC_Cmd(DAC_Channel_2, ENABLE);

  /* Enable DMA for DAC Channel2 */
  //DAC_DMACmd(DAC_Channel_2, ENABLE);
}

/**
  * @brief  TIM6 Configuration
  * @note   TIM6 configuration is based on CPU @144MHz and APB1 @36MHz
  * @note   TIM6 Update event occurs each 36MHz/219 = 164.38 KHz
  * @param  None
  * @retval None
  */
static void TIM6_Config(void)
{
  TIM_TimeBaseInitTypeDef    TIM_TimeBaseStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  /* Enable the TIM6 gloabal Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  /* TIM6 Periph clock enable */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);

  /* Time base configuration */
  TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
  TIM_TimeBaseStructure.TIM_Period = 750*2;
  TIM_TimeBaseStructure.TIM_Prescaler = 0;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);

  /* TIM6 TRGO selection */
  //TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);
  TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);

  /* TIM6 enable counter */
  TIM_Cmd(TIM6, ENABLE);
}