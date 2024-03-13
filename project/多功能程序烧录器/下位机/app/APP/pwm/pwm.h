#ifndef _pwm_H
#define _pwm_H

#include "system.h"

void TIM2_Int_Init(u16 arr,u16 psc);
void TIM3_IC_Init(void);
uint32_t Get_TIM3_CH1_Freq(void);
uint32_t Get_TIM3_Duty(void);

#endif
