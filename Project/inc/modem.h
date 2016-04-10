/*******************************************************************************
* File:   modem.h
* Author: Christian Cleveland and Natalie Astorga
* Brief:  To provide functionality to the 4G LTE and GPS modem LE910-SVG
*******************************************************************************/

#ifndef __MODEM_H
#define __MODEM_H

void modemInit(void);
void Modem_USART1_IRQ(void);

#endif
