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
  uint32_t last, now, source;
  
  HAL_Init();
  
  iwdg_init();
  clock_init();
  usart_init(115200);
  printf("STM32F10x testapp\n");
  printf("SystemCoreClock=%u\n", SystemCoreClock);
  
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
    printf("IWDG reset!\n");
  __HAL_RCC_CLEAR_RESET_FLAGS();
  
  source = __HAL_RCC_GET_PLL_OSCSOURCE();
  if (source == RCC_PLLSOURCE_HSI_DIV2)
    printf("PLL source: HSI/2\n");
  else if (source == RCC_PLLSOURCE_HSE)
    printf("PLL source: HSE\n");
  
  while (1)
  {
    HAL_IWDG_Refresh(&hiwdg);
    
    now = HAL_GetTick();
    if (now - last > 500)
    {
      last = now;
      printf("ticks=%u\n", now);
    }
    
    reset_check();
  }
}
