#ifndef __OLED_H
#define __OLED_H
 
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
 
void OLED_DrawDot(unsigned char x,unsigned char y,unsigned char t);
void OLED_DrawLine(unsigned int x1, unsigned int y1, unsigned int x2,unsigned int y2);
void OLED_DrawCircle(u8 x,u8 y, u8 r);
void OLED_Refresh_Gram(void);
void OLED_DrawPwm(int freq);
void OLED_DrawClear(void);
#endif
