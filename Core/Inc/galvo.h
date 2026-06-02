#ifndef __GALVO_H__
#define __GALVO_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GALVO_CONTROL_ENABLE     0x0001U
#define GALVO_CONTROL_LASER      0x0002U
#define GALVO_CONTROL_LATCH      0x0004U

#define GALVO_STATUS_READY       0x0001U
#define GALVO_STATUS_ENABLED     0x0002U
#define GALVO_STATUS_LIMITED     0x0004U
#define GALVO_STATUS_START_MISS  0x0008U
#define GALVO_STATUS_FAULT       0x8000U

typedef struct
{
   uint16_t control;
   int32_t start_x;
   int32_t start_y;
   int32_t end_x;
   int32_t end_y;
   uint32_t cycle_counter;
   uint16_t flags;
} galvo_rxpdo_t;

typedef struct
{
   uint16_t status;
   int32_t start_x;
   int32_t start_y;
   int32_t end_x;
   int32_t end_y;
   int32_t actual_x;
   int32_t actual_y;
   int32_t following_error_x;
   int32_t following_error_y;
   uint32_t sequence;
} galvo_txpdo_t;

void Galvo_Init(void);
void Galvo_ApplyRxPdo(const galvo_rxpdo_t *rxpdo);
void Galvo_GetTxPdo(galvo_txpdo_t *txpdo);
void Galvo_SafeOutput(void);
int Galvo_ReadActualPosition(int32_t *actual_x, int32_t *actual_y);

#ifdef __cplusplus
}
#endif

#endif /* __GALVO_H__ */
