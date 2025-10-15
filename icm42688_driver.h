#ifndef ICM42688_DRIVER_H
#define ICM42688_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

int icm42688_init(void);
void icm42688_wakeup(void);
void icm42688_sleep(void);
void icm42688_read_accel(float *accel_x, float *accel_y, float *accel_z);
void icm42688_read_gyro(float *gyro_x, float *gyro_y, float *gyro_z);
bool icm42688_detect_fall(void);

#endif