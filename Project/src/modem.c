/*******************************************************************************
* File:   modem.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To provide functionality to the 4G LTE and GPS modem LE910-SVG
*******************************************************************************/
#include <includes.h>

#define RX_BUFFER_SIZE 255
#define RX_CIRC_INC(x) ((x + 1) % RX_BUFFER_SIZE)

#define OUT_FILE_NAME "base64.txt"

#define RECV_TIMEOUT            0
#define RECV_ERR_NONE           1
#define RECV_ERROR_STR          2
#define RECV_NO_MEMORY          3
#define TRAN_FAIL               4

#define STD_TIMEOUT            500

static const char rx_error[] = "ERROR\r";

static volatile char rx_buffer[RX_BUFFER_SIZE] = {0};
static volatile int rx_in  = 0;
static volatile int rx_out = 0;

char rx_line[RX_BUFFER_SIZE] = {0};
char rx_response[RX_BUFFER_SIZE] = {0};

static void modemSend(const char *str, uint8_t literalPlus);
static int waitResponse(const char *target, int numChar, int timeout_ms);
static int modemGetNextLine(int timeout_ms);
static int parseGPSData(void);
static int getResponse(const char *start, const char *end, int timeout_ms);
static int openExositeSocket(int contentLength, char *buffer);
static int modemSendAndWaitResp(const char *cmd, const char *resp, int numChar, int timeout_ms, int maxRepeat);
void smsSend(void);
void setupGPS(void);
void pictureSend(const char* imageFileName, uint8_t * inBuff, uint8_t * outBuff);

struct Gps {
  char utc[11];       //"hhmmss.sss"
  char googleLatitude[12];  //"+dd.ddddddd"
  char googleLongitude[13]; //"-ddd.ddddddd"
  char exositeLatitude[11]; //"+ddmm.mmmm"
  char exositeLongitude[12]; //-dddmm.mmmm"
  char hdop[4];       //"x.x"
  char altidude[7];   //"xxxx.x"
  char fix[2];        //"x"
  char cog[7];        //"ddd.mm"
  char spkm[7];       //"xxxx.x"
  char spkn[7];       //"xxxx.x"
  char date[7];       //"ddmmyy"
  char nsat[3];       //"nn"
};

static uint8_t gpsValid = FALSE;

const char base64[64] = {\
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',\
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',\
      'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',\
	't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',\
          '8', '9', '+', '/'};

extern char      phoneNum[];
extern OS_EVENT *uartRecvSem;
extern OS_EVENT *SDMutex;
extern FATFS     filesystem;

FIL inFile, outFile;

struct Gps lastGPS = {"hhmmss.ss", "+dd.ddddddd", "-ddd.ddddddd", "+ddmm.mmmm",\
  "-dddmm.mmmm", "x.x", "xxxx.x",  "x", "ddd.mm", "xxxx.x", "xxxx.x", "ddmmyy",\
    "nn"};

//Must add latitude,longitude to the end and keep the total characters under 150
const char googleTextMessage[50]  = "https://www.google.com/maps/dir/Current+Location/";
const char altTextMessage[51]     = "gps data will be posted to exosite when available ";
const char exositeTestMessage[57] = " https://portals.exosite.com/views/1828597911/3170008474";

int modemInit()
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
  //GPIO_ResetBits(GPIOA, GPIO_Pin_8);

  /* Turn off echo */
  if(modemSendAndWaitResp("ATE0\r\n", "OK", 2, STD_TIMEOUT, 5) != RECV_ERR_NONE)
  {
    //  /* Turn Modem on */
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    OSTimeDlyHMSM(0, 0, 2, 0);
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    OSTimeDlyHMSM(0, 0, 2, 0);
   // GPIO_ResetBits(GPIOA, GPIO_Pin_8);

    if(modemSendAndWaitResp("ATE0\r\n", "OK", 2, STD_TIMEOUT, 5) != RECV_ERR_NONE)
    {
      return 0;
    }
  }
  modemSendAndWaitResp("AT+CSQ\r\n", "OK", 2, STD_TIMEOUT, 5);
  modemSendAndWaitResp("AT#SCFG=3,3,300,90,600,50\r\n", "OK", 2, STD_TIMEOUT, 5);
  modemSendAndWaitResp("AT+CGDCONT=3,\"IP\",\"VZWINTERNET\"\r\n", "OK", 2, STD_TIMEOUT, 5);
  modemSendAndWaitResp("AT#SGACT=3,1\r\n", "OK", 2, STD_TIMEOUT*5, 5);
  // modemSendAndWaitResp("AT+CGREG\r\n", "OK", 2, STD_TIMEOUT, 5);

  setupGPS();

  return 1;
}



void smsSend()
{
  // Update the GPS data if possible, if it fails, then send the last known
  // position
  parseGPSData();

  OSTimeDlyHMSM(0, 0, 0, 50);
  modemSendAndWaitResp("AT+CMGF=1\r\n", "OK", 2, STD_TIMEOUT, 5);
  modemSend("AT+CMGS=\"+", FALSE);
  modemSend(phoneNum, FALSE);
  modemSend("\"\r\n", FALSE);
  OSTimeDlyHMSM(0, 0, 0, 250);

  modemSend("Help! ", FALSE);
  if(gpsValid == TRUE)
  {
    modemSend(googleTextMessage, FALSE);       //49
    modemSend(lastGPS.googleLatitude, FALSE);  //11
    modemSend(",", FALSE);                     //1
    modemSend(lastGPS.googleLongitude, FALSE); //12
  }
  else
  {
    modemSend(altTextMessage, FALSE); //50
  }
  modemSend(exositeTestMessage, FALSE);      //56
  modemSend(" ONLY A TEST\x1A", FALSE); //12, total 147

  //waitResponse("+CMGS:xx",2);
  waitResponse("OK",2, 500);
}


void setupGPS()
{
  //Ok if modem responds with ERROR, this means GPS is already powering the GPS
  //antenna
  modemSendAndWaitResp("AT$GPSAT=1\r\n", "OK", 2, STD_TIMEOUT, 5);

  //Ok if modem responds with ERROR, this means GPS is already powered
  modemSendAndWaitResp("AT$GPSP=1\r\n",  "OK", 2, STD_TIMEOUT, 5);

  // Try to get the first location
  parseGPSData();
}

void gpsReceive()
{
  parseGPSData();
}

static int parseGPSData()
{
  static struct Gps curGps;
  static char rawLatitude[]  = "ddmm.mmmmD";
  static char rawLongitude[] = "dddmm.mmmmmD";
  double minutes;
  double degrees;

  modemSend("AT$GPSACP\r\n", FALSE);

  if(getResponse("$GPSACP:", "OK\r", STD_TIMEOUT) == RECV_ERR_NONE)
  {
    if(sscanf(rx_response + 9, \
      "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\r]", \
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

      curGps.googleLatitude[0]   = (rawLatitude[9]   == 'N') ? '+' : '-';
      curGps.googleLongitude[0]  = (rawLongitude[10] == 'E') ? '+' : '-';

      curGps.exositeLatitude[0]  = (rawLatitude[9]   == 'N') ? '+' : '-';
      curGps.exositeLongitude[0] = (rawLongitude[10] == 'E') ? '+' : '-';

      rawLatitude[9]   = (char)NULL;
      rawLongitude[10] = (char)NULL;

      strcpy(curGps.exositeLatitude  + 1, rawLatitude);
      strcpy(curGps.exositeLongitude + 1, rawLongitude);

      minutes = atof(rawLatitude + 2);
      degrees = minutes / 60;
      sprintf(curGps.googleLatitude + 2, "%0.7lf", degrees);

      minutes = atof(rawLongitude + 3);
      degrees = minutes / 60;
      sprintf(curGps.googleLongitude + 3, "%0.7lf", degrees);

      curGps.googleLatitude[1] = rawLatitude[0];
      curGps.googleLatitude[2] = rawLatitude[1];

      curGps.googleLongitude[1] = rawLongitude[0];
      curGps.googleLongitude[2] = rawLongitude[1];
      curGps.googleLongitude[3] = rawLongitude[2];

      memcpy(&lastGPS, &curGps, sizeof(struct Gps));

      gpsValid = TRUE;

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

// inBuff  needs to be 514 bytes
// outBuff needs to be 512 bytes
void pictureSend(const char* imageFileName, uint8_t * inBuff, uint8_t * outBuff)
{
  int contentLength = 0;
  int numPlus = 0;
  INT8U perr;

  // Number of bytes read from the JPEG file during the last f_read call
  uint32_t bytesRead      = 0;

  // Number of bytes left from the JEPG file that need to be encoded
  uint32_t jpegBytesLeft  = 0;

  // Number of bytes written to the base64 file during the last f_write call
  uint32_t bytesWritten   = 0;

  // Number of bytes that have not been written to the base64 file yet
  uint32_t bytesUnwritten = 0;

  // Used to hold 3 bytes at a time from the JPEG file that need to be base64
  // encoded
  uint32_t temp           = 0;
  int inBuffIndex         = 0;
  int totalWritten        = 0;
  int base64FileSize      = 0;
  uint8_t endOfFile       = FALSE;
  uint8_t dataToProcess   = FALSE;
  uint8_t unEncodedByte1  = 0;
  uint8_t unEncodedByte2  = 0;
  uint8_t numUnEncoded    = 0;

  OSMutexPend(SDMutex, 0, &perr);

  if(f_mount(0, &filesystem) == FR_OK)
  {
    if(f_open(&outFile, OUT_FILE_NAME, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
    {
      // Open the JPEG image with read access
      if(f_open(&inFile, imageFileName, FA_READ) == FR_OK)
      {

        // Begin converting the image file to Base64 encoding
        while(!endOfFile)
        {
          if(f_read(&inFile, inBuff, 512, (UINT*)&bytesRead) == FR_OK)
          {
            jpegBytesLeft += bytesRead;
            inBuffIndex    = 0;

            // True even if we reach the end of the file since we need to check
            // for any unencoded bytes.
            dataToProcess  = TRUE;

            // Go to write the end sequence if we are done
            while(dataToProcess)
            {
              // bytesRead is zero when we reach the end of the file.
              if(bytesRead != 0)
              {

                // Encode data until we don't have enough JPEG bytes to make
                // the set of three that we need to make four bytes of base64
                // data or run out of out buffer room and need to flush it.
                while((jpegBytesLeft >= 3) && (bytesUnwritten < 512))
                {
                  // First use up any left over unencoded JPEG bytes left over
                  // from last time we were here.
                  switch (numUnEncoded)
                  {
                  case 2:
                    temp = ((unEncodedByte1 << 16) | \
                      (unEncodedByte2 << 8)  | \
                        (inBuff[inBuffIndex]));
                    inBuffIndex += 1;
                    numUnEncoded = 0;
                    break;
                  case 1:
                    temp = ((unEncodedByte1      << 16) | \
                      (inBuff[inBuffIndex] << 8)  | \
                        (inBuff[inBuffIndex+1]));
                    inBuffIndex += 2;
                    numUnEncoded = 0;
                    break;
                  default:
                    // Most common case.  Either case 1 or 2 can only happen
                    // at most once per f_read.
                    temp = ((inBuff[inBuffIndex]   << 16) | \
                      (inBuff[inBuffIndex+1] << 8)  | \
                        (inBuff[inBuffIndex+2]));
                    inBuffIndex    += 3;
                    break;
                  }

                  // Convert 6 bits of JPEG data at a time to base64
                  outBuff[bytesUnwritten]   = base64[(temp >> 18) & 0x3F];
                  outBuff[bytesUnwritten+1] = base64[(temp >> 12) & 0x3F];
                  outBuff[bytesUnwritten+2] = base64[(temp >> 6)  & 0x3F];
                  outBuff[bytesUnwritten+3] = base64[ temp        & 0x3F];

                  bytesUnwritten += 4;
                  jpegBytesLeft  -= 3;
                }

                // Check to see if we ran out of data and if we need to keep
                // track of any unencoded JPEG bytes.
                if(jpegBytesLeft < 3)
                {
                  dataToProcess = FALSE;
                  numUnEncoded  = jpegBytesLeft;

                  // Need to read more data from the JPEG file, but there may
                  // be up to 2 bytes of unprocessed data left over.
                  switch (jpegBytesLeft)
                  {
                  case 2:
                    unEncodedByte2 = inBuff[inBuffIndex+1];
                  case 1:
                    unEncodedByte1 = inBuff[inBuffIndex];
                  default :
                    break;
                  }
                }
              }
              else //No bytes read, this most likely means end of file.
              {
                endOfFile     = TRUE;
                dataToProcess = FALSE;

                // Write end sequence according to the MIME standard using
                // whatever unencoded JPEG bytes are left, if any at all.
                switch(jpegBytesLeft)
                {
                case 0 :
                  // Nothing to do but flush the out buffer
                  break;
                case 1 :
                  outBuff[bytesUnwritten]   = base64[unEncodedByte1 >> 2 & 0x3F];
                  outBuff[bytesUnwritten+1] = base64[unEncodedByte1 << 4 & 0x3F];
                  outBuff[bytesUnwritten+2] = '=';
                  outBuff[bytesUnwritten+3] = '=';

                  bytesUnwritten += 4;
                  jpegBytesLeft  -= 1;
                  numUnEncoded   -= 1;
                  break;
                case 2 :
                  temp = ((unEncodedByte1 << 8) | (unEncodedByte2));

                  outBuff[bytesUnwritten]   = base64[(temp >> 10) & 0x3F];
                  outBuff[bytesUnwritten+1] = base64[(temp >> 4)  & 0x3F];
                  outBuff[bytesUnwritten+2] = base64[(temp << 2)  & 0x3F];
                  outBuff[bytesUnwritten+3] = '=';

                  bytesUnwritten += 4;
                  jpegBytesLeft  -= 2;
                  numUnEncoded   -= 2;
                  break;
                default :
                  //Should never happen
                  while(1);
                }
              }

              // Write unwritten bytes to the base64 file in chunks of 512
              // except if we reach the end of the JPEG file, then write
              // whatever is left.
              if(bytesUnwritten >= 512 || endOfFile)
              {
                // We need to keep track of the number of '+' characters for the
                // contentLength in the HTTP POST.
                for(temp = 0; temp < bytesUnwritten; temp++)
                {
                  if(outBuff[temp] == '+')
                  {
                    numPlus++;
                  }
                }
                for(totalWritten  = 0; bytesUnwritten != 0; \
                  bytesUnwritten -= bytesWritten, \
                    totalWritten   += bytesWritten)
                {
                  if(f_write(&outFile, outBuff + totalWritten, bytesUnwritten, \
                    (UINT*)&bytesWritten) != FR_OK)
                  {
                    // Something terrible happened
                    f_close(&inFile);
                    f_close(&outFile);

                    goto end;
                  }
                }

                // Keeping track of the base64FileSize for calculating the
                // content length in the HTTP POST command.
                base64FileSize += totalWritten;
              }

              // Remove after thorough testing
              if(endOfFile)
              {
                // Shouldn't happen I think.
                if(jpegBytesLeft  != 0) {while(1);}
                if(bytesUnwritten != 0) {while(1);}
                if(numUnEncoded   != 0) {while(1);}
              }
            }
          }
          else // f_read of JPEG file error
          {
            f_close(&inFile);
            f_close(&outFile);

            goto end;
          }
        }

        // Close the JPEG and base64 files
        f_close(&inFile);
        f_close(&outFile);

        // Open the base64 file for read
        if(f_open(&outFile, OUT_FILE_NAME, FA_READ) == FR_OK)
        {

          contentLength = 9 /* "TheImage=" */ + base64FileSize + (2*numPlus);
          if(gpsValid)
          {
            contentLength += 8 /* "GPSdata=" */
              + strlen(lastGPS.exositeLatitude)
                + 1 /* '_' */
                  + strlen(lastGPS.exositeLongitude)
                    + 1 /* '&' */;
          }

          if(!openExositeSocket(contentLength, (char *)outBuff))
          {
            f_close(&outFile);

            goto end;
          }

          if(gpsValid)
          {
            sprintf((char *)outBuff, "GPSdata=%s_%s&", lastGPS.exositeLatitude, lastGPS.exositeLongitude);
            modemSend((char *)outBuff, TRUE);
          }

          modemSend("TheImage=", FALSE);

          // Send the base64 file
          while(1)
          {
            if(f_read(&outFile, outBuff, 256, (UINT*)&bytesRead) == FR_OK)
            {
              if(bytesRead != 0)
              {
                // Modem send requires null terminated character strings
                outBuff[bytesRead] = (char)NULL;
                modemSend((char *)outBuff, TRUE);
              }
              else
              {
                // End of file
                break;
              }
            }
            else
            {
              f_close(&outFile);
              goto end;
            }
          }


          //OSTimeDlyHMSM(0, 0, 1, 0);
          waitResponse("Content-Length: 0", 17, 10000);
          modemSend("+++\n", FALSE);
          waitResponse("NO CARRIER", 10, STD_TIMEOUT*5);

          modemSendAndWaitResp("AT#SH=3\r\n", "OK", 2, STD_TIMEOUT, 5);

          /* Close the base64 file */
          f_close(&outFile);
        }
      }
    }

  end:
    f_mount(0, NULL);

    if(OSMutexPost(SDMutex) != OS_ERR_NONE)
    {
      while(1);
    }
  }
}

static int openExositeSocket(int contentLength, char *buffer)
{
  // Open the socket
  // Okay if it returns error, the PDP context may already be active.
  modemSendAndWaitResp("AT#SGACT=3,1\r\n", "OK", 2, STD_TIMEOUT, 5);


  if( modemSendAndWaitResp("AT#SD=3,0,80,\"m2.exosite.com\",0\r\n", "CONNECT",
                           7, STD_TIMEOUT*5, 5) != RECV_ERR_NONE)
  {
    return FALSE;
  }

  modemSend("POST /onep:v1/stack/alias HTTP/1.1\x0A", FALSE);
  modemSend("Host: m2.exosite.com\x0A", FALSE);
  modemSend("X-Exosite-CIK: 312330d15297814c0855f25f6d6bda93d10b450f\x0A", FALSE);
  modemSend("Content-Type: application/x-www-form-urlencoded; charset=utf-8\x0A", FALSE);
  modemSend("Accept: application/x-www-form-urlencoded\x0A", FALSE);

  sprintf((char *)buffer, "Content-Length: %d\x0A\x0A", contentLength);
  modemSend((char *)buffer, FALSE);

  return TRUE;
}

void gpsUpdate()
{
  char data[32];
  int contentLength = 8 /* "GPSdata=" */
    + strlen(lastGPS.exositeLatitude)
      + 1 /* '_' */
        + strlen(lastGPS.exositeLongitude);
  if(gpsValid && openExositeSocket(contentLength, data))
  {
    sprintf(data, "GPSdata=%s_%s", lastGPS.exositeLatitude, lastGPS.exositeLongitude);
    modemSend(data, FALSE);

    waitResponse("Content-Length: 0", 17, STD_TIMEOUT*10);
    modemSend("+++\r", FALSE);
    waitResponse("NO CARRIER", 10, STD_TIMEOUT*5);

    modemSendAndWaitResp("AT#SH=3\r\n", "OK", 2, STD_TIMEOUT, 5);
  }
}

static void modemSend(const char *str, uint8_t literalPlus)
{
  static int first = TRUE;
  int i;
  for(i = 0; str[i] != 0; i++)
  {
    if(!literalPlus || str[i] != '+')
    {
      USART_SendData(USART1, (uint8_t) str[i]);
    }
    else
    {
      USART_SendData(USART1, (uint8_t) '%');
      while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
      {}
      USART_SendData(USART1, (uint8_t) '2');
      while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
      {}
      USART_SendData(USART1, (uint8_t) 'B');
    }

    if(first == TRUE)
    {
      first = FALSE;
      OSTimeDlyHMSM(0, 0, 0, 10);
    }

    /* Loop until the end of transmission */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
    {}
    //LCD_DisplayStringLine(0,"Success");
  }
}

static int modemSendAndWaitResp(const char *cmd, const char *resp, int numChar, int timeout_ms, int maxRepeat)
{
  int i;

  // Transmit fails to send if user sets maxRepeat <= 0
  int retval = TRAN_FAIL;

  for(i = 0; i < maxRepeat; i++)
  {
    modemSend(cmd, FALSE);
    retval = waitResponse(resp, numChar, timeout_ms);
    if(retval != RECV_TIMEOUT)
    {
      return retval;
    }
  }
  return retval;
}

static int getResponse(const char *start, const char *end, int timeout_ms)
{
  int startSize = strlen(start);
  int endSize    = strlen(end);
  rx_response[0] = (char)NULL;

  do {
    if(modemGetNextLine(timeout_ms) == RECV_TIMEOUT)
    {
      return RECV_TIMEOUT;
    }
  } while(strncmp(rx_line,start,startSize) && strcmp(rx_line,rx_error));

  if(strcmp(rx_line,rx_error) == 0)
  {
    return RECV_ERROR_STR;
  }

  strcpy(rx_response, rx_line);

  // Keep copying lines until we find the end, an error, or run out of memory
  while(strncmp(rx_line,end,endSize) && strcmp(rx_line,rx_error))
  {
    if(modemGetNextLine(timeout_ms) == RECV_TIMEOUT)
    {
      return RECV_TIMEOUT;
    }
    if(strlen(rx_response) + strlen(rx_line) > RX_BUFFER_SIZE)
    {
      //Ran out of memory
      return RECV_NO_MEMORY;
    }
    else
    {
      strcpy(rx_response + strlen(rx_response), rx_line);
    }
  }

  if(strcmp(rx_line,rx_error) == 0)
  {
    return RECV_ERROR_STR;
  }
  return RECV_ERR_NONE;
}

static int waitResponse(const char *target, int numChar, int timeout_ms)
{
  do{
    if(modemGetNextLine(timeout_ms) == RECV_TIMEOUT)
    {
      return RECV_TIMEOUT;
    }
  } while(strncmp(rx_line,target,numChar) && strcmp(rx_line,rx_error));

  if(strcmp(rx_line,rx_error) == 0)
  {
    return RECV_ERROR_STR;
  }
  return RECV_ERR_NONE;
}

static int modemGetNextLine(int timeout_ms)
{
  //
  int lineIndex = 0;
  int in, out; //Temp variables for holding globals that the UART interrupt changes
  INT8U perr;

  rx_line[0] = (char)NULL;

  //Disable UART interrupt while we check the globals it changes
  //USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);

  while( lineIndex == 0 || (rx_line[lineIndex-1] != '\n'))
  {
    in  = rx_in;
    out = rx_out;

    // Wait for more data if there is none
    if (in == out)
    {
      OSSemPend(uartRecvSem, timeout_ms, &perr);
      in  = rx_in;
      out = rx_out;

      // Check for pend error combined with no new data
      if((perr != OS_ERR_NONE) && (in == out))
      {
        return RECV_TIMEOUT;
      }

      //Enable interrupt because there is no new data
      //USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
      //do {
      //  in  = rx_in;
      //  out = rx_out;
      //} while( in == out);

      //Disable UART interrupt while we check the globals it changes
      //USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
    }
    else
    {
      rx_line[lineIndex] = rx_buffer[rx_out];
      lineIndex++;
      rx_out = (rx_out+1) % RX_BUFFER_SIZE;
    }
  }  //while

  //USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

  rx_line[lineIndex-1] = 0; //Strip off the newline and terminate the string

  return RECV_ERR_NONE;
}

void Modem_USART1_IRQ()
{
  if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
  {
    // We may need to read more than one byte in the FIFO.
    while((USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) && (RX_CIRC_INC(rx_in) != rx_out))
    {
      rx_buffer[rx_in] = (char)USART_ReceiveData(USART1);

      // Ignore any non-ASCII and control characters by not incrementing the
      // index rx_in.  Non-ASCII characters are probably garbage and we are not
      // expecting any control characters from this specific modem.
      if((rx_buffer[rx_in] >= ' ' && rx_buffer[rx_in] <= '~' )||
         rx_buffer[rx_in] == '\n' || rx_buffer[rx_in] == '\r')
      {
        rx_in = RX_CIRC_INC(rx_in);
        if(OSSemPost(uartRecvSem) != OS_ERR_NONE) {while(1);}
      }
    }
  }
}
