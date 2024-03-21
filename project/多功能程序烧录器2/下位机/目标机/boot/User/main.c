#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "uart3.h"
#include "upgrade.h"

int main()
{
	u8 val;
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	LED_Init();
	USART1_Init(115200);
	USART3_Init(115200);
	QueueInit(&g_UpdataRxBuff);
	
	printf("slave mcu in while\r\n");
	while (1) {
		//LED2=!LED2;
		if (QueueOut(&g_UpdataRxBuff, &val) == 0) {
			printf("%x ", val);
		}
		delay_ms(20);
	}
}
