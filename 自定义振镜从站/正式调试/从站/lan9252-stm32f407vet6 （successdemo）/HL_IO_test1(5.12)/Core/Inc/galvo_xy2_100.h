#ifndef __GALVO_XY2_100_H__
#define __GALVO_XY2_100_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define GALVO_CONTROL_ENABLE   0x0001u
#define GALVO_STATUS_ENABLED   0x0001u
#define GALVO_STATUS_STALE     0x0002u

void GalvoXY2_Init(void);
void GalvoXY2_SetTarget(int16_t x_code, int16_t y_code, uint16_t sequence, uint16_t control_flags);
void GalvoXY2_TimerTick(void);
uint16_t GalvoXY2_GetStatus(void);
uint16_t GalvoXY2_GetLastSequence(void);
uint16_t GalvoXY2_GetFrameCounter(void);

#ifdef __cplusplus
}
#endif

#endif
