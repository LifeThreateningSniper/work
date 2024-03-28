#ifndef __OLED_H
#define __OLED_H
 
#include <stdio.h>
#include <stm32f1xx.h>
 
/*引脚配置（作修改部分）*/
#define OLED_W_SCL(x)       do{ x ? \
                            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET) : \
                            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); \
                            }while(0)
#define OLED_W_SDA(x)       do{ x ? \
                            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET) : \
                            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET); \
                            }while(0)
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

#endif
														