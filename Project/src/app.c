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
  /* Initialize BSP functions. */
  BSP_Init();
  /* Initialize the SysTick. */
  OS_CPU_SysTickInit();
  /* Initialize external interrupt callbacks */
  itInit();
  
#if (OS_TASK_STAT_EN > 0)
  /* Determine CPU capacity. */
  OSStatInit();
#endif
  /* Create application events. */
  App_EventCreate();
  /* Create application tasks. */
  App_TaskCreate();
  
  //STM32f4_Discovery_Debug_Init();
  
  //LCD_Display();
  
  /* mount the filesystem */
  if (f_mount(0, &filesystem) != FR_OK) {
    //printf("could not open filesystem \n\r");
  }
  OSTimeDlyHMSM(0, 0, 0, 10);
  
  //printf("Open a test file (message.txt) \n\r");
  ret = f_open(&file, "MESSAGE.TXT", FA_READ);
  if (ret) {
    //printf("not exist the test file (message.txt)\n\r");
  } else {
    //printf("Type the file content\n\r");
    for (;;) {
      ret = f_read(&file, buff, sizeof(buff), &br);	/* Read a chunk of file */
      if (ret || !br) {
        break;			/* Error or end of file */
      }
      buff[br] = 0;
      //printf("%s",buff);
      //printf("\n\r");
    }
    if (ret) {
      //printf("Read the file error\n\r");
      fault_err(ret);
    }
    
    //printf("Close the file\n\r");
    ret = f_close(&file);
    if (ret) {
      // printf("Close the file error\n\r");
    }
  }
  
  /*  hello.txt write test*/
  OSTimeDlyHMSM(0, 0, 0, 50);
  //printf("Create a new file (hello.txt)\n\r");
  ret = f_open(&file, "HELLO.TXT", FA_WRITE | FA_CREATE_ALWAYS);
  if (ret) {
    //printf("Create a new file error\n\r");
    fault_err(ret);
  } else {
    //printf("Write a text data. (hello.txt)\n\r");
    ret = f_write(&file, "Hello world!", 14, &bw);
    if (ret) {
      //printf("Write a text data to file error\n\r");
    } else {
      //printf("%u bytes written\n\r", bw);
    }
    OSTimeDlyHMSM(0, 0, 0, 50);
    //printf("Close the file\n\r");
    ret = f_close(&file);
    if (ret) {
      //printf("Close the hello.txt file error\n\r");
    }
  }
  
  /*  hello.txt read test*/
  OSTimeDlyHMSM(0, 0, 0, 50);
  //printf("read the file (hello.txt)\n\r");
  ret = f_open(&file, "HELLO.TXT", FA_READ);
  if (ret) {
    //printf("open hello.txt file error\n\r");
  } else {
    //printf("Type the file content(hello.txt)\n\r");
    for (;;) {
      ret = f_read(&file, buff, sizeof(buff), &br);	/* Read a chunk of file */
      if (ret || !br) {
        break;			/* Error or end of file */
      }
      buff[br] = 0;
      //printf("%s",buff);
      //printf("\n\r");
    }
    if (ret) {
      //printf("Read file (hello.txt) error\n\r");
      fault_err(ret);
    }
    
    //printf("Close the file (hello.txt)\n\r");
    ret = f_close(&file);
    if (ret) {
      //printf("Close the file (hello.txt) error\n\r");
    }
  }
  
  /*  directory display test */
  //  OSTimeDlyHMSM(0, 0, 0, 50);
  //  printf("Open root directory\n\r");
  //  ret = f_opendir(&dir, "");
  //  if (ret) {
  //    printf("Open root directory error\n\r");
  //  } else {
  //    printf("Directory listing...\n\r");
  //    for (;;) {
  //      ret = f_readdir(&dir, &fno);		/* Read a directory item */
  //      if (ret || !fno.fname[0]) {
  //        break;	/* Error or end of dir */
  //      }
  //      if (fno.fattrib & AM_DIR) {
  //        printf("  <dir>  %s\n\r", fno.fname);
  //      } else {
  //        printf("%8lu  %s\n\r", fno.fsize, fno.fname);
  //      }
  //    }
  //    if (ret) {
  //      printf("Read a directory error\n\r");
  //      fault_err(ret);
  //    }
  //  }
  OSTimeDlyHMSM(0, 0, 0, 50);
  //printf("Test completed\n\r");
  
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
/**
* @brief  assert failed function.
* @param  None
* @retval None
*/
void assert_failed(char* file, int line)
{
  
}

/**
* @brief   FatFs err dispose
* @param  None
* @retval None
*/
static void fault_err (FRESULT rc)
{
  const char *str =
    "OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
      "INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
        "INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
          "LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
  FRESULT i;
  
  for (i = (FRESULT)0; i != rc && *str; i++) {
    while (*str++) ;
  }
  //printf("rc=%u FR_%s\n\r", (UINT)rc, str);
  //STM_EVAL_LEDOn(LED6);
  while(1);
}

/******************** COPYRIGHT 2012 Embest Tech. Co., Ltd.*****END OF FILE****/
