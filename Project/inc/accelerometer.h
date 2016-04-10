/*******************************************************************************
* File:    accelerometer.c
* Purpose: Provide functionality to the accelerometer and compass
* Authors: Christian Cleveland and Natalie Astorga
*******************************************************************************/

#ifndef __ACCELEROMETER_H
#define __ACCELEROMETER_H

void accelerometerCompassInit(void);
void LSM303DLHC_CompassReadAcc(float* pfData);
void LSM303DLHC_CompassReadMag (float* pfData);
uint32_t LSM303DLHC_TIMEOUT_UserCallback(void);

#endif
