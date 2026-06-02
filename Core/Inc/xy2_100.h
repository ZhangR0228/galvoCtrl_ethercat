#ifndef __XY2_100_H__
#define __XY2_100_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XY2_100_AXIS_MIN     (-524288L)
#define XY2_100_AXIS_MAX     (524287L)

typedef struct
{
   int32_t x;
   int32_t y;
   uint16_t control;
} xy2_100_frame_t;

void XY2_100_Init(void);
void XY2_100_SendFrame(const xy2_100_frame_t *frame);
void XY2_100_SendPosition(int32_t x, int32_t y, uint16_t control);
int32_t XY2_100_ClampAxis(int32_t value);

#ifdef __cplusplus
}
#endif

#endif /* __XY2_100_H__ */
