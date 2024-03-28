/*
  DFU工具下载的子程序虽然起始地址不是0x08000000 (由项目属性Target选项卡中的IROM1配置)
  但是也可以在Keil中直接下载程序, 而且还能使用ST-Link进行程序调试
  只要在DFU主程序中禁用CRC校验就行
  请确保system_stm32xxxx.c中设置的SCB->VTOR刚好等于程序的起始地址
*/
#include <stdio.h>
#include <stm32f1xx.h>
#include "oled.h"
#include "common.h"

int main(void)
{
	uint32_t cnt = 0;
  
  HAL_Init();
  clock_init();
  usart_init(115200);
	OLED_Init();
  OLED_Clear();
  printf("SystemCoreClock=%u\n", SystemCoreClock);
  printf("In While\n");
  
  while (1)
  {
			printf("V3.00 apptest, cnt=%d\n", cnt);
      OLED_ShowString(2, 1, "V3.00 apptest!");    //1行3列显示字符串HelloWorld!
			HAL_Delay(1000);
			cnt++;
  }
}
