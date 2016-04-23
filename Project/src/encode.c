/**
  ******************************************************************************
  * @file    LibJPEG/LibJPEG_Encoding/Src/encode.c 
  * @author  MCD Application Team
  * @version V1.0.3
  * @date    18-November-2015
  * @brief   This file contain the compress method.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "encode.h"

/* Private typedef -----------------------------------------------------------*/
   /* This struct contains the JPEG compression parameters */
   static struct jpeg_compress_struct cinfo; 
   /* This struct represents a JPEG error handler */
   static struct jpeg_error_mgr jerr; 
  
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Jpeg Encode
  * @param  file:          pointer to the bmp file
  * @param  file1:         pointer to the jpg file  
  * @param  width:         image width
  * @param  height:        image height
  * @param  image_quality: image quality
  * @param  buff:          pointer to the image line
  * @retval None
  */
void jpeg_encode(FIL *file, FIL *file1, uint32_t width, uint32_t height, uint32_t image_quality, uint8_t * buff)
{
#ifdef BMP_16BIT
	int p16Index, p24Index;
#endif
    
  /* Encode BMP Image to JPEG */  
  JSAMPROW row_pointer;    /* Pointer to a single row */
  uint32_t bytesread;
            
  /* Step 1: allocate and initialize JPEG compression object */
  /* Set up the error handler */
  cinfo.err = jpeg_std_error(&jerr);
  
  /* Initialize the JPEG compression object */  
  jpeg_create_compress(&cinfo);
  
  /* Step 2: specify data destination */
  jpeg_stdio_dest(&cinfo, file1);
  
  /* Step 3: set parameters for compression */
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  
  /* Set default compression parameters */
  jpeg_set_defaults(&cinfo);
  
  cinfo.dct_method  = JDCT_FLOAT;    
  
  jpeg_set_quality(&cinfo, image_quality, TRUE);
  
  /* Step 4: start compressor */
  jpeg_start_compress(&cinfo, TRUE);
  
  /* Bypass the header bmp file */
#ifdef BMP_16BIT
  f_read(file, buff, 70, (UINT*)&bytesread);
#elif  BMP_24BIT
  f_read(file, buff, 54, (UINT*)&bytesread);
#endif
  
  while (cinfo.next_scanline < cinfo.image_height)
  {          
#ifdef BMP_16BIT
    if(f_read(file, buff, width*2, (UINT*)&bytesread) == FR_OK)
    {
			// Convert to 24 bit because that is what the LibJPEG expects
			// In order to not waste memory, we use the same buffer and start at the
			// end and work our way to the beginning
			for(p16Index = width*2 - 3, p24Index = width*3 - 1; p16Index > 0; p16Index -= 2)
      {
				//The 16 bit RGB code is 5-6-5
				//Starting with blue
				while(p24Index == p16Index + 1);
				while(p24Index < 0);
				while(p16Index < 0);
				buff[p24Index--] = (buff[p16Index + 1] & 0x1F) << 3;
				
				//Green
				while(p24Index == p16Index || p24Index == p16Index + 1);
				while(p24Index < 0);
				while(p16Index < 0);
				buff[p24Index--] = (buff[p16Index] << 5) | ((buff[p16Index + 1] & 0xE0) >> 3);
				
				//Red
				while(p24Index != 0 && p16Index!= 0 && p24Index == p16Index);
				while(p24Index < 0);
				while(p16Index < 0);
				buff[p24Index--] = buff[p16Index] & 0xF8;
      }
      row_pointer = (JSAMPROW)buff;
      jpeg_write_scanlines(&cinfo, &row_pointer, 1);          
    }
#elif  BMP_24BIT
		if(f_read(file, buff, width*3, (UINT*)&bytesread) == FR_OK)
    {
      row_pointer = (JSAMPROW)buff;
      jpeg_write_scanlines(&cinfo, &row_pointer, 1);          
    }
#endif
  }
  /* Step 5: finish compression */
  jpeg_finish_compress(&cinfo);
  
  /* Step 6: release JPEG compression object */
  jpeg_destroy_compress(&cinfo);
    
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
