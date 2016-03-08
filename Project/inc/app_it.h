/*******************************************************************************
* File:   app_it.c
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To initialize all interrupt call backs and to contain all the call
*         backs if not already defined somewhere else.
*******************************************************************************/
#include "stm32f4xx.h"

#ifndef __APP_IT_H
#define __APP_IT_H
extern uint8_t touched;
void itInit();
void TouchIRQ();
//void EXTI0_IRQHandler(void);
#endif