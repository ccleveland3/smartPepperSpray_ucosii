/*******************************************************************************
* File:   modem.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To provide functionality to the 4G LTE and GPS modem LE910-SVG
*******************************************************************************/
#include <includes.h>

#define RX_BUFFER_SIZE 255
#define RX_CIRC_INC(x) ((x + 1) % RX_BUFFER_SIZE)

static const char rx_error[] = "ERROR";

static volatile char rx_buffer[RX_BUFFER_SIZE] = {0};
static volatile int rx_in  = 0;
static volatile int rx_out = 0;

char rx_line[RX_BUFFER_SIZE] = {0};
char rx_response[RX_BUFFER_SIZE] = {0};

static void modemSend(const char *str);
static int waitResponse(const char *target, int numChar);
static int modemGetNextLine();
static int parseGPSData();
void messageSend();
void setupGPS();

struct Gps {
  char utc[10];       //"hhmmss.ss"
  char latitude[12];  //"+dd.ddddddd"
  char longitude[13]; //"-ddd.ddddddd"
  char hdop[4];       //"x.x"
  char altidude[7];   //"xxxx.x"
  char fix[2];        //"x"
  char cog[7];        //"ddd.mm"
  char spkm[7];       //"xxxx.x"
  char spkn[7];       //"xxxx.x"
  char date[7];       //"ddmmyy"
  char nsat[3];       //"nn"
};

struct Gps lastGPS = {"hhmmss.ss", "+dd.ddddddd", "-ddd.ddddddd", "x.x", \
  "xxxx.x", "x", "ddd.mm", "xxxx.x", "xxxx.x", "ddmmyy", "nn"};

//Must add latitude,longitude to the end and keep the total characters under 150
const char textMessage[68] = "COME HELP PLEASE! https://www.google.com/maps/dir/Current+Location/";

void modemInit()
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  
  /* Enable GPIO clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  
  /* Enable UART clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  
  /* Connect PA9 to USART1_Tx*/
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
  
  /* Connect PA10 to USART1_Rx*/
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
  
  /* Connect PA11 to USART1_CTS*/
  //GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_USART1);
  
  /* Connect PA12 to USART1_RTS*/
  //GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_USART1);
  
  /* Configure USART Tx, Rx, CTS, and RTS as alternate function  */
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
  //GPIO_Pin_11 | GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  
  
  /* Configure the ON_OFF pin for the modem
  * It is very important that the output type be open drain because a high is
  * 1.8V for the modem.  The other inputs/outputs are level shifted.
  */
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL; //The modem pulls up
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
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
  USART_Init(USART1, &USART_InitStructure);
  
  /* Enable USART */
  USART_Cmd(USART1, ENABLE);
  
  
  
  /* Enable the USART1 gloabal Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  
  /* Enable the Receive Data register not empty interrupt */
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  
  //  /* Turn Modem on */
  GPIO_ResetBits(GPIOA, GPIO_Pin_8);
  OSTimeDlyHMSM(0, 0, 2, 0);
  GPIO_SetBits(GPIOA, GPIO_Pin_8);
  OSTimeDlyHMSM(0, 0, 2, 0);
  GPIO_ResetBits(GPIOA, GPIO_Pin_8);
  
  /* Turn off echo */
  modemSend("XATE0\r\n"); 
  waitResponse("OK",2);
  //    modemSend("AT$GPSSLSR=2,3\r\n");
  // waitResponse("OK",2);
  //  modemSend("AT$GPSP=1");
  //  waitResponse("OK",2);
  //  modemSend("AT$GPSNMUN=3,1,1,1,1,1,1\r\n");
  //  waitResponse("CONNECT",2);
  //  modemSend("+++\r\n");
  //  waitResponse("OK",2);
  //  modemSend("AT$GPSACP\r\n");
  //  waitResponse("OK",2);
  
  
  //messageSend();
  setupGPS();
  
}



void messageSend()
{
  // Update the GPS data if possible, if it fails, then send the last known
  // position
  parseGPSData();
  
  OSTimeDlyHMSM(0, 0, 0, 50);
  modemSend("AT+CMGF=1\r\n");
  waitResponse("OK",2);
  OSTimeDlyHMSM(0, 0, 0, 50);
  modemSend("AT+CMGS=\"+15306809928\"\r\n");
  OSTimeDlyHMSM(0, 0, 0, 50);
  
  modemSend(textMessage);       //67
  modemSend(lastGPS.latitude);  //11
  modemSend(",");               //1
  modemSend(lastGPS.longitude); //12
  modemSend("  This is only a test of Smart Pepper Spray\x1A"); //43, total 134
  
  waitResponse("+CMGS:xx",2);
}


void setupGPS()
{
  modemSend("AT$GPSAT=1");
  waitResponse("OK",2);
  modemSend("AT$GPSP=1");
  waitResponse("OK",2);
  
  //Get first location
  while(!parseGPSData())
  {
    //Free up CPU while we wait for the GPS to get a fix
    OSTimeDlyHMSM(0, 0, 1, 0);
  }
}

static int parseGPSData()
{
  static struct Gps curGps;
  static char rawLatitude[]  = "ddmm.mmmmD";
  static char rawLongitude[] = "dddmm.mmmmmD";
  
  modemSend("AT$GPSACP\r\n");
  
  if(getResponse("$GPSACP:", "OK"))
  {  
    if(sscanf(rx_response + 9, \
      "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\n]", \
        curGps.utc,rawLatitude,rawLongitude,curGps.hdop,curGps.altidude, \
          curGps.fix,curGps.cog,curGps.spkm,curGps.spkn,curGps.date, \
            curGps.nsat) == 11)
    {
      //The longitude and latitude have to be converted to degrees for google
      //maps.  The modem gives us the latitude in the form of ddmm.mmmmD where 
      //dd is degrees between from 00 to 90, mm.mmmm is minutes from 00.0000 to
      //59.9999 and D is N for north or S for south.  Longitude is in the form
      //dddmm.mmmmD where ddd is degrees from 000 to 180, mm.mmmm is minutes
      //from 00.0000 to 59.9999, and D is either E for east or W for west.
      
      curGps.latitude[0]  = (rawLatitude[9]   == 'N') ? '+' : '-';
      curGps.longitude[0] = (rawLongitude[10] == 'E') ? '+' : '-';
      
      rawLatitude[9]   = (char)NULL;
      rawLongitude[10] = (char)NULL;
      
      double minutes = atof(rawLatitude + 2);
      double degrees = minutes / 60;
      sprintf(curGps.latitude + 2, "%0.7lf", degrees);
      
      minutes = atof(rawLongitude + 2);
      degrees = minutes / 60;
      sprintf(curGps.longitude + 2, "%0.7lf", degrees);
      
      curGps.latitude[1] = rawLatitude[0];
      curGps.latitude[2] = rawLatitude[1];
      
      curGps.longitude[1] = rawLongitude[0];
      curGps.longitude[2] = rawLongitude[1];
      curGps.longitude[3] = rawLongitude[2];
      
      memcpy(&lastGPS, &curGps, sizeof(struct Gps));
      
      return 1;
    }
    else
    {
      return 0;
    }   
  }
  
  //getResponse failed
  return 0;
}

static void modemSend(const char *str)
{
  for(int i = 0; str[i] != 0; i++)
  {
    USART_SendData(USART1, (uint8_t) str[i]);
    
    /* Loop until the end of transmission */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
    {}
    //LCD_DisplayStringLine(0,"Sucess");
  }
}

static int getResponse(const char *start, const char *end)
{
  rx_response[0] = (char)NULL;
  
  do {
    modemGetNextLine();
  } while(strcmp(rx_line,start) || strcmp(rx_line,rx_error));
  
  if(strcmp(rx_line,rx_error))
  {
    return 0;
  }
  
  strcpy(rx_response, rx_line);
  
  // Keep copying lines until we find the end, an error, or run out of memory
  while(strcmp(rx_line,end) || strcmp(rx_line,rx_error))
  {
    modemGetNextLine();
    if(strlen(rx_response) + strlen(rx_line) > RX_BUFFER_SIZE)
    {
      //Ran out of memory
      return 0;
    }
    else
    {    
      strcpy(rx_response + strlen(rx_response), rx_line);
    }
  }
  
  if(strcmp(rx_line,rx_error))
  {
    return 0;
  }
  return 1;
}

static int waitResponse(const char *target, int numChar)
{
  do{
    modemGetNextLine();
  } while(strncmp(rx_line,target,numChar));
  return 0;
}

static int modemGetNextLine()
{
  int lineIndex = 0;
  int in, out; //Temp variables for holding globals that the UART interrupt changes
  
  //Disable UART interrupt while we check the globals it changes
  USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
  
  while( lineIndex == 0 || (rx_line[lineIndex-1] != '\n'))
  {
    in  = rx_in;
    out = rx_out;
    
    //Wait for more data if there is none
    if (in == out)
    {
      //Enable interrupt because there is no new data
      USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
      do {
        in  = rx_in;
        out = rx_out;
      } while( in == out);
      
      //Disable UART interrupt while we check the globals it changes
      USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
    }
    rx_line[lineIndex] = rx_buffer[rx_out];
    lineIndex++;
    rx_out = (rx_out+1) % RX_BUFFER_SIZE;
  }  //while
  
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  
  rx_line[lineIndex-1] = 0; //Strip off the newline and terminate the string
  
  return lineIndex;
}

void Modem_USART1_IRQ()
{
  if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
  {
    while((USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) && (RX_CIRC_INC(rx_in) != rx_out))
    {
      rx_buffer[rx_in] = (char)USART_ReceiveData(USART1);
      rx_in = RX_CIRC_INC(rx_in);
    }
  }
}