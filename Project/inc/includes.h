/**
  ******************************************************************************
  * <h2><center>&copy; COPYRIGHT 2012 Embest Tech. Co., Ltd.</center></h2>
  * @file    includes.h
  * @author  CMP Team
  * @version V1.0.0
  * @date    28-December-2012
  * @brief   Library header file.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef  __INCLUDES_H__
#define  __INCLUDES_H__
/* Includes ------------------------------------------------------------------*/
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#include  <stdlib.h>
#include  <stdarg.h>
#include  <stdint.h>
#include <STMPE811QTR.h>

#include  "ucos_ii.h"
#include  "cpu.h"
#include  "lib.h"
#include  "stm32f4xx_conf.h"
#include  "stm32f4xx.h"
#include  "app_cfg.h"
#include  "bsp.h"
#include "app_it.h"
#include "stm32f4_discovery_sdio_sd.h"
#include "stm32f4_discovery_debug.h"
#include "ff.h"
#include "stm32f4_discovery_lcd.h"
#include "stmpe811qtr.h"
#include "LCD_Touch_Calibration.h"
#include "sound_output.h"
#include "waverecorder.h"
#include "selfTest.h"
#include "stm32f401_discovery_lsm303dlhc.h"
#include "accelerometer.h"
#include "dcmi_ov9655.h"
#include "bmp.h"
#include "modem.h"
#include "jpeglib.h"
#include "decode.h"
#include "encode.h"

#endif /*__INCLUDES_H__*/

/******************** COPYRIGHT 2012 Embest Tech. Co., Ltd.*****END OF FILE****/
