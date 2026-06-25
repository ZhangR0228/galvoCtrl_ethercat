#include "galvo_xy2_100.h"

#define XY2_CLK_PIN      GPIO_PIN_8
#define XY2_CLK_PORT     GPIOA
#define XY2_SYNC_PIN     GPIO_PIN_6
#define XY2_SYNC_PORT    GPIOC
#define XY2_X_PIN        GPIO_PIN_7
#define XY2_X_PORT       GPIOC
#define XY2_Y_PIN        GPIO_PIN_9
#define XY2_Y_PORT       GPIOC

#define XY2_FRAME_BITS       20u
#define XY2_INTERP_FRAMES    10
#define XY2_STALE_FRAMES     500u

typedef struct
{
    volatile int32_t current_x;
    volatile int32_t current_y;
    volatile int32_t start_x;
    volatile int32_t start_y;
    volatile int32_t target_x;
    volatile int32_t target_y;
    volatile uint16_t sequence;
    volatile uint16_t control_flags;
    volatile uint16_t status;
    volatile uint16_t frame_counter;
    volatile uint16_t frames_since_update;
    volatile uint8_t interp_index;
} GalvoState;

static GalvoState sGalvo;

static inline void gpio_set(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = pin;
}

static inline void gpio_reset(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = pin << 16u;
}

static inline void xy2_delay(void)
{
    __NOP();
    __NOP();
    __NOP();
    __NOP();
}

static uint32_t xy2_pack_word(int16_t code)
{
    uint32_t payload = ((uint16_t)code) & 0xFFFFu;
    uint32_t word = payload << 1u;
    uint32_t parity = 0u;

    for (uint32_t i = 0u; i < 16u; ++i) {
        parity ^= (payload >> i) & 0x1u;
    }

    word |= parity;
    return word;
}

static void xy2_write_bit(uint8_t x_bit, uint8_t y_bit)
{
    if (x_bit) {
        gpio_set(XY2_X_PORT, XY2_X_PIN);
    } else {
        gpio_reset(XY2_X_PORT, XY2_X_PIN);
    }

    if (y_bit) {
        gpio_set(XY2_Y_PORT, XY2_Y_PIN);
    } else {
        gpio_reset(XY2_Y_PORT, XY2_Y_PIN);
    }

    gpio_set(XY2_CLK_PORT, XY2_CLK_PIN);
    xy2_delay();
    gpio_reset(XY2_CLK_PORT, XY2_CLK_PIN);
    xy2_delay();
}

static void xy2_send_frame(int16_t x_code, int16_t y_code)
{
    uint32_t x_word = xy2_pack_word(x_code);
    uint32_t y_word = xy2_pack_word(y_code);

    gpio_set(XY2_SYNC_PORT, XY2_SYNC_PIN);
    xy2_delay();

    for (int bit = (int)XY2_FRAME_BITS - 1; bit >= 0; --bit) {
        xy2_write_bit((uint8_t)((x_word >> bit) & 0x1u),
                      (uint8_t)((y_word >> bit) & 0x1u));
    }

    gpio_reset(XY2_SYNC_PORT, XY2_SYNC_PIN);
}

void GalvoXY2_Init(void)
{
    gpio_reset(XY2_CLK_PORT, XY2_CLK_PIN);
    gpio_reset(XY2_SYNC_PORT, XY2_SYNC_PIN);
    gpio_reset(XY2_X_PORT, XY2_X_PIN);
    gpio_reset(XY2_Y_PORT, XY2_Y_PIN);

    sGalvo.current_x = 0;
    sGalvo.current_y = 0;
    sGalvo.start_x = 0;
    sGalvo.start_y = 0;
    sGalvo.target_x = 0;
    sGalvo.target_y = 0;
    sGalvo.sequence = 0;
    sGalvo.control_flags = 0;
    sGalvo.status = GALVO_STATUS_STALE;
    sGalvo.frame_counter = 0;
    sGalvo.frames_since_update = XY2_STALE_FRAMES;
    sGalvo.interp_index = XY2_INTERP_FRAMES;
}

void GalvoXY2_SetTarget(int16_t x_code, int16_t y_code, uint16_t sequence, uint16_t control_flags)
{
    if ((sequence != sGalvo.sequence) ||
        (x_code != (int16_t)sGalvo.target_x) ||
        (y_code != (int16_t)sGalvo.target_y) ||
        (control_flags != sGalvo.control_flags)) {
        __disable_irq();
        sGalvo.start_x = sGalvo.current_x;
        sGalvo.start_y = sGalvo.current_y;
        sGalvo.target_x = x_code;
        sGalvo.target_y = y_code;
        sGalvo.sequence = sequence;
        sGalvo.control_flags = control_flags;
        sGalvo.frames_since_update = 0;
        sGalvo.interp_index = 0;
        __enable_irq();
    }
}

void GalvoXY2_TimerTick(void)
{
    int32_t x = sGalvo.current_x;
    int32_t y = sGalvo.current_y;

    if (sGalvo.interp_index < XY2_INTERP_FRAMES) {
        int32_t step = (int32_t)sGalvo.interp_index + 1;
        x = sGalvo.start_x + ((sGalvo.target_x - sGalvo.start_x) * step) / XY2_INTERP_FRAMES;
        y = sGalvo.start_y + ((sGalvo.target_y - sGalvo.start_y) * step) / XY2_INTERP_FRAMES;
        sGalvo.interp_index++;
    } else {
        x = sGalvo.target_x;
        y = sGalvo.target_y;
    }

    sGalvo.current_x = x;
    sGalvo.current_y = y;

    if ((sGalvo.control_flags & GALVO_CONTROL_ENABLE) != 0u) {
        xy2_send_frame((int16_t)x, (int16_t)y);
        sGalvo.status = GALVO_STATUS_ENABLED;
    } else {
        gpio_reset(XY2_SYNC_PORT, XY2_SYNC_PIN);
        gpio_reset(XY2_CLK_PORT, XY2_CLK_PIN);
        sGalvo.status = 0u;
    }

    if (sGalvo.frames_since_update < XY2_STALE_FRAMES) {
        sGalvo.frames_since_update++;
    } else {
        sGalvo.status |= GALVO_STATUS_STALE;
    }

    sGalvo.frame_counter++;
}

uint16_t GalvoXY2_GetStatus(void)
{
    return sGalvo.status;
}

uint16_t GalvoXY2_GetLastSequence(void)
{
    return sGalvo.sequence;
}

uint16_t GalvoXY2_GetFrameCounter(void)
{
    return sGalvo.frame_counter;
}
