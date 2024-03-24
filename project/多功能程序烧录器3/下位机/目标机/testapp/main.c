/*
  DFU工具下载的子程序虽然起始地址不是0x08000000 (由项目属性Target选项卡中的IROM1配置)
  但是也可以在Keil中直接下载程序, 而且还能使用ST-Link进行程序调试
  只要在DFU主程序中禁用CRC校验就行
  请确保system_stm32xxxx.c中设置的SCB->VTOR刚好等于程序的起始地址
*/
#include <stdio.h>
#include <stm32f1xx.h>
#include "common.h"

IWDG_HandleTypeDef hiwdg;

static void iwdg_init(void)
{
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 2499; // 超时时间为(2499+1)*0.8ms=2s
  HAL_IWDG_Init(&hiwdg);
}

static void reset_check(void)
{
  int i;
  uint8_t cmd[2];
  HAL_StatusTypeDef status;
  
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
  {
    status = HAL_UART_Receive(&huart1, cmd, sizeof(cmd), 200);
    if (status == HAL_OK)
    {
      if (cmd[0] == 0xab && cmd[1] == 0xab)
      {
        for (i = 0; i < 2; i++)
        {
          HAL_IWDG_Refresh(&hiwdg);
          HAL_Delay(1000);
        }
        HAL_NVIC_SystemReset();
      }
    }
  }
}

int main(void)
{
	uint32_t cnt = 0;
  uint32_t last, now, source;
  
  HAL_Init();
  clock_init();
  usart_init(115200);
  printf("SystemCoreClock=%u\n", SystemCoreClock);
  printf("In While\n");
  
  while (1)
  {
			printf("V3.00 apptest, cnt=%d\n", cnt);
			HAL_Delay(1000);
			cnt++;
  }
}
