/*
  DFU�������ص��ӳ�����Ȼ��ʼ��ַ����0x08000000 (����Ŀ����Targetѡ��е�IROM1����)
  ����Ҳ������Keil��ֱ�����س���, ���һ���ʹ��ST-Link���г������
  ֻҪ��DFU�������н���CRCУ�����
  ��ȷ��system_stm32xxxx.c�����õ�SCB->VTOR�պõ��ڳ������ʼ��ַ
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
      OLED_ShowString(2, 1, "V3.00 apptest!");    //1��3����ʾ�ַ���HelloWorld!
			HAL_Delay(1000);
			cnt++;
  }
}
