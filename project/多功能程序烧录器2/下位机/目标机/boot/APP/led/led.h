#ifndef _led_H
#define _led_H
#include "system.h"
 

#define LED2_PORT 			GPIOC 
#define LED2_PIN 			GPIO_Pin_13
#define LED2_PORT_RCC		RCC_APB2Periph_GPIOC


#define LED2 PCout(13)  	


void LED_Init(void);


#endif
