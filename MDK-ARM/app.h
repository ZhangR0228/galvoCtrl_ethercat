#ifndef __APP_H__
#define __APP_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int get_object_dictionary_value(uint16_t index, uint8_t subindex, void *outbuf, size_t *outlen);
void app_init(void);
void app_run(void);
void interpolation(void);
void fsmc_send_to_fpga(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
