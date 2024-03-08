#include <stdio.h>
#include <stdlib.h>
#include <stm32f1xx.h>
#include <string.h>
#include "common.h"

#pragma import(__use_no_semihosting) // 禁用半主机模式 (不然调用printf就会进HardFault)

FILE __stdout = {1};
FILE __stderr = {2};
UART_HandleTypeDef huart1;

/* main函数返回时执行的函数 */
void _sys_exit(int returncode)
{
  printf("Exited! returncode=%d\n", returncode);
  while (1);
}

void _ttywrch(int ch)
{
  if (ch == '\n')
    HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
  else
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
}

/* HAL库参数错误警告 */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  printf("%s: file %s on line %d\n", __FUNCTION__, file, line);
  while (1);
}
#endif

void clock_init(void)
{
  HAL_StatusTypeDef status;
  RCC_ClkInitTypeDef clk = {0};
  RCC_OscInitTypeDef osc = {0};
  
  // 启动HSE晶振, 并用PLL倍频9倍
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.PLL.PLLMUL = RCC_PLL_MUL9;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLState = RCC_PLL_ON;
  status = HAL_RCC_OscConfig(&osc);
  
  // 若HSE启动失败, 则改用HSI, 并用PLL倍频8倍
  if (status != HAL_OK)
  {
    osc.HSEState = RCC_HSE_OFF;
    osc.PLL.PLLMUL = RCC_PLL_MUL16;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    HAL_RCC_OscConfig(&osc);
  }
  
  // 设置ADC时钟分频系数
  __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);
  
  // 将PLL设为系统时钟
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

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

void usart_init(int baud_rate)
{
  GPIO_InitTypeDef gpio;
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pin = GPIO_PIN_9;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pin = GPIO_PIN_10;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &gpio);
  
  huart1.Instance = USART1;
#ifdef UART_ADVFEATURE_RXOVERRUNDISABLE_INIT
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_RXOVERRUNDISABLE_INIT;
  huart1.AdvancedInit.OverrunDisable = UART_ADVFEATURE_OVERRUN_DISABLE;
#endif
  huart1.Init.BaudRate = baud_rate;
  huart1.Init.Mode = UART_MODE_TX_RX;
  HAL_UART_Init(&huart1);
}

void HardFault_Handler(void)
{
  printf("Hard Error!\n");
  while (1);
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
