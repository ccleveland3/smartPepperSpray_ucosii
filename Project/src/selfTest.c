/*******************************************************************************
* File:    selfTest.c
* Purpose: Provide functionality to test hardware when the system starts
* Authors: Christian Cleveland and Natalie Astorga
*******************************************************************************/
#include <includes.h>


void fileSystemTest()
{
  //Assumes the file system is already mounted

  printf("Open a test file (message.txt) \n\r");
  ret = f_open(&file, "MESSAGE.TXT", FA_READ);
  if (ret) {
    printf("not exist the test file (message.txt)\n\r");
  } else {
    printf("Type the file content\n\r");
    for (;;) {
      ret = f_read(&file, buff, sizeof(buff), &br);	/* Read a chunk of file */
      if (ret || !br) {
        break;			/* Error or end of file */
      }
      buff[br] = 0;
      printf("%s",buff);
      printf("\n\r");
    }
    if (ret) {
      printf("Read the file error\n\r");
      fault_err(ret);
    }

    printf("Close the file\n\r");
    ret = f_close(&file);
    if (ret) {
      printf("Close the file error\n\r");
    }
  }

  /*  hello.txt write test*/
  OSTimeDlyHMSM(0, 0, 0, 50);
  printf("Create a new file (hello.txt)\n\r");
  ret = f_open(&file, "HELLO.TXT", FA_WRITE | FA_CREATE_ALWAYS);
  if (ret) {
    printf("Create a new file error\n\r");
    fault_err(ret);
  } else {
    printf("Write a text data. (hello.txt)\n\r");
    ret = f_write(&file, "Hello world!", 14, &bw);
    if (ret) {
      printf("Write a text data to file error\n\r");
    } else {
      printf("%u bytes written\n\r", bw);
    }
    OSTimeDlyHMSM(0, 0, 0, 50);
    printf("Close the file\n\r");
    ret = f_close(&file);
    if (ret) {
      printf("Close the hello.txt file error\n\r");
    }
  }

  /*  hello.txt read test*/
  OSTimeDlyHMSM(0, 0, 0, 50);
  printf("read the file (hello.txt)\n\r");
  ret = f_open(&file, "HELLO.TXT", FA_READ);
  if (ret) {
    printf("open hello.txt file error\n\r");
  } else {
    printf("Type the file content(hello.txt)\n\r");
    for (;;) {
      ret = f_read(&file, buff, sizeof(buff), &br);	/* Read a chunk of file */
      if (ret || !br) {
        break;			/* Error or end of file */
      }
      buff[br] = 0;
      printf("%s",buff);
      printf("\n\r");
    }
    if (ret) {
      printf("Read file (hello.txt) error\n\r");
      fault_err(ret);
    }

    printf("Close the file (hello.txt)\n\r");
    ret = f_close(&file);
    if (ret) {
      printf("Close the file (hello.txt) error\n\r");
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
  printf("Test completed\n\r");
}

/**
* @brief   FatFs err dispose
* @param  None
* @retval None
*/
void fault_err (FRESULT rc)
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
  printf("rc=%u FR_%s\n\r", (UINT)rc, str);
  STM_EVAL_LEDOn(LED6);
  while(1);
}