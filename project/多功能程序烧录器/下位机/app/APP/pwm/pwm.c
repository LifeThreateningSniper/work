#include "pwm.h"

/*
初始化TIM3_IC
*/
void TIM3_IC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TimeBase_InitStructure;
	TIM_ICInitTypeDef TIM_InitStructure;

	// RCC使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	// 配置GPIO口 A6 TIM3_CH1
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// 配置时钟源
	TIM_InternalClockConfig(TIM3);
	// 配置时基单元
	TimeBase_InitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TimeBase_InitStructure.TIM_CounterMode=TIM_CounterMode_Up;  // 向上计数
	TimeBase_InitStructure.TIM_Period=65536-1;  // ARR
	TimeBase_InitStructure.TIM_Prescaler=72-1;  // PSC
	TimeBase_InitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM3, &TimeBase_InitStructure);
	
	// 配置IC单元
	TIM_InitStructure.TIM_Channel=TIM_Channel_1;  // 定时器通道
	TIM_InitStructure.TIM_ICFilter=0xF;  // 滤波器
	TIM_InitStructure.TIM_ICPolarity=TIM_ICPolarity_Rising;  // 边沿极性选择
	TIM_InitStructure.TIM_ICPrescaler=TIM_ICPSC_DIV1;  // 分频器
	TIM_InitStructure.TIM_ICSelection=TIM_ICSelection_DirectTI; // 选择器
	TIM_PWMIConfig(TIM3, &TIM_InitStructure); // 区别在这里
	
	// 配置从模式
	TIM_SelectInputTrigger(TIM3, TIM_TS_TI1FP1);  // 选择TI1FP1通道
	TIM_SelectSlaveMode(TIM3, TIM_SlaveMode_Reset);  // 选择Reset模式
	// 开启定时器
	TIM_Cmd(TIM3, ENABLE);
}

/*
获取TIM3_CH1 输入波形频率
*/
uint32_t Get_TIM3_CH1_Freq(void)
{
	return 72000000/(TIM_GetPrescaler(TIM3)+1)/ (TIM_GetCapture1(TIM3)+1);
}

/*
获取TIM3_PWMI 模式下占空比
*/

uint32_t Get_TIM3_Duty(void)
{
	return (TIM_GetCapture2(TIM3) + 1)*100 / (TIM_GetCapture1(TIM3) + 1);
}
