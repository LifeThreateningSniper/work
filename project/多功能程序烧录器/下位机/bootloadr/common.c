#include <stdio.h>
#include <stdlib.h>
#include <stm32f1xx.h>
#include <string.h>
#include "common.h"

#pragma import(__use_no_semihosting) // 禁用半主机模式 (不然调用printf就会进HardFault)

FILE __stdout = {1};
FILE __stderr = {2};
UART_HandleTypeDef huart1, huart4;
static int printf_enabled;

/* main函数返回时执行的函数 */
void _sys_exit(int returncode)
{
  printf_enabled = 1;
  printf("Exited! returncode=%d\n", returncode);
  while (1);
}

void _ttywrch(int ch)
{
  if (printf_enabled)
  {
    if (ch == '\n')
      HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
    else
      HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  }
}

/* HAL库参数错误警告 */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  printf_enabled = 1;
  printf("%s: file %s on line %d\n", __FUNCTION__, file, line);
  while (1);
}
#endif

void dump_data(const void *data, int len)
{
  const uint8_t *p = data;
  
  while (len--)
    printf("%02X", *p++);
  printf("\n");
}

/* printf和perror重定向到串口 */
int fputc(int ch, FILE *fp)
{
  if (fp->handle == 1 || fp->handle == 2)
  {
    _ttywrch(ch);
    return ch;
  }
  return EOF;
}

void printf_enable(int enabled)
{
  printf_enabled = enabled;
}

void usart_init(int baud_rate)
{
  GPIO_InitTypeDef gpio;
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_UART4_CLK_ENABLE();
  
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pin = GPIO_PIN_9;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pin = GPIO_PIN_10;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &gpio);
  
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pin = GPIO_PIN_10;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &gpio);
  
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pin = GPIO_PIN_11;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &gpio);
  
  // 用于下载子程序的串口
  huart1.Instance = USART1;
#ifdef UART_ADVFEATURE_RXOVERRUNDISABLE_INIT
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_RXOVERRUNDISABLE_INIT;
  huart1.AdvancedInit.OverrunDisable = UART_ADVFEATURE_OVERRUN_DISABLE;
#endif
  huart1.Init.BaudRate = baud_rate;
  huart1.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart1);
  
  // printf调试用的串口
  huart4.Instance = UART4;
  huart4.Init.BaudRate = baud_rate;
  huart4.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart4);
}

void HardFault_Handler(void)
{
  printf_enabled = 1;
  printf("Hard Error!\n");
  while (1);
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
