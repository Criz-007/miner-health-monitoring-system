#ifndef TMP117_DRIVER_H
#define TMP117_DRIVER_H

#include <stdint.h>

int tmp117_init(void);
void tmp117_wakeup(void);
void tmp117_sleep(void);
float tmp117_read_temperature(void);
void tmp117_set_alert_limits(float high_limit, float low_limit);

#endif