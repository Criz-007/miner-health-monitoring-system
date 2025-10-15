#ifndef ADS1292R_DRIVER_H
#define ADS1292R_DRIVER_H

#include <stdint.h>

int ads1292r_init(void);
void ads1292r_power_on(void);
void ads1292r_power_off(void);
void ads1292r_read_ecg_and_bp(uint16_t *bp_systolic, uint16_t *bp_diastolic);
void ads1292r_get_raw_ecg(int32_t *ch1_buffer, int32_t *ch2_buffer, uint16_t *length);

#endif