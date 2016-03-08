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

SD_Error Status = SD_OK;
FATFS filesystem;		/* volume lable */
FRESULT ret;			/* Result code */
FIL file;			/* File object */
DIR dir;			/* Directory object */
FILINFO fno;			/* File information object */
UINT bw, br;
uint8_t buff[128];
uint8_t touched = 0;


static TS_STATE ts_State;

/* Private function prototypes -----------------------------------------------*/
static  void  App_TaskCreate       (void);
static  void  App_EventCreate      (void);
static  void  App_TaskStart        (void    *p_arg);
static  void  App_TaskKbd          (void    *p_arg);
static  void  App_TouchTask        (void *p_arg);
static  void  fault_err            (FRESULT rc);
static void LCD_Display();
static void KeyPad();
static void Touch();
uint8_t DCMI_OV9655Config(void);
void DCMI_Config(void);
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

/**
* @brief  The startup task.  The uC/OS-II ticker should only be initialize
*         once multitasking starts.
* @param  p_arg  Argument passed to 'App_TaskStart()' by 'OSTaskCreate()'.
* @retval None
*/
static  void  App_TaskStart (void *p_arg)
{
  (void)p_arg;
  float pfData[3] = {0};

  /* Initialize BSP functions. */
  BSP_Init();
  /* Initialize the SysTick. */
  OS_CPU_SysTickInit();
  /* Initialize external interrupt callbacks */
  itInit();

  sineWave_init();

#if (OS_TASK_STAT_EN > 0)
  /* Determine CPU capacity. */
  OSStatInit();
#endif
  /* Create application events. */
  App_EventCreate();
  /* Create application tasks. */
  App_TaskCreate();

  /* mount the filesystem */
  if (f_mount(0, &filesystem) != FR_OK) {
    //printf("could not open filesystem \n\r");
  }
  OSTimeDlyHMSM(0, 0, 0, 10);

  WaveRecorderUpdate();

  accelerometerCompassInit();
  LSM303DLHC_CompassReadAcc(pfData);
  LSM303DLHC_CompassReadMag(pfData);

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
  
  LCD_Display();
  KeyPad();
  
  
  while(1)
  {
    Touch();
    OSTimeDlyHMSM(0, 0, 0, 20);
  }
  
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
  LCD_Clear(LCD_COLOR_MAGENTA);
  
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
    LCD_DisplayStringLine(0,buffer);
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
