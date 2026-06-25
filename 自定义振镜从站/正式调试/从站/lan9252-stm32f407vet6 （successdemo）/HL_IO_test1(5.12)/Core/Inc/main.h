/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOE
#define LED3_Pin GPIO_PIN_3
#define LED3_GPIO_Port GPIOE
#define LED4_Pin GPIO_PIN_4
#define LED4_GPIO_Port GPIOE
#define LED5_Pin GPIO_PIN_5
#define LED5_GPIO_Port GPIOE
#define LED6_Pin GPIO_PIN_6
#define LED6_GPIO_Port GPIOE
#define SYS_RUN_Pin GPIO_PIN_2
#define SYS_RUN_GPIO_Port GPIOC
#define SYS_ERR_Pin GPIO_PIN_3
#define SYS_ERR_GPIO_Port GPIOC
#define LED7_Pin GPIO_PIN_7
#define LED7_GPIO_Port GPIOE
#define KEY0_Pin GPIO_PIN_8
#define KEY0_GPIO_Port GPIOE
#define KEY1_Pin GPIO_PIN_9
#define KEY1_GPIO_Port GPIOE
#define KEY2_Pin GPIO_PIN_10
#define KEY2_GPIO_Port GPIOE
#define KEY3_Pin GPIO_PIN_11
#define KEY3_GPIO_Port GPIOE
#define KEY4_Pin GPIO_PIN_12
#define KEY4_GPIO_Port GPIOE
#define KEY5_Pin GPIO_PIN_13
#define KEY5_GPIO_Port GPIOE
#define KEY6_Pin GPIO_PIN_14
#define KEY6_GPIO_Port GPIOE
#define KEY7_Pin GPIO_PIN_15
#define KEY7_GPIO_Port GPIOE
#define Lan9252_E_LED_Pin GPIO_PIN_15
#define Lan9252_E_LED_GPIO_Port GPIOD
#define SPI3_CS_Pin GPIO_PIN_15
#define SPI3_CS_GPIO_Port GPIOA
#define L9252_RST_Pin GPIO_PIN_2
#define L9252_RST_GPIO_Port GPIOD
#define L9252_ISR_Pin GPIO_PIN_3
#define L9252_ISR_GPIO_Port GPIOD
#define L9252_ISR_EXTI_IRQn EXTI3_IRQn
#define L9252_SYN0_Pin GPIO_PIN_4
#define L9252_SYN0_GPIO_Port GPIOD
#define L9252_SYN0_EXTI_IRQn EXTI4_IRQn
#define L9252_SYN1_Pin GPIO_PIN_7
#define L9252_SYN1_GPIO_Port GPIOD
#define L9252_SYN1_EXTI_IRQn EXTI9_5_IRQn
#define LED0_Pin GPIO_PIN_0
#define LED0_GPIO_Port GPIOE
#define LED1_Pin GPIO_PIN_1
#define LED1_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */
#define XY2_CLK_Pin GPIO_PIN_8
#define XY2_CLK_GPIO_Port GPIOA
#define XY2_SYNC_Pin GPIO_PIN_6
#define XY2_SYNC_GPIO_Port GPIOC
#define XY2_X_Pin GPIO_PIN_7
#define XY2_X_GPIO_Port GPIOC
#define XY2_Y_Pin GPIO_PIN_9
#define XY2_Y_GPIO_Port GPIOC
#define OLED_SCK_Pin GPIO_PIN_10
#define OLED_SCK_GPIO_Port GPIOB
#define OLED_MOSI_Pin GPIO_PIN_14
#define OLED_MOSI_GPIO_Port GPIOB
#define OLED_CS_Pin GPIO_PIN_10
#define OLED_CS_GPIO_Port GPIOD

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
