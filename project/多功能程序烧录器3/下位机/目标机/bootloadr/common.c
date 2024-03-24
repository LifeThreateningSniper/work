#include <stdio.h>
#include <stdlib.h>
#include <stm32f1xx.h>
#include <string.h>
#include "common.h"

#pragma import(__use_no_semihosting) // ���ð�����ģʽ (��Ȼ����printf�ͻ��HardFault)

FILE __stdout = {1};
FILE __stderr = {2};
UART_HandleTypeDef huart1, huart2, huart3;
static int printf_enabled;

/* main��������ʱִ�еĺ��� */
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

/* HAL��������󾯸� */
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

/* printf��perror�ض��򵽴��� */
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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();
  
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pin = GPIO_PIN_9;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pin = GPIO_PIN_10;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pin = GPIO_PIN_2;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pin = GPIO_PIN_3;
  gpio.Pull = GPIO_PULLUP;  // �ر������������¼����Ŀ��ӵĴ���ͨ��ʧ������
  HAL_GPIO_Init(GPIOA, &gpio);
  
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pin = GPIO_PIN_10;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);
  
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pin = GPIO_PIN_11;
  // gpio.Pull = GPIO_PULLUP;  // �ر������������¼����Ŀ��ӵĴ���ͨ��ʧ������
  HAL_GPIO_Init(GPIOB, &gpio);
  
  // ���������ӳ���Ĵ���
  huart1.Instance = USART1;
#ifdef UART_ADVFEATURE_RXOVERRUNDISABLE_INIT
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_RXOVERRUNDISABLE_INIT;
  huart1.AdvancedInit.OverrunDisable = UART_ADVFEATURE_OVERRUN_DISABLE;
#endif
  huart1.Init.BaudRate = baud_rate;
  huart1.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart1);
  
  // ��������
  huart2.Instance = USART2;
  huart2.Init.BaudRate = baud_rate;
  huart2.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart2);

  // ���ߴ���
  huart3.Instance = USART3;
  huart3.Init.BaudRate = baud_rate;
  huart3.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart3);
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
