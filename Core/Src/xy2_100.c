#include "xy2_100.h"
#include "stm32f4xx.h"

#define XY2_PORT             GPIOD
#define XY2_PORT_CLK         RCC_AHB1ENR_GPIODEN
#define XY2_CLK_PIN          0U
#define XY2_SYNC_PIN         1U
#define XY2_X_PIN            2U
#define XY2_Y_PIN            3U

#define XY2_CLK_MASK         (1UL << XY2_CLK_PIN)
#define XY2_SYNC_MASK        (1UL << XY2_SYNC_PIN)
#define XY2_X_MASK           (1UL << XY2_X_PIN)
#define XY2_Y_MASK           (1UL << XY2_Y_PIN)
#define XY2_DATA_MASK        (XY2_X_MASK | XY2_Y_MASK)
#define XY2_ALL_MASK         (XY2_CLK_MASK | XY2_SYNC_MASK | XY2_DATA_MASK)

static UINT8 xy2Initialized = 0;
static UINT32 xy2HalfPeriodCycles = 1;

static UINT8 XY2_100_EvenParity(UINT32 value)
{
    value ^= value >> 16;
    value ^= value >> 8;
    value ^= value >> 4;
    value &= 0x0FU;

    return (UINT8)((0x6996U >> value) & 1U);
}

static UINT32 XY2_100_BuildWord(INT16 value)
{
    UINT32 word = (((UINT32)XY2_100_CONTROL_BITS & 0x07U) << 17)
                | (((UINT32)((UINT16)value)) << 1);

    word |= XY2_100_EvenParity(word >> 1);
    return word & 0x000FFFFFU;
}

static void XY2_100_DelayHalfPeriod(void)
{
    UINT32 start = DWT->CYCCNT;

    while ((DWT->CYCCNT - start) < xy2HalfPeriodCycles)
    {
        __NOP();
    }
}

void XY2_100_Init(void)
{
    UINT32 modeMask;

    if (xy2Initialized != 0)
    {
        return;
    }

    RCC->AHB1ENR |= XY2_PORT_CLK;
    (void)RCC->AHB1ENR;

    modeMask = (3UL << (XY2_CLK_PIN * 2U))
             | (3UL << (XY2_SYNC_PIN * 2U))
             | (3UL << (XY2_X_PIN * 2U))
             | (3UL << (XY2_Y_PIN * 2U));

    XY2_PORT->MODER = (XY2_PORT->MODER & ~modeMask)
                    | (1UL << (XY2_CLK_PIN * 2U))
                    | (1UL << (XY2_SYNC_PIN * 2U))
                    | (1UL << (XY2_X_PIN * 2U))
                    | (1UL << (XY2_Y_PIN * 2U));
    XY2_PORT->OTYPER &= ~XY2_ALL_MASK;
    XY2_PORT->OSPEEDR |= modeMask;
    XY2_PORT->PUPDR &= ~modeMask;
    XY2_PORT->BSRRH = XY2_ALL_MASK;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    xy2HalfPeriodCycles = SystemCoreClock / (XY2_100_CLOCK_HZ * 2UL);
    if (xy2HalfPeriodCycles == 0)
    {
        xy2HalfPeriodCycles = 1;
    }

    xy2Initialized = 1;
}

void XY2_100_SendPoint(INT16 x, INT16 y)
{
    UINT32 xWord;
    UINT32 yWord;
    INT32 bit;

    XY2_100_Init();

    xWord = XY2_100_BuildWord(x);
    yWord = XY2_100_BuildWord(y);

    XY2_PORT->BSRRL = XY2_SYNC_MASK;

    for (bit = (INT32)XY2_100_BITS_PER_FRAME - 1; bit >= 0; bit--)
    {
        UINT32 setMask = 0;

        if ((xWord & (1UL << bit)) != 0)
        {
            setMask |= XY2_X_MASK;
        }

        if ((yWord & (1UL << bit)) != 0)
        {
            setMask |= XY2_Y_MASK;
        }

        XY2_PORT->BSRRH = XY2_CLK_MASK | XY2_DATA_MASK;
        XY2_PORT->BSRRL = setMask;
        XY2_100_DelayHalfPeriod();

        XY2_PORT->BSRRL = XY2_CLK_MASK;
        XY2_100_DelayHalfPeriod();
    }

    XY2_PORT->BSRRH = XY2_CLK_MASK | XY2_SYNC_MASK | XY2_DATA_MASK;
}