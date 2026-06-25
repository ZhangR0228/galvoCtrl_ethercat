#include "my_bsp_ec.h"

#include "tim.h"
TIM_HandleTypeDef htimx;

void INIT_ESC_RST(void)//PD2     ------> RST
{
   /* 定义IO硬件初始化结构体变量 */
  GPIO_InitTypeDef GPIO_InitStruct;
	
  __HAL_RCC_GPIOD_CLK_ENABLE();	
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;	
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct); 
 
}


void INIT_ISR_EXTI3(void)//PD3     ------> IRQ
{
   /* 定义IO硬件初始化结构体变量 */
  GPIO_InitTypeDef GPIO_InitStruct;
	
  __HAL_RCC_GPIOD_CLK_ENABLE();	
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);   
	
  HAL_NVIC_SetPriority(EXTI3_IRQn,1, 0);
	
}


void INIT_SYNC0_EXTI4(void)//PD4     ------> SYNC0
{
   /* 定义IO硬件初始化结构体变量 */
  GPIO_InitTypeDef GPIO_InitStruct;	
	
  __HAL_RCC_GPIOD_CLK_ENABLE();	
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);   	
	HAL_NVIC_SetPriority(EXTI4_IRQn,1, 1);
}

void INIT_SYNC1_EXTI7(void)//PD7     ------> SYNC1
{
   /* 定义IO硬件初始化结构体变量 */
  GPIO_InitTypeDef GPIO_InitStruct;	
	
  __HAL_RCC_GPIOD_CLK_ENABLE();	

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);   
		

	HAL_NVIC_SetPriority(EXTI9_5_IRQn,1, 1);

	
}


void INIT_ESC_TIME(void )
{
//  TIM_ClockConfigTypeDef sClockSourceConfig;
//  TIM_MasterConfigTypeDef sMasterConfig;

//  htimx.Instance = GENERAL_TIMx;
//  htimx.Init.Prescaler = 42-1;
//  htimx.Init.CounterMode = TIM_COUNTERMODE_UP;
//  htimx.Init.Period = 2000-1;
//  htimx.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
//  htimx.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;	
//  HAL_TIM_Base_Init(&htimx);

//  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;//选择使用内部时钟
//  HAL_TIM_ConfigClockSource(&htimx, &sClockSourceConfig);

//  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
//  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
//  HAL_TIMEx_MasterConfigSynchronization(&htimx, &sMasterConfig);  
//	#if ECAT_TIMER_INT
//  //HAL_TIM_Base_Start_IT(&htimx);
//	__NOP();
//	#endif	
	
//void MX_TIM7_Init(void)
{



  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htimx.Instance = TIM2;
  htimx.Init.Prescaler = 42-1;
  htimx.Init.CounterMode = TIM_COUNTERMODE_UP;
  htimx.Init.Period = 2000-1;
  htimx.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htimx) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htimx, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }


}
	
	

}