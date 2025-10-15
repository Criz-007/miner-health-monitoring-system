#ifndef MAX30102_DRIVER_H
#define MAX30102_DRIVER_H

#include <stdint.h>

int max30102_init(void);
void max30102_power_on(void);
void max30102_power_off(void);
void max30102_read_data(uint8_t *spo2, uint16_t *heart_rate);

#endif