#include "app.h"
#include <stdint.h>
#include <string.h>
#include "../SSC/esc_coe.h"
#include "../SSC/ecat_slv.h"
#include "../Core/Inc/galvo.h"

extern galvo_rxpdo_t GalvoRxPdo;
extern galvo_txpdo_t GalvoTxPdo;

static uint8_t app_initialized;

static void app_safe_outputs(void)
{
    Galvo_SafeOutput();
}

int get_object_dictionary_value(uint16_t index, uint8_t subindex, void *outbuf, size_t *outlen)
{
    int32_t nidx = SDO_findobject(index);
    if (nidx < 0) return -1;

    int16_t nsub = SDO_findsubindex(nidx, subindex);
    if (nsub < 0) return -2;

    const _objd *objd = &SDOobjects[nidx].objdesc[nsub];
    size_t bytes = (objd->bitlength + 7U) / 8U;

    if (outlen == 0) return -3;

    if (outbuf == 0)
    {
        *outlen = bytes;
        return 0;
    }

    if (*outlen < bytes) return -4;

    if (objd->data != 0)
    {
        memcpy(outbuf, objd->data, bytes);
        *outlen = bytes;
        return 0;
    }

    if (bytes <= sizeof(objd->value))
    {
        uint32_t value = objd->value;
        memcpy(outbuf, &value, bytes);
        *outlen = bytes;
        return 0;
    }

    return -5;
}

__attribute__((weak)) void fsmc_send_to_fpga(const void *data, size_t len)
{
    (void)data;
    (void)len;
}

void app_init(void)
{
    esc_cfg_t config;

    memset(&config, 0, sizeof(config));
    config.watchdog_cnt = 1000;
    config.safeoutput_override = app_safe_outputs;

    ecat_slv_init(&config);
    app_initialized = 1;
}

void interpolation(void)
{
    Galvo_ApplyRxPdo(&GalvoRxPdo);
}

void cb_set_outputs(void)
{
    interpolation();
}

void cb_get_inputs(void)
{
    Galvo_GetTxPdo(&GalvoTxPdo);
}

void app_run(void)
{
    if (app_initialized == 0U)
    {
        return;
    }

    ecat_slv();
}
