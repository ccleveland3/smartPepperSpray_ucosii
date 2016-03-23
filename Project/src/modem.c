/*******************************************************************************
* File:   modem.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To provide functionality to the 4G LTE and GPS modem LE910-SVG
*******************************************************************************/
#include <includes.h>

#define RX_BUFFER_SIZE 255
#define RX_CIRC_INC(x) ((x + 1) % RX_BUFFER_SIZE)

static volatile char rx_buffer[RX_BUFFER_SIZE] = {0};
static volatile int rx_in  = 0;
static volatile int rx_out = 0;

char rx_line[RX_BUFFER_SIZE] = {0};


static void modemSend(const char *str);
static int waitResponse(const char *target, int numChar);



void modemInit()
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  
  /* Enable GPIO clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  
  /* Enable UART clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
  
  /* Connect PA9 to USART1_Tx*/
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
  
  /* Connect PA10 to USART1_Rx*/
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
  
  /* Connect PA11 to USART1_CTS*/
  //GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_USART1);
  
  /* Connect PA12 to USART1_RTS*/
  //GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_USART1);
  
  /* Configure USART Tx, Rx, CTS, and RTS as alternate function  */
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2  | GPIO_Pin_3;// |
    //GPIO_Pin_11 | GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;//GPIO_PuPd_UP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  /* Configure the ON_OFF pin for the modem
  * It is very important that the output type be open drain because a high is
  * 1.8V for the modem.  The other inputs/outputs are level shifted.
  */
//  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
//  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
//  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
//  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL; //The modem pulls up
//  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  /* USARTx configured as follow:
  - BaudRate = 115200 baud  
  - Word Length = 8 Bits
  - One Stop Bit
  - No parity
  - Hardware flow control enabled (RTS and CTS signals)
  - Receive and transmit enabled
  */
  USART_InitStructure.USART_BaudRate            = 115200;
  USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits            = USART_StopBits_1;
  USART_InitStructure.USART_Parity              = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  //USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
  USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
  
// STM_EVAL_COMInit(COM1, &USART_InitStructure);
  
  /* USART configuration */
  USART_Init(USART2, &USART_InitStructure);
  
  /* Enable USART */
  USART_Cmd(USART2, ENABLE);
  
   /* Enable the Receive Data register not empty interrupt */
  USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
  
  /* Enable the USART1 gloabal Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  
 
  
//  /* Turn Modem on */
//  GPIO_ResetBits(GPIOA, GPIO_Pin_8);
//  OSTimeDlyHMSM(0, 0, 2, 0);
//  GPIO_SetBits(GPIOA, GPIO_Pin_8);
//  OSTimeDlyHMSM(0, 0, 2, 0);
//  GPIO_ResetBits(GPIOA, GPIO_Pin_8);

  /* Turn off echo */
  modemSend("ATE0\r\n");
  waitResponse("OK",2);
}

static void modemSend(const char *str)
{
  for(int i = 0; str[i] != 0; i++)
  {
    USART_SendData(USART2, (uint8_t) str[i]);
    
    /* Loop until the end of transmission */
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
    {}
  }
}

static int waitResponse(const char *target, int numChar)
{
  do{
    int i = 0;
    int j;
    int k;
    USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
    while( i == 0 || (rx_line[i-1] != '\n'))
    {
      j = rx_in;
      k = rx_out;
      if (j == k)
      {
        USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
        do {
           j = rx_in;
           k = rx_out;
        } while( j == k);
        USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
      }
      rx_line[i] = rx_buffer[rx_out];
      i++;
      rx_out = (rx_out+1) % RX_BUFFER_SIZE;
    }
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    rx_line[i-1] = 0;
  } while(strncmp(rx_line,target,numChar));
  return 0;
}

void Modem_USART2_IRQ()
{
  if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
  {
    while((USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == SET) && (RX_CIRC_INC(rx_in) != rx_out))
    {
      rx_buffer[rx_in] = (char)USART_ReceiveData(USART2);
      rx_in = RX_CIRC_INC(rx_in);
    }
  }
}