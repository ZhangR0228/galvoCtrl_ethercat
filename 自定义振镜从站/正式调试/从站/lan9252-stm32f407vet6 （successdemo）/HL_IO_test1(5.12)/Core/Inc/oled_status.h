#ifndef __OLED_STATUS_H__
#define __OLED_STATUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

void OledStatus_Init(void);
void OledStatus_Task(void);
void OledStatus_SetEthercat(uint16_t status, uint16_t last_sequence, uint16_t frame_counter);
void OledStatus_SetCommand(int16_t x_code, int16_t y_code, uint16_t sequence, uint16_t flags);

#ifdef __cplusplus
}
#endif

#endif
