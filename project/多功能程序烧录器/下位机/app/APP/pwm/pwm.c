#include "pwm.h"

/*
��ʼ��TIM3_IC
*/
void TIM3_IC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TimeBase_InitStructure;
	TIM_ICInitTypeDef TIM_InitStructure;

	// RCCʹ��ʱ��
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	// ����GPIO�� A6 TIM3_CH1
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// ����ʱ��Դ
	TIM_InternalClockConfig(TIM3);
	// ����ʱ����Ԫ
	TimeBase_InitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TimeBase_InitStructure.TIM_CounterMode=TIM_CounterMode_Up;  // ���ϼ���
	TimeBase_InitStructure.TIM_Period=65536-1;  // ARR
	TimeBase_InitStructure.TIM_Prescaler=72-1;  // PSC
	TimeBase_InitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM3, &TimeBase_InitStructure);
	
	// ����IC��Ԫ
	TIM_InitStructure.TIM_Channel=TIM_Channel_1;  // ��ʱ��ͨ��
	TIM_InitStructure.TIM_ICFilter=0xF;  // �˲���
	TIM_InitStructure.TIM_ICPolarity=TIM_ICPolarity_Rising;  // ���ؼ���ѡ��
	TIM_InitStructure.TIM_ICPrescaler=TIM_ICPSC_DIV1;  // ��Ƶ��
	TIM_InitStructure.TIM_ICSelection=TIM_ICSelection_DirectTI; // ѡ����
	TIM_PWMIConfig(TIM3, &TIM_InitStructure); // ����������
	
	// ���ô�ģʽ
	TIM_SelectInputTrigger(TIM3, TIM_TS_TI1FP1);  // ѡ��TI1FP1ͨ��
	TIM_SelectSlaveMode(TIM3, TIM_SlaveMode_Reset);  // ѡ��Resetģʽ
	// ������ʱ��
	TIM_Cmd(TIM3, ENABLE);
}

/*
��ȡTIM3_CH1 ���벨��Ƶ��
*/
uint32_t Get_TIM3_CH1_Freq(void)
{
	return 72000000/(TIM_GetPrescaler(TIM3)+1)/ (TIM_GetCapture1(TIM3)+1);
}

/*
��ȡTIM3_PWMI ģʽ��ռ�ձ�
*/

uint32_t Get_TIM3_Duty(void)
{
	return (TIM_GetCapture2(TIM3) + 1)*100 / (TIM_GetCapture1(TIM3) + 1);
}
