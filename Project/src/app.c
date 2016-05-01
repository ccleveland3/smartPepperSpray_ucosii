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

#define PHONE_LEN           11
#define PIN_LEN             4

#define BACKSPACE           'B'
#define ENTER               'E'

#define STOP  1
#define START 0

#define STOP_UPDATES FALSE
#define SEND_UPDATES TRUE

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static  OS_STK         App_TaskStartStk[APP_TASK_START_STK_SIZE];
static  OS_STK         App_ModemTaskStk[APP_TASK_MODEM_STK_SIZE];
static  OS_EVENT      *SDMutex      = NULL;
        OS_EVENT      *emergencySem = NULL;
static  OS_EVENT      *modemSem     = NULL;
        OS_EVENT      *uartRecvSem  = NULL;
static  OS_TMR        *timerPtr     = NULL;

static  volatile uint8_t emergencyState = STOP_UPDATES;

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
uint8_t modemBuffer1[512];
uint8_t modemBuffer2[512];
RGB_typedef *RGB_matrix;
uint32_t line_counter = 0;
char phoneNum[PHONE_LEN + 1] = "01234567890";
char pinNum[PIN_LEN+1] = "0123";
TS_STATE *TP_State = NULL;
volatile uint8_t appFlags = 0;


//static TS_STATE ts_State;

/* Private function prototypes -----------------------------------------------*/
static  void  App_TaskCreate       (void);
static  void  App_EventCreate      (void);
static  void  App_TaskStart        (void    *p_arg);
static  void  App_ModemTask        (void *p_arg);
static void LCD_Display(void);
static void KeyPad(void);
static uint8_t Touch(uint8_t appFlagMask);
static uint8_t getKey(char *key);
uint8_t DCMI_OV9655Config(void);
void EXTILine0_Config(void);
void DCMI_Config(void);
void I2C1_Config(void);
static void Camera(void);
static void CameraCapture(void);
static uint8_t Jpeg_CallbackFunction(uint8_t* Row, uint32_t DataLength);
void impactTimerCallbackFunction(void *ptmr, void *parg);
static uint8_t getPIN(char *pinBuff, uint8_t appFlagMask);

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

uint8_t validConfig(char *config)
{
  int i;

  for(i = 0; i < (PHONE_LEN + 1/*Space*/ + PIN_LEN); i++)
  {
    // All values should be a number except the space between the phone number
    // and the PIN number.
    if(i != PHONE_LEN)
    {
      if(i != PHONE_LEN && config[i] < '0' && config[i] > '9')
        return FALSE;
    }
    else if (config[i] != ' ')
    {
      return FALSE;
    }
  }
  return TRUE;
}

uint8_t DisplayCheckSettings()
{
  LCD_Clear(LCD_COLOR_WHITE);
  LCD_SetFont(&Font16x24);
  LCD_SetTextColor(LCD_COLOR_BLACK);
  LCD_SetBackColor(LCD_COLOR_WHITE);

  sprintf((char *)buff, "   Configuration:");
  LCD_DisplayStringLine(LCD_LINE_0, buff);

  sprintf((char *)buff, "Phone number:");
  LCD_DisplayStringLine(LCD_LINE_1, buff);
  sprintf((char *)buff, "  +%s", phoneNum);
  LCD_DisplayStringLine(LCD_LINE_2, buff);

  sprintf((char *)buff, "Stop PIN number:");
  LCD_DisplayStringLine(LCD_LINE_3, buff);
  sprintf((char *)buff, "  %s", pinNum);
  LCD_DisplayStringLine(LCD_LINE_4, buff);

  LCD_SetBackColor(LCD_COLOR_RED);
  sprintf((char *)buff, "             Modify");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  LCD_SetBackColor(LCD_COLOR_WHITE);
  sprintf((char *)buff, "             ");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  LCD_SetBackColor(LCD_COLOR_GREEN);
  sprintf((char *)buff, " Accept");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  LCD_SetBackColor(LCD_COLOR_WHITE);
  sprintf((char *)buff, " ");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  Touch(0x00);

  if(TP_State->X > 2000)
  {
    LCD_SetBackColor(0x06E0);
    sprintf((char *)buff, " Accept");
    LCD_DisplayStringLine(LCD_LINE_6, buff);

    LCD_SetBackColor(LCD_COLOR_WHITE);
    sprintf((char *)buff, " ");
    LCD_DisplayStringLine(LCD_LINE_6, buff);

    OSTimeDlyHMSM(0, 0, 1, 0);

    return TRUE;
  }

  LCD_SetBackColor(0x7800);
  sprintf((char *)buff, "             Modify");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  LCD_SetBackColor(LCD_COLOR_WHITE);
  sprintf((char *)buff, "             ");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  LCD_SetBackColor(LCD_COLOR_GREEN);
  sprintf((char *)buff, " Accept");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  LCD_SetBackColor(LCD_COLOR_WHITE);
  sprintf((char *)buff, " ");
  LCD_DisplayStringLine(LCD_LINE_6, buff);

  OSTimeDlyHMSM(0, 0, 0, 100);

  return FALSE;
}

void LCD_DisplayStringOffset(uint16_t line, uint16_t column, uint8_t *ptr)
{
  int i;
  sFONT *font = LCD_GetFont();
  column *= font->Width;

  for(i = 0; ptr[i] != NULL; i++, column += font->Width)
  {
    LCD_DisplayChar(line, column, ptr[i]);
  }
}

void DisplayGetSettings()
{
  char key;
  uint8_t index;

  KeyPad();


  LCD_SetFont(&Font16x24);

  //Get the phone number
  sprintf((char *)buff, "Emergency");
  LCD_DisplayStringOffset(LCD_LINE_0, 10, buff);
  sprintf((char *)buff, "Contact");
  LCD_DisplayStringOffset(LCD_LINE_1, 10, buff);
  sprintf((char *)buff, "Phone");
  LCD_DisplayStringOffset(LCD_LINE_2, 10, buff);
  sprintf((char *)buff, "Number");
  LCD_DisplayStringOffset(LCD_LINE_3, 10, buff);

  sprintf((char *)buff, "Example");
  LCD_DisplayStringOffset(LCD_LINE_8, 10, buff);

  LCD_SetFont(&Font8x12);
  sprintf((char *)buff, "15306809928");
  LCD_DisplayStringOffset(LCD_LINE_18, 20, buff);

  LCD_SetBackColor(LCD_COLOR_WHITE);
  sprintf((char *)phoneNum, "           ");
  LCD_DisplayStringOffset(LCD_LINE_10, 20, (uint8_t *)phoneNum);

  index = 0;
  while(1)
  {
    Touch(0x00);
    if(getKey(&key))
    {
      if(key == BACKSPACE)
      {
        if(index != 0)
        {
          --index;
          phoneNum[index] = ' ';
        }
      }
      else if(key == ENTER)
      {
        if(index == PHONE_LEN)
          break;
      }
      else if(index < PHONE_LEN && key != BACKSPACE)
      {
        phoneNum[index] = key;
        index++;
      }
      LCD_DisplayStringOffset(LCD_LINE_10, 20, (uint8_t *)phoneNum);
    }
  }

  // Get PIN number
  KeyPad();
  LCD_SetFont(&Font16x24);

  sprintf((char *)buff, "Enter 4");
  LCD_DisplayStringOffset(LCD_LINE_0, 10, buff);
  sprintf((char *)buff, "digit stop");
  LCD_DisplayStringOffset(LCD_LINE_1, 10, buff);
  sprintf((char *)buff, "PIN");
  LCD_DisplayStringOffset(LCD_LINE_2, 10, buff);

  getPIN(pinNum, 0x00);
}

static uint8_t getPIN(char *pinBuff, uint8_t appFlagMask)
{
  uint8_t index;
  char key;

  LCD_SetFont(&Font16x24);
  LCD_SetBackColor(LCD_COLOR_WHITE);
  sprintf((char *)pinBuff, "    ");
  LCD_DisplayStringOffset(LCD_LINE_6, 10, (uint8_t *)pinBuff);

  index = 0;
  while(1)
  {
    if(Touch(appFlagMask))
    {
      if(getKey(&key))
      {
        if(key == BACKSPACE)
        {
          if(index != 0)
          {
            --index;
            pinBuff[index] = ' ';
          }
        }
        else if(key == ENTER)
        {
          if(index == PIN_LEN)
            return TRUE;
        }
        else if(index < PIN_LEN && key != BACKSPACE)
        {
          pinBuff[index] = key;
          index++;
        }
        LCD_DisplayStringOffset(LCD_LINE_6, 10, (uint8_t *)pinBuff);
      }
    }
    else
    {
      return FALSE;
    }
  }
}

static void waitForCorrectPIN()
{
  char pinBuff[PIN_LEN + 1];

  KeyPad();
  LCD_SetFont(&Font16x24);

  sprintf((char *)buff, "Enter 4");
  LCD_DisplayStringOffset(LCD_LINE_0, 10, buff);
  sprintf((char *)buff, "digit stop");
  LCD_DisplayStringOffset(LCD_LINE_1, 10, buff);
  sprintf((char *)buff, "PIN to");
  LCD_DisplayStringOffset(LCD_LINE_2, 10, buff);
  sprintf((char *)buff, "cancel GPS");
  LCD_DisplayStringOffset(LCD_LINE_3, 10, buff);
  sprintf((char *)buff, "updates");
  LCD_DisplayStringOffset(LCD_LINE_4, 10, buff);

  do
  {
    getPIN(pinBuff, 0x00);
  } while(strcmp(pinNum, pinBuff) != 0);

  LCD_SetTextColor(LCD_COLOR_BLACK);
  LCD_SetBackColor(LCD_COLOR_GREEN);
  sprintf((char *)buff, "SUCCESS");
  LCD_DisplayStringOffset(LCD_LINE_9, 10, buff);
}

void checkAndSetSettings()
{
  INT8U perr;
  uint32_t bytesRead, bytesWritten = 0;

  OSMutexPend(SDMutex, 0, &perr);

  /* mount the filesystem */
  if (perr != OS_ERR_NONE || f_mount(0, &filesystem) != FR_OK)
  {
    if(perr != OS_ERR_NONE)
    {
      LCD_DisplayStringLine(LCD_LINE_0, (uint8_t *)"SD Mutex error");
    }
    else
    {
      LCD_DisplayStringLine(LCD_LINE_0, (uint8_t *)"Could not mount thefilesystem");
      LCD_DisplayStringLine(LCD_LINE_1, (uint8_t *)"filesystem");
    }
    while(1);
  }
  else
  {
    // Wait for filesystem to be ready or for user to insert microSD card
    while((ret = f_open(&file, "conf.txt", FA_READ | FA_WRITE | FA_OPEN_ALWAYS)) != FR_OK)
    {
      if(SD_Detect() == SD_NOT_PRESENT)
      {
        LCD_DisplayStringLine(LCD_LINE_0, "Please Insert");
        LCD_DisplayStringLine(LCD_LINE_1, "MicroSD Card");
      }
      else
      {
        LCD_DisplayStringLine(LCD_LINE_0, "Could not mount the");
        LCD_DisplayStringLine(LCD_LINE_1, "filesystem error");
        sprintf((char *)buff, "#%d", ret);
        LCD_DisplayStringLine(LCD_LINE_2, buff);
      }
      OSTimeDlyHMSM(0, 0, 0, 250);
    }

    // Check if file is empty and read if not
    if(f_read(&file, _aucLine, 512, &bytesRead) != FR_OK)
    {
      sprintf((char *)buff, "Could not read the file error #%d", ret);
      LCD_DisplayStringLine(LCD_LINE_0, buff);
      while(1);
    }
    else
    {
      if((bytesRead == PHONE_LEN + 1 + PIN_LEN) && validConfig((char *)_aucLine))
      {
        memcpy(phoneNum, _aucLine, PHONE_LEN);
        phoneNum[PHONE_LEN] = (char)NULL;

        memcpy(pinNum, _aucLine + PHONE_LEN + 1, PIN_LEN);
        pinNum[PIN_LEN] = (char)NULL;

        // Check with user if configuration is okay
        if(DisplayCheckSettings())
        {
          ret = f_close(&file);
          f_mount(0, NULL);
          goto end;
        }
      }

      // Ask user for settings
      do
      {
        DisplayGetSettings();
      } while(!DisplayCheckSettings());

      //verify
      sprintf((char *)_aucLine, "%s %s", phoneNum, pinNum);
      if(!validConfig((char *)_aucLine))
      {
        while(1);
      }
      //write file
      if(f_lseek(&file, 0) != FR_OK)
      {
        while(1);
      }
      if((f_write(&file, _aucLine, (PHONE_LEN + 1 + PIN_LEN), &bytesWritten) != FR_OK) || bytesWritten != (PHONE_LEN + 1 + PIN_LEN))
      {
        while(1);
      }

      if(f_close(&file) != FR_OK)
      {
        while(1);
      }

      f_mount(0, NULL);
    }
  }

end:
  if(OSMutexPost(SDMutex) != OS_ERR_NONE)
  {
    while(1);
  }
}

static uint8_t cancelImpact()
{
  INT8U perr;
  char pinBuff[PIN_LEN + 1];

  KeyPad();
  LCD_SetFont(&Font16x24);

  sprintf((char *)buff, "IMPACT");
  LCD_DisplayStringOffset(LCD_LINE_0, 10, buff);
  sprintf((char *)buff, "DETECTED");
  LCD_DisplayStringOffset(LCD_LINE_1, 10, buff);
  sprintf((char *)buff, "Enter 4");
  LCD_DisplayStringOffset(LCD_LINE_2, 10, buff);
  sprintf((char *)buff, "digit stop");
  LCD_DisplayStringOffset(LCD_LINE_3, 10, buff);
  sprintf((char *)buff, "PIN to");
  LCD_DisplayStringOffset(LCD_LINE_4, 10, buff);
  sprintf((char *)buff, "ignore");
  LCD_DisplayStringOffset(LCD_LINE_5, 10, buff);

  //LCD_SetColors(LCD_COLOR_BLACK, LCD_COLOR_WHITE);
  //LCD_DisplayStringOffset(LCD_LINE_9, 18, "10");

  //Start timer
  if(!OSTmrStart(timerPtr, &perr)) {while(1);}
  if(perr != OS_ERR_NONE) {while(1);}
  //try to get pin while button not pressed and not timed out.
  do
  {
    // getPIN will return FALSE if it was not able to get a complete pin due to
    // the emergency button being pressed or because of a timeout.
    if(!getPIN(pinBuff, EMER_BUTTON | APP_TIMEOUT))
    {
      // If the timeout has not happened, stop the timer
      if((appFlags & APP_TIMEOUT) != 0)
      {
        if(!OSTmrStop(timerPtr, OS_TMR_OPT_CALLBACK_ARG, (void *)STOP, &perr)) {while(1);}
        if(perr != OS_ERR_NONE) {while(1);}
      }
      return FALSE;
    }
  } while(strcmp(pinNum, pinBuff) != 0);

  LCD_SetTextColor(LCD_COLOR_BLACK);
  LCD_SetBackColor(LCD_COLOR_GREEN);
  sprintf((char *)buff, "SUCCESS");
  LCD_DisplayStringOffset(LCD_LINE_9, 10, buff);

  if(!OSTmrStop(timerPtr, OS_TMR_OPT_CALLBACK_ARG, (void *)STOP, &perr)) {while(1);}
  if(perr != OS_ERR_NONE) {while(1);}

  return TRUE;
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
  uint8_t perr;

  /* Init */
  BSP_Init();
  OS_CPU_SysTickInit();
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

  accelerometerCompassInit();

  //  WaveRecorderUpdate();

  checkAndSetSettings();

  Camera();

  while(1)
  {

    // Turn the camera display on
    LCD_SetDisplayWindow(0, 0, 320, 240);
    LCD_WriteRAM_Prepare();
    DCMI_CaptureCmd(ENABLE);

    appFlags = 0;
    OSSemSet( emergencySem, 0, &perr);
    OSSemPend(emergencySem, 0, &perr);

    CameraCapture();
    jpeg_test();

    // In the impact case the user will have the oppertunity to cancel the
    // emergency algorithm and return to the initial state.  Pressing the
    // emergency button always results in entering the emergency algorithm
    if((appFlags & EMER_BUTTON) || !cancelImpact())
    {
      emergencyState = SEND_UPDATES;
      //TODO: Tell the modem to send data + text + GPS updats
      if(OSSemPost(modemSem) != OS_ERR_NONE) {while(1);}

      // Display KeyPad to enter stop PIN
      waitForCorrectPIN();
    }
    emergencyState = STOP_UPDATES;
  }
}

static  void  App_ModemTask (void *p_arg)
{
  (void)p_arg;
  INT8U perr;


  //LCD_DisplayStringLine(4, (uint8_t *)"Modem Init");
  if(!modemInit())
  {
  //  LCD_DisplayStringLine(4, (uint8_t *)"Modem Init Failed!");
    while(1);
  }

  while(1)
  {
    // Wait for main task to post semaphore which lets this modem task know it
    // is time to send the photo+gps coordinates to exosite and a send text
    // message with links and coordinates, timeout 5 seconds
    OSSemPend(modemSem, 5000, &perr);

    if(perr == OS_ERR_TIMEOUT)
    {
      // Try to update the gps coordinates every timeout
      gpsReceive();
      if(emergencyState == SEND_UPDATES)
      {
        // Send gps data to exosite
        gpsUpdate();
      }
    }
    else if(perr == OS_ERR_NONE)
    {
      //pictureSend("image.jpg", modemBuffer1, modemBuffer2);
      smsSend();
    }
  }
}



static void Camera()
{

  /* OV9655 Camera Module configuration */
  if (DCMI_OV9655Config() == 0x00)
  {
    //LCD_DisplayStringLine(LINE(2), "                ");
    //LCD_SetDisplayWindow(0, 0, 320, 240);
    //LCD_WriteRAM_Prepare();

    /* Start Image capture and Display on the LCD *****************************/
    /* Enable DMA transfer */
    DMA_Cmd(DMA2_Stream1, ENABLE);

    /* Enable DCMI interface */
    DCMI_Cmd(ENABLE);

    /* Start Image capture */
    //DCMI_CaptureCmd(ENABLE);

    /*init the picture count*/
    init_picture_count();

  } else {
    LCD_SetTextColor(LCD_COLOR_RED);

    LCD_DisplayStringLine(LINE(2), "Camera Init.. fails");
    LCD_DisplayStringLine(LINE(4), "Check the Camera HW ");
    LCD_DisplayStringLine(LINE(5), "  and try again ");

    /* Go to infinite loop */
    while (1);
  }
}

static void CameraCapture()
{
  DCMI_CaptureCmd(DISABLE);
  capture_Flag = DISABLE;
  Capture_Image_TO_Bmp();
  LCD_SetDisplayWindow(0, 0, 320, 240);
  LCD_WriteRAM_Prepare();
  capture_Flag = ENABLE;
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

// Waits for a touch, but returns if the appFlags ANDed with the appFlagMask is
// not 0.  Pass in a appFlagMask value of 0x00 to not return for any reason
// other than a touch detected.
static uint8_t Touch(uint8_t appFlagMask)
{
  //int tpx_sum = 0,tpy_sum = 0;
  //uint8_t  status = 0;
  //uint8_t i;
  char buffer[50];
  //TS_STATE *state = NULL;

  OSTimeDlyHMSM(0, 0, 0, 50);

  // If blocking, inifite while till a touch is detected
  // If nonblocking, inifinit while until any flag is set or a touch is detected
  while((appFlags & appFlagMask) == 0)
  {
    TP_State = IOE_TS_GetState();
    if(TP_State->TouchDetected)
    {
      sprintf(buffer,"                  ");
      LCD_DisplayStringLine(LCD_LINE_9, (uint8_t *)buffer);
      sprintf(buffer,"(x=%d,y=%d)", TP_State->X, TP_State->Y);
      LCD_DisplayStringLine(LCD_LINE_9, (uint8_t *)buffer);
      return TRUE;
    }
    else
    {
      OSTimeDlyHMSM(0, 0, 0, 50);
    }
  }
  return FALSE;

  //LCD_SetTextColor(Blue);
  /*Read AD convert result*/

  //state = IOE_TS_GetState();
  //if(state->TouchDetected)
  //{
  //  for(i = 0; i < 16; i++) {

  //   tpx_sum += IOE_TS_Read_X();
  //   tpy_sum += IOE_TS_Read_Y();
  //   OSTimeDlyHMSM(0, 0, 0, 2);
  //}
  //LCD_Clear(LCD_COLOR_WHITE);
  //tpx_sum >>= 4;
  //tpy_sum >>= 4;
  //}
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
  //LCD_DrawRect(x_pos7,y_pos4,height,width);
  //LCD_DisplayChar(x_pos3,y_pos7,'+');
  //LCD_DrawRect(x_pos7,y_pos2,height,width);
  //LCD_DisplayChar(x_pos5,y_pos7,'-');

  //End call button, Circle
  //LCD_DrawCircle(x_circle_pos0,y_circle_pos,25);
  //LCD_DisplayChar(x_circle_pos1,y_circle_pos,'X');
}

static uint8_t getKey(char *key)
{
  // Array of the key values in row column order
  static char keyArray[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {BACKSPACE, '0', ENTER}};
  uint8_t row = 0;
  uint8_t col = 0;

  *key = (char)NULL;

  // Numbers are from experimentation
  if(TP_State->TouchDetected && TP_State->X > 1900)
  {
    if(TP_State->Y < 1000)
      row = 0;
    else if(TP_State->Y < 1900)
      row = 1;
    else if(TP_State->Y < 2800)
      row = 2;
    else
      row = 3;

    if(TP_State->X > 3200)
      col = 0;
    else if(TP_State->X > 2600)
      col = 1;
    else
      col = 2;

    *key = keyArray[row][col];
    return TRUE;
  }

  return FALSE;
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
  INT8U perr = OS_ERR_NONE;

  // This Mutex protects the SD card since the main task and modem task both use
  // it.
  SDMutex = OSMutexCreate(SD_MUTEX_PRIO, &perr);
  if(SDMutex == NULL) {while(1);}
#if (OS_EVENT_EN) && (OS_EVENT_NAME_EN > 0u)
  OSEventNameSet (SDMutex, "SDMutex", &perr);
  if(perr != OS_ERR_NONE) {while(1);}
#endif

  // The Button Semiphore wakes up the main task when the button is pressed.
  emergencySem =  OSSemCreate(0);
  if(emergencySem == NULL) {while(1);}
#if (OS_EVENT_EN) && (OS_EVENT_NAME_EN > 0u)
  OSEventNameSet (emergencySem, "emergencySem", &perr);
  if(perr != OS_ERR_NONE) {while(1);}
#endif

  modemSem = OSSemCreate (0);
  if(modemSem == NULL) {while(1);}
#if (OS_EVENT_EN) && (OS_EVENT_NAME_EN > 0u)
  OSEventNameSet(modemSem, "modemSem", &perr);
  if(perr != OS_ERR_NONE) {while(1);}
#endif

  uartRecvSem = OSSemCreate (0);
  if(uartRecvSem == NULL) {while(1);}
#if (OS_EVENT_EN) && (OS_EVENT_NAME_EN > 0u)
  OSEventNameSet(uartRecvSem, "uartRecvSem", &perr);
  if(perr != OS_ERR_NONE) {while(1);}
#endif

  timerPtr = OSTmrCreate (10                           /* 10 = 1s  delay */,
                          10                           /* 10 = 1s Repeat period (not periodic) */,
                          OS_TMR_OPT_PERIODIC          /* opt */,
                          impactTimerCallbackFunction  /* Callback function to call at 0 */,
                          START                        /* callback_arg */,
                          "Impact"                     /* pname */,
                          &perr);
  if(timerPtr == NULL) {while(1);}
  if(perr != OS_ERR_NONE) {while(1);}
}

/**
* @brief  Create the application tasks.
* @param  None
* @retval None
*/
static  void  App_TaskCreate (void)
{
  CPU_INT08U  os_err;

  os_err = OSTaskCreateExt((void (*)(void *)) App_ModemTask,
                           (void          * ) 0,
                           (OS_STK        * )&App_ModemTaskStk[APP_TASK_MODEM_STK_SIZE - 1],
                           (INT8U           ) APP_TASK_MODEM_PRIO,
                           (INT16U          ) APP_TASK_MODEM_PRIO,
                           (OS_STK        * )&App_ModemTaskStk[0],
                           (INT32U          ) APP_TASK_MODEM_STK_SIZE,
                           (void          * ) 0,
                           (INT16U          )(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

#if (OS_TASK_NAME_SIZE >= 9)
  OSTaskNameSet(APP_TASK_MODEM_PRIO, "Modem", &os_err);
#endif
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

void impactTimerCallbackFunction(void *ptmr, void *parg)
{
  static int countDown = 10;
  char       countDownStr[3];
  sFONT      *origFont;
  uint16_t origTextColor;
  uint16_t origBackColor;

  if((int)parg != STOP)
  {
    if(countDown != 0)
    {
      // Saving original font and color since we may be interrupting another
      // task that is using the LCD.  It is possible we could screw up one of
      // their pixels or they could screw up one of ours, but the probability
      // and consquence is low enough to not worry about protecting the LCD
      // with a mutex.
      origFont = LCD_GetFont();
      LCD_GetColors(&origTextColor, &origBackColor);

      LCD_SetFont(&Font16x24);

      if(countDown != 1)
      {
        LCD_SetColors(LCD_COLOR_BLACK, LCD_COLOR_WHITE);
        sprintf(countDownStr, "%2d", countDown);
        LCD_DisplayStringOffset(LCD_LINE_9, 18, (uint8_t *)countDownStr);
      }
      else
      {
        LCD_SetColors(LCD_COLOR_BLACK, LCD_COLOR_RED);
        LCD_DisplayStringOffset(LCD_LINE_9, 13, (uint8_t *)"TIMEOUT");
      }

      LCD_SetFont(origFont);
      LCD_SetColors(origTextColor, origBackColor);

      countDown--;
    }
    else
    {
      countDown = 10;
      appFlags |= APP_TIMEOUT;
    }
  }
  else
  {
    countDown = 10;
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
