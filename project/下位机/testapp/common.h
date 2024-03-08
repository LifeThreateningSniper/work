#ifndef _COMMON_H
#define _COMMON_H

extern UART_HandleTypeDef huart1;

struct __FILE
{
  int handle;
};

void clock_init(void);
void dump_data(const void *data, int len);
void usart_init(int baud_rate);

#endif
