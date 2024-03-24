#ifndef _COMMON_H
#define _COMMON_H

#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
  
#ifndef __UNALIGNED_UINT32_READ
#define __UNALIGNED_UINT32_READ(addr) (*((const __packed uint32_t *)(addr)))
#endif

#ifndef FLASH_SIZE
#ifdef FLASHSIZE_BASE
#define FLASH_SIZE (*(uint16_t *)FLASHSIZE_BASE * 1024)
#else
#error "FLASH_SIZE is unknown!"
#endif
#endif

extern UART_HandleTypeDef huart1, huart2, huart3;

struct __FILE
{
  int handle;
};

void dump_data(const void *data, int len);
void printf_enable(int enabled);
void usart_init(int baud_rate);

#endif
