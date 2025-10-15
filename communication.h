#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdint.h>
#include <stdbool.h>

void communication_init(void);
void communication_send_data(uint8_t *data, uint16_t length, bool is_emergency);

#endif