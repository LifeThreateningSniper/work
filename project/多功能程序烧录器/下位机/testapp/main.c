/*
  DFU�������ص��ӳ�����Ȼ��ʼ��ַ����0x08000000 (����Ŀ����Targetѡ��е�IROM1����)
  ����Ҳ������Keil��ֱ�����س���, ���һ���ʹ��ST-Link���г������
  ֻҪ��DFU�������н���CRCУ�����
  ��ȷ��system_stm32xxxx.c�����õ�SCB->VTOR�պõ��ڳ������ʼ��ַ
*/
#include <stdio.h>
#include <stm32f1xx.h>
#include "common.h"

IWDG_HandleTypeDef hiwdg;

static void iwdg_init(void)
{
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 2499; // ��ʱʱ��Ϊ(2499+1)*0.8ms=2s
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
