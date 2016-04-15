/**
******************************************************************************
* <h2><center>&copy; COPYRIGHT 2012 Embest Tech. Co., Ltd.</center></h2>
* @file    app.c
* @author  CMP Team
* @version V1.0.0
* @date    28-December-2012
* @brief   Main program body.
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, Embest SHALL NOT BE HELD LIABLE FOR ANY DIRECT, INDIRECT
* OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE CONTENT
* OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING INFORMATION
* CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/

#include <includes.h>


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define LED_DLYCOUNT_MAX     500
#define LED_DLYCOUNT_MIN     50
#define LED_DLYCOUNT_STEP    50
#define DCMI_DR_ADDRESS     0x50050028
#define FSMC_LCD_ADDRESS    0x60100000

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static  OS_STK         App_TaskStartStk[APP_TASK_START_STK_SIZE];
static  OS_STK         App_TaskKbdStk[APP_TASK_KBD_STK_SIZE];
static  OS_STK         App_TouchTaskStk[APP_TASK_KBD_STK_SIZE];
static  CPU_INT16U     led_dly_cout = 100;
EXTI_InitTypeDef   EXTI_InitStructure;

SD_Error Status = SD_OK;
FATFS filesystem;		/* volume lable */
FRESULT ret;			/* Result code */
FIL file;			/* File object */
FIL MyFile, MyFile1;
DIR dir;			/* Directory object */
FILINFO fno;			/* File information object */
UINT bw, br;
uint8_t buff[128];
uint8_t touched = 0;
uint8_t KeyPressFlg = 0;
uint8_t capture_Flag = ENABLE;
uint8_t   _aucLine[1024];
RGB_typedef *RGB_matrix;
uint32_t line_counter = 0;



//static TS_STATE ts_State;

/* Private function prototypes -----------------------------------------------*/
static  void  App_TaskCreate       (void);
static  void  App_EventCreate      (void);
static  void  App_TaskStart        (void    *p_arg);
static  void  App_TaskKbd          (void    *p_arg);
static  void  App_TouchTask        (void *p_arg);
static  void  fault_err            (FRESULT rc);
static void LCD_Display(void);
static void KeyPad(void);
static void Touch(void);
uint8_t DCMI_OV9655Config(void);
void EXTILine0_Config(void);
void DCMI_Config(void);
void I2C1_Config(void);
void Camera(void);
static uint8_t Jpeg_CallbackFunction(uint8_t* Row, uint32_t DataLength);

/* Private functions ---------------------------------------------------------*/
/**
* @brief  Main program.
* @param  None
* @retval None
*/

int  main (void)
{
  CPU_INT08U  os_err;
  /* Disable all ints until we are ready to accept them. */
  BSP_IntDisAll();
  /* Initialize "uC/OS-II, The Real-Time Kernel".*/
  OSInit();
  /* Create the start task.*/

  os_err = OSTaskCreateExt((void (*)(void *)) App_TaskStart,
                           (void          * ) 0,
                           (OS_STK        * )&App_TaskStartStk[APP_TASK_START_STK_SIZE - 1],
                           (INT8U           ) APP_TASK_START_PRIO,
                           (INT16U          ) APP_TASK_START_PRIO,
                           (OS_STK        * )&App_TaskStartStk[0],
                           (INT32U          ) APP_TASK_START_STK_SIZE,
                           (void          * )0,
                           (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

#if (OS_TASK_NAME_SIZE >= 11)
  OSTaskNameSet(APP_TASK_START_PRIO, (CPU_INT08U *)"Start Task", &os_err);
#endif
  /* Start multitasking (i.e. give control to uC/OS-II).*/
  OSStart();

  return (0);
}

void jpeg_test()
{
	if (f_mount(0, &filesystem) == FR_OK)
	{
    //printf("could not open filesystem \n\r");
	  if(f_open(&MyFile1, "image.jpg", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
    {
      /*##-5- Open the BMP image with read access ##########################*/
      if(f_open(&MyFile, "image.bmp", FA_READ) == FR_OK)
      {         
        /*##-6- Jpeg encoding ##############################################*/
        jpeg_encode(&MyFile, &MyFile1, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_QUALITY, _aucLine);
				//jpeg_decode(&MyFile1, IMAGE_WIDTH, _aucLine, Jpeg_CallbackFunction);
				//jpeg_decode(&MyFile1, IMAGE_WIDTH, _aucLine, Jpeg_CallbackFunction);
                   
        /* Close the BMP and JPEG files */
        f_close(&MyFile1);
        f_close(&MyFile);
          
        /*##-7- Jpeg decoding ##############################################*/
        /* Open the JPEG file for read */
        if(f_open(&MyFile1, "image.jpg", FA_READ) == FR_OK)
        {          
          /* Jpeg Decoding for display to LCD */
          jpeg_decode(&MyFile1, IMAGE_WIDTH, _aucLine, Jpeg_CallbackFunction);
            
          /* Close the JPEG file */
          f_close(&MyFile1);   
        }
      }
    }
		f_mount(0, NULL);
  }
}

/**
* @brief  The startup task.  The uC/OS-II ticker should only be initialize
*         once multitasking starts.
* @param  p_arg  Argument passed to 'App_TaskStart()' by 'OSTaskCreate()'.
* @retval None
*/
static  void  App_TaskStart (void *p_arg)
{
  (void)p_arg;
  //float pfData[3] = {0};

  /* Initialize BSP functions. */
  BSP_Init();
  /* Initialize the SysTick. */
  OS_CPU_SysTickInit();
  /* Initialize external interrupt callbacks */
  itInit();
    EXTILine0_Config();
 DCMI_Control_IO_Init();
	LCD_Display();
  //sineWave_init();

#if (OS_TASK_STAT_EN > 0)
  /* Determine CPU capacity. */
  OSStatInit();
#endif
  /* Create application events. */
  App_EventCreate();
  /* Create application tasks. */
  App_TaskCreate();

  /* mount the filesystem */
//  if (f_mount(0, &filesystem) != FR_OK) {
    //printf("could not open filesystem \n\r");
//  }
	
  OSTimeDlyHMSM(0, 0, 0, 10);
//  WaveRecorderUpdate();
//
//  accelerometerCompassInit();
//  LSM303DLHC_CompassReadAcc(pfData);
//  LSM303DLHC_CompassReadMag(pfData);

  while (DEF_TRUE)
  {
    //STM_EVAL_LEDToggle(LED4);
    OSTimeDlyHMSM(0, 0, 0, led_dly_cout);
    //STM_EVAL_LEDToggle(LED6);
    OSTimeDlyHMSM(0, 0, 0, led_dly_cout);
    //STM_EVAL_LEDToggle(LED5);
    OSTimeDlyHMSM(0, 0, 0, led_dly_cout);
    //STM_EVAL_LEDToggle(LED3);
    OSTimeDlyHMSM(0, 0, 0, led_dly_cout);
  }
}

static  void  App_TouchTask (void *p_arg)
{  
  (void)p_arg;
  
  
  //LCD_DisplayStringLine(4, (uint8_t *)"Modem Init");
  //if(!modemInit())
	//{
	//	LCD_DisplayStringLine(4, (uint8_t *)"Modem Init Failed!");
	//	while(1);
	//}
  Camera();
	
	LCD_DisplayStringLine(4, (uint8_t *)"Start");
	jpeg_test();
	LCD_DisplayStringLine(4, (uint8_t *)"Done!");
	
	//pictureSend();
  //smsSend();
  OSTimeDlyHMSM(0, 0, 2, 0);
  //KeyPad();
  
  //LCD_DisplayStringLine(0,"Sucess");
  
  
  while(1)
  {
  //  Touch();
    OSTimeDlyHMSM(0, 0, 0, 20);
  }
  
}



static void Camera()
{
   
  /* OV9655 Camera Module configuration */
  if (DCMI_OV9655Config() == 0x00)
  {
   //LCD_DisplayStringLine(LINE(2), "                ");
    LCD_SetDisplayWindow(0, 0, 320, 240);
    LCD_WriteRAM_Prepare();

    /* Start Image capture and Display on the LCD *****************************/
    /* Enable DMA transfer */
    DMA_Cmd(DMA2_Stream1, ENABLE);

    /* Enable DCMI interface */
    DCMI_Cmd(ENABLE); 

    /* Start Image capture */ 
    DCMI_CaptureCmd(ENABLE);   

    /*init the picture count*/
    init_picture_count();

    //KeyPressFlg = 0;
    while (KeyPressFlg == 0)
    {
      /* Insert 100ms delay */
     OSTimeDlyHMSM(0, 0, 0, 100);

      if (KeyPressFlg) {
        KeyPressFlg = 0;
        /* press user KEY take a photo */
        if (capture_Flag == ENABLE) {
          DCMI_CaptureCmd(DISABLE);
          capture_Flag = DISABLE;
          Capture_Image_TO_Bmp();
          LCD_SetDisplayWindow(0, 0, 320, 240);
          LCD_WriteRAM_Prepare();
          //DCMI_CaptureCmd(ENABLE);
          capture_Flag = ENABLE;
        }			
      }
    }
    
  } else {
    LCD_SetTextColor(LCD_COLOR_RED);

    LCD_DisplayStringLine(LINE(2), "Camera Init.. fails");    
    LCD_DisplayStringLine(LINE(4), "Check the Camera HW ");    
    LCD_DisplayStringLine(LINE(5), "  and try again ");

    /* Go to infinite loop */
    while (1);      
  }
}
  
void EXTILine0_Config(void)
{
  
  GPIO_InitTypeDef   GPIO_InitStructure;
  NVIC_InitTypeDef   NVIC_InitStructure;

  /* Enable GPIOA clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  /* Enable SYSCFG clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
  
  /* Configure PA0 pin as input floating */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* Connect EXTI Line0 to PA0 pin */
  SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

  /* Configure EXTI Line0 */
  EXTI_InitStructure.EXTI_Line = EXTI_Line0;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;  
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  /* Enable and set EXTI Line0 Interrupt to the lowest priority */
  NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}



/**
  * @brief  Configures the I2C1 used for OV9655 camera module configuration.
  * @param  None
  * @retval None
  */
void I2C1_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  I2C_InitTypeDef  I2C_InitStruct;

 /* I2C1 clock enable */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
  /* GPIOB clock enable */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE); 

  /* Connect I2C1 pins to AF4 ************************************************/
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_I2C1);  
  
  /* Configure I2C1 GPIOs *****************************************************/  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;   
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /* Configure I2C1 ***********************************************************/  
  /* I2C DeInit */   
  I2C_DeInit(I2C1);
    
  /* Enable the I2C peripheral */
  I2C_Cmd(I2C1, ENABLE);
 
  /* Set the I2C structure parameters */
  I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
  I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
  I2C_InitStruct.I2C_OwnAddress1 = 0xFE;
  I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
  I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_InitStruct.I2C_ClockSpeed = 30000;
  
  /* Initialize the I2C peripheral w/ selected parameters */
  I2C_Init(I2C1, &I2C_InitStruct);
}

static void LCD_Display()
{
  IOE_Config();
  //IOE_ITConfig(IOE_ITSRC_TSC);
  // IOE_IOITConfig(IOE_1_ADDR,0,ENABLE);
  // IOE_EXTI_Config();
  
  /* Initialize the LCD */
  STM32f4_Discovery_LCD_Init();
  /* Clear the LCD */ 
 // LCD_Clear(LCD_COLOR_MAGENTA);
  
  /* Set the LCD Text size */
  LCD_SetFont(&Font16x24);
  
  //Lcd_Touch_Calibration();
  //char array[] = "Insert Email Address";
  //LCD_DisplayStringLine(6, array);
  
  //  while (1) {
  //    Calibration_Test_Dispose();
  //}
}

static void Touch()
{
  int tpx_sum = 0,tpy_sum = 0;
  //uint8_t  status = 0;
  uint8_t i;
  char buffer[50];
  TS_STATE *state = NULL;
  
  OSTimeDlyHMSM(0, 0, 0, 10);
  
  //LCD_SetTextColor(Blue);
  /*Read AD convert result*/
  state = IOE_TS_GetState();
  if(state->TouchDetected)
  {
    for(i = 0; i < 16; i++) {
      
      tpx_sum += IOE_TS_Read_X();
      tpy_sum += IOE_TS_Read_Y();	
      OSTimeDlyHMSM(0, 0, 0, 2);
    }
    //LCD_Clear(LCD_COLOR_WHITE); 
    tpx_sum >>= 4;
    tpy_sum >>= 4;
    
    sprintf(buffer,"x=%d, y=%d",tpx_sum,tpy_sum);
    LCD_DisplayStringLine(0, (uint8_t *)buffer);
  }
}

static void KeyPad()
{
  LCD_Clear(LCD_COLOR_MAGENTA); 
  /* Set the LCD Text size */
  LCD_SetFont(&Font16x24);
  LCD_SetTextColor(LCD_COLOR_BLACK);
  
  LCD_SetBackColor(LCD_COLOR_MAGENTA);
#define x_pos0  12
#define x_pos1  22
#define x_pos2  64
#define x_pos3  79
#define x_pos4 116
#define x_pos5 136
#define x_pos6 193
#define x_pos7 268
#define x_circle_pos0 212
#define x_circle_pos1 197
  
#define y_pos0   12
#define y_pos1   24
#define y_pos2   69
#define y_pos3   76
#define y_pos4  126
#define y_pos5  128
#define y_pos6  183
#define y_pos7  280
#define y_circle_pos 205
  
#define height 40
#define width  45
  
  
  //Rectangles and numbers
  LCD_DrawRect(x_pos0,y_pos0,height,width);
  LCD_DisplayChar(x_pos1,y_pos1,'1');
  LCD_DrawRect(x_pos2,y_pos0,height,width);
  LCD_DisplayChar(x_pos1,y_pos3,'2');
  LCD_DrawRect(x_pos4,y_pos0,height,width);
  LCD_DisplayChar(x_pos1,y_pos5,'3');
  
  
  LCD_DrawRect(x_pos0,y_pos2,height,width);
  LCD_DisplayChar(x_pos3,y_pos1,'4');
  LCD_DrawRect(x_pos2,y_pos2,height,width);
  LCD_DisplayChar(x_pos3,y_pos3,'5');
  LCD_DrawRect(x_pos4,y_pos2,height,width);
  LCD_DisplayChar(x_pos3,y_pos5,'6');
  
  
  LCD_DrawRect(x_pos0,y_pos4,height,width);
  LCD_DisplayChar(x_pos5,y_pos1,'7');
  LCD_DrawRect(x_pos2,y_pos4,height,width);
  LCD_DisplayChar(x_pos5,y_pos2,'8');
  LCD_DrawRect(x_pos4,y_pos4,height,width);
  LCD_DisplayChar(x_pos5,y_pos5,'9');
  
  LCD_DrawRect(x_pos0,y_pos6,height,width);
  LCD_DisplayChar(x_pos6,y_pos1,'*');
  LCD_DrawRect(x_pos2,y_pos6,height,width);
  LCD_DisplayChar(x_pos6,y_pos3,'0');
  LCD_DrawRect(x_pos4,y_pos6,height,width);
  LCD_DisplayChar(x_pos6,y_pos5,'#');
  
  //volume button
  LCD_DrawRect(x_pos7,y_pos4,height,width);
  LCD_DisplayChar(x_pos3,y_pos7,'+');
  LCD_DrawRect(x_pos7,y_pos2,height,width);
  LCD_DisplayChar(x_pos5,y_pos7,'-');
  
  //End call button, Circle 
  LCD_DrawCircle(x_circle_pos0,y_circle_pos,25);
  LCD_DisplayChar(x_circle_pos1,y_circle_pos,'X');
}

/**
  * @brief  Configures the DCMI to interface with the OV9655 camera module.
  * @param  None
  * @retval None
  */
void DCMI_Config(void)
{
  DCMI_InitTypeDef DCMI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  DMA_InitTypeDef  DMA_InitStructure;
  
  /* Enable DCMI GPIOs clocks */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE | 
                         RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOA, ENABLE); 

  /* Enable DCMI clock */
  RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_DCMI, ENABLE);

  /* Connect DCMI pins to AF13 ************************************************/
  /* PCLK */
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_DCMI);
  /* D0-D7 */
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource0, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource1, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource4, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource5, GPIO_AF_DCMI);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource6, GPIO_AF_DCMI);
  /* VSYNC */
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_DCMI);
  /* HSYNC */
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_DCMI);
  
  /* DCMI GPIO configuration **************************************************/
  /* D0 D1(PC6/7) */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;  
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  /* D2..D4(PE0/1/4) D6/D7(PE5/6) */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 
	                              | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
  GPIO_Init(GPIOE, &GPIO_InitStructure);

  /* D5(PB6), VSYNC(PB7) */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /* PCLK(PA6) HSYNC(PA4)*/
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_6;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  /* DCMI configuration *******************************************************/ 
  DCMI_InitStructure.DCMI_CaptureMode = DCMI_CaptureMode_Continuous;
  DCMI_InitStructure.DCMI_SynchroMode = DCMI_SynchroMode_Hardware;
  DCMI_InitStructure.DCMI_PCKPolarity = DCMI_PCKPolarity_Falling;
  DCMI_InitStructure.DCMI_VSPolarity = DCMI_VSPolarity_High;
  DCMI_InitStructure.DCMI_HSPolarity = DCMI_HSPolarity_High;
  DCMI_InitStructure.DCMI_CaptureRate = DCMI_CaptureRate_All_Frame;
  DCMI_InitStructure.DCMI_ExtendedDataMode = DCMI_ExtendedDataMode_8b;
  
  DCMI_Init(&DCMI_InitStructure);

  /* Configures the DMA2 to transfer Data from DCMI to the LCD ****************/
  /* Enable DMA2 clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);  
  
  /* DMA2 Stream1 Configuration */  
  DMA_DeInit(DMA2_Stream1);

  DMA_InitStructure.DMA_Channel = DMA_Channel_1;  
  DMA_InitStructure.DMA_PeripheralBaseAddr = DCMI_DR_ADDRESS;	
  DMA_InitStructure.DMA_Memory0BaseAddr = FSMC_LCD_ADDRESS;
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
  DMA_InitStructure.DMA_BufferSize = 1;
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;         
  DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
  DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
  DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
     
  DMA_Init(DMA2_Stream1, &DMA_InitStructure);
}

/**
  * @brief  Configures all needed resources (I2C, DCMI and DMA) to interface with
  *         the OV9655 camera module
  * @param  None
  * @retval 0x00 Camera module configured correctly 
  *         0xFF Camera module configuration failed
  */
uint8_t DCMI_OV9655Config(void)
{
  /* I2C1 will be used for OV9655 camera configuration */
  I2C1_Config();

  /* Reset and check the presence of the OV9655 camera module */
  if (DCMI_SingleRandomWrite(OV9655_DEVICE_WRITE_ADDRESS,0x12, 0x80))
  {
     return (0xFF);
  }

  /* OV9655 Camera size setup */    
  DCMI_OV9655_QVGASizeSetup();

  /* Set the RGB565 mode */
  DCMI_SingleRandomWrite(OV9655_DEVICE_WRITE_ADDRESS, OV9655_COM7, 0x63);
  DCMI_SingleRandomWrite(OV9655_DEVICE_WRITE_ADDRESS, OV9655_COM15, 0x10);

  /* Invert the HRef signal*/
  DCMI_SingleRandomWrite(OV9655_DEVICE_WRITE_ADDRESS, OV9655_COM10, 0x08);

  /* Configure the DCMI to interface with the OV9655 camera module */
  DCMI_Config();
  
  return (0x00);
}

/**
* @brief  Create the application events.
* @param  None
* @retval None
*/
static  void  App_EventCreate (void)
{
  
}

/**
* @brief  Create the application tasks.
* @param  None
* @retval None
*/
static  void  App_TaskCreate (void)
{
  CPU_INT08U  os_err;

  os_err = OSTaskCreateExt((void (*)(void *)) App_TaskKbd,
                           (void          * ) 0,
                           (OS_STK        * )&App_TaskKbdStk[APP_TASK_KBD_STK_SIZE - 1],
                           (INT8U           ) APP_TASK_KBD_PRIO,
                           (INT16U          ) APP_TASK_KBD_PRIO,
                           (OS_STK        * )&App_TaskKbdStk[0],
                           (INT32U          ) APP_TASK_KBD_STK_SIZE,
                           (void          * ) 0,
                           (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
  
#if (OS_TASK_NAME_SIZE >= 9)
  OSTaskNameSet(APP_TASK_KBD_PRIO, "Keyboard", &os_err);
#endif
  
  os_err = OSTaskCreateExt((void (*)(void *)) App_TouchTask,
                           (void          * ) 0,
                           (OS_STK        * )&App_TouchTaskStk[APP_TASK_KBD_STK_SIZE - 1],
                           (INT8U           ) APP_TASK_TOUCH_PRIO,
                           (INT16U          ) APP_TASK_TOUCH_PRIO,
                           (OS_STK        * )&App_TouchTaskStk[0],
                           (INT32U          ) APP_TASK_KBD_STK_SIZE,
                           (void          * ) 0,
                           (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
  
#if (OS_TASK_NAME_SIZE >= 9)
  OSTaskNameSet(APP_TASK_TOUCH_PRIO, "Touch", &os_err);
#endif
}

/**
* @brief  Monitor the state of the push buttons and passes messages to AppTaskUserIF()
* @param  p_arg  Argument passed to 'App_TaskKbd()' by 'OSTaskCreate()'.
* @retval None
*/
static  void  App_TaskKbd (void *p_arg)
{
  CPU_BOOLEAN  b1_prev;
  CPU_BOOLEAN  b1;
  
  
  (void)p_arg;
  
  b1_prev = DEF_TRUE;
  
  while (DEF_TRUE)
  {
    b1 = STM_EVAL_PBGetState(BUTTON_USER);
    
    if ((b1 == DEF_FALSE) && (b1_prev == DEF_TRUE)) {
      led_dly_cout += LED_DLYCOUNT_STEP;
      if (led_dly_cout >= LED_DLYCOUNT_MAX) {
        led_dly_cout = LED_DLYCOUNT_MIN;
      }
      b1_prev = b1;
    }
    
    if ((b1 == DEF_TRUE) && (b1_prev==DEF_FALSE)) {
      b1_prev = DEF_TRUE;
    }
    OSTimeDlyHMSM(0, 0, 0, 20);
  }
}

/**
  * @brief  Copy decompressed data to display buffer.
  * @param  Row: Output row buffer
  * @param  DataLength: Row width in output buffer
  * @retval None
  */
static uint8_t Jpeg_CallbackFunction(uint8_t* Row, uint32_t DataLength)
{
	uint32_t i = 0;
  RGB_matrix =  (RGB_typedef*)Row;
  uint16_t  RGB16Buffer[IMAGE_WIDTH];
  
  for(i = 0; i < IMAGE_WIDTH; i++)
  {
    RGB16Buffer[i]  = (uint16_t)
      (
       ((RGB_matrix[i].B & 0x00F8) >> 3)|
       ((RGB_matrix[i].G & 0x00FC) << 3)|
       ((RGB_matrix[i].R & 0x00F8) << 8)
             );
    LCD_SetTextColor(RGB16Buffer[i]);
		//LCD_DrawLine(i, IMAGE_HEIGHT - line_counter - 1, 1, LCD_DIR_HORIZONTAL);
		LCD_DrawLine(i, line_counter, 1, LCD_DIR_HORIZONTAL);

  }
  
  line_counter++;
  return 0;
}

#if (OS_APP_HOOKS_EN > 0)
/**
* @brief  This function is called when a task is created.
* @param  ptcb  is a pointer to the task control block of the task being created.
* @retval None
*/
void  App_TaskCreateHook (OS_TCB *ptcb)
{
  
}

/**
* @brief  This function is called when a task is deleted.
* @param  ptcb  is a pointer to the task control block of the task being deleted.
* @retval None
*/
void  App_TaskDelHook (OS_TCB *ptcb)
{
  (void)ptcb;
}

/**
* @brief  This function is called by OSTaskIdleHook(), which is called by the
*         idle task.  This hook has been added to allow you to do such things
*         as STOP the CPU to conserve power.
* @param  None
* @retval None
*/
#if OS_VERSION >= 251
void  App_TaskIdleHook (void)
{
}
#endif

/**
* @brief  This function is called by OSTaskStatHook(), which is called every
*         second by uC/OS-II's statistics task.  This allows your application
*         to add functionality to the statistics task.
* @param  None
* @retval None
*/
void  App_TaskStatHook (void)
{
}

/**
* @brief  This function is called when a task switch is performed.  This
*         allows you to perform other operations during a context switch.
* @param  None
* @retval None
*/
#if OS_TASK_SW_HOOK_EN > 0
void  App_TaskSwHook (void)
{
  
}
#endif

/**
* @brief  This function is called by OSTCBInitHook(), which is called by
*         OS_TCBInit() after setting up most of the TCB.
* @param  ptcb is a pointer to the TCB of the task being created.
* @retval None
*/
#if OS_VERSION >= 204
void  App_TCBInitHook (OS_TCB *ptcb)
{
  (void)ptcb;
}
#endif

/**
* @brief  This function is called every tick.
* @param  None
* @retval None
*/
#if OS_TIME_TICK_HOOK_EN > 0
void  App_TimeTickHook (void)
{
  waveRecordTickInc();
}
#endif
#endif

/**
* @brief  This function is called if a task accidentally returns.  In other words,
*         a task should either be an infinite loop or delete itself when done.
* @param  ptcb  is a pointer to the task control block of the task that is returning.
* @retval None
*/
#if OS_VERSION >= 289
void  App_TaskReturnHook (OS_TCB  *ptcb)
{
  (void)ptcb;
}
#endif

/******************** COPYRIGHT 2012 Embest Tech. Co., Ltd.*****END OF FILE****/
