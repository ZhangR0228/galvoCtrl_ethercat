#ifndef __MY_BSP_EC_H__
#define __MY_BSP_EC_H__

/* 包含头文件 ----------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "ecat_def.h"
#include "time.h"

/* 类型定义 --------------------------------------------------------------*/

/* 宏定义 --------------------------------------------------------------------*/
#define RST_L     HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
#define RST_H     HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);

#define GENERAL_TIMx                     TIM2
#define GENERAL_TIM_RCC_CLK_ENABLE()     __HAL_RCC_TIM2_CLK_ENABLE()
#define GENERAL_TIM_RCC_CLK_DISABLE()    __HAL_RCC_TIM2_CLK_DISABLE()
#define GENERAL_TIM_IRQ                  TIM2_IRQn
#define GENERAL_TIM_INT_FUN              TIM2_IRQHandler

extern TIM_HandleTypeDef htimx;
/* 扩展变量 ------------------------------------------------------------------*/
/* 函数声明 ------------------------------------------------------------------*/


void INIT_SYNC0_EXTI4(void);//PD4 SYNC0
void INIT_SYNC1_EXTI7(void);//PD7 SYNC1

void INIT_ISR_EXTI3(void);//PD3 ISR


void INIT_ESC_RST(void);//PD2 ISR


void INIT_ESC_TIME(void );

#endif  // __BSP_GPIO_H__
