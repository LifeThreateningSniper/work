#include "stm32f10x.h"
#include "SysTick.h"
#include "oled.h"
#include "OLED_Font.h"
#include "stdio.h"
#include "string.h"

unsigned char OLED_GRAM[128][8];

/*��������*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOC, GPIO_Pin_0, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOC, GPIO_Pin_1, (BitAction)(x))

/*���ų�ʼ��*/
void OLED_I2C_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
   	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
 	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
 	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}
 
/**
  * @brief  I2C��ʼ
  * @param  ��
  * @retval ��
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}
 
/**
  * @brief  I2Cֹͣ
  * @param  ��
  * @retval ��
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}
 
/**
  * @brief  I2C����һ���ֽ�
  * @param  Byte Ҫ���͵�һ���ֽ�
  * @retval ��
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(Byte & (0x80 >> i));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//�����һ��ʱ�ӣ�������Ӧ���ź�
	OLED_W_SCL(0);
}
 
/**
  * @brief  OLEDд����
  * @param  Command Ҫд�������
  * @retval ��
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//�ӻ���ַ
	OLED_I2C_SendByte(0x00);		//д����
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}
 
/**
  * @brief  OLEDд����
  * @param  Data Ҫд�������
  * @retval ��
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//�ӻ���ַ
	OLED_I2C_SendByte(0x40);		//д����
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}
 
/**
  * @brief  OLED���ù��λ��
  * @param  Y �����Ͻ�Ϊԭ�㣬���·�������꣬��Χ��0~7
  * @param  X �����Ͻ�Ϊԭ�㣬���ҷ�������꣬��Χ��0~127
  * @retval ��
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//����Yλ��
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//����Xλ�ø�4λ
	OLED_WriteCommand(0x00 | (X & 0x0F));			//����Xλ�õ�4λ
}
 
/**
  * @brief  OLED����
  * @param  ��
  * @retval ��
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}
 
/**
  * @brief  OLED��ʾһ���ַ�
  * @param  Line ��λ�ã���Χ��1~4
  * @param  Column ��λ�ã���Χ��1~16
  * @param  Char Ҫ��ʾ��һ���ַ�����Χ��ASCII�ɼ��ַ�
  * @retval ��
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//���ù��λ�����ϰ벿��
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//��ʾ�ϰ벿������
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//���ù��λ�����°벿��
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//��ʾ�°벿������
	}
}
 
/**
  * @brief  OLED��ʾ�ַ���
  * @param  Line ��ʼ��λ�ã���Χ��1~4
  * @param  Column ��ʼ��λ�ã���Χ��1~16
  * @param  String Ҫ��ʾ���ַ�������Χ��ASCII�ɼ��ַ�
  * @retval ��
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}
 
/**
  * @brief  OLED�η�����
  * @retval ����ֵ����X��Y�η�
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}
 
/**
  * @brief  OLED��ʾ���֣�ʮ���ƣ�������
  * @param  Line ��ʼ��λ�ã���Χ��1~4
  * @param  Column ��ʼ��λ�ã���Χ��1~16
  * @param  Number Ҫ��ʾ�����֣���Χ��0~4294967295
  * @param  Length Ҫ��ʾ���ֵĳ��ȣ���Χ��1~10
  * @retval ��
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}
 
/**
  * @brief  OLED��ʾ���֣�ʮ���ƣ�����������
  * @param  Line ��ʼ��λ�ã���Χ��1~4
  * @param  Column ��ʼ��λ�ã���Χ��1~16
  * @param  Number Ҫ��ʾ�����֣���Χ��-2147483648~2147483647
  * @param  Length Ҫ��ʾ���ֵĳ��ȣ���Χ��1~10
  * @retval ��
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}
 
/**
  * @brief  OLED��ʾ���֣�ʮ�����ƣ�������
  * @param  Line ��ʼ��λ�ã���Χ��1~4
  * @param  Column ��ʼ��λ�ã���Χ��1~16
  * @param  Number Ҫ��ʾ�����֣���Χ��0~0xFFFFFFFF
  * @param  Length Ҫ��ʾ���ֵĳ��ȣ���Χ��1~8
  * @retval ��
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}
 
/**
  * @brief  OLED��ʾ���֣������ƣ�������
  * @param  Line ��ʼ��λ�ã���Χ��1~4
  * @param  Column ��ʼ��λ�ã���Χ��1~16
  * @param  Number Ҫ��ʾ�����֣���Χ��0~1111 1111 1111 1111
  * @param  Length Ҫ��ʾ���ֵĳ��ȣ���Χ��1~16
  * @retval ��
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}
 
/**
  * @brief  OLED��ʼ��
  * @param  ��
  * @retval ��
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)			//�ϵ���ʱ
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			//�˿ڳ�ʼ��

	OLED_WriteCommand(0xAE);	//�ر���ʾ
	
	OLED_WriteCommand(0xD5);	//������ʾʱ�ӷ�Ƶ��/����Ƶ��
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	//���ö�·������
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	//������ʾƫ��
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	//������ʾ��ʼ��
	
	OLED_WriteCommand(0xA1);	//�������ҷ���0xA1���� 0xA0���ҷ���
	
	OLED_WriteCommand(0xC8);	//�������·���0xC8���� 0xC0���·���
 
	OLED_WriteCommand(0xDA);	//����COM����Ӳ������
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//���öԱȶȿ���
	OLED_WriteCommand(0xCF);
 
	OLED_WriteCommand(0xD9);	//����Ԥ�������
	OLED_WriteCommand(0xF1);
 
	OLED_WriteCommand(0xDB);	//����VCOMHȡ��ѡ�񼶱�
	OLED_WriteCommand(0x30);
 
	OLED_WriteCommand(0xA4);	//����������ʾ��/�ر�
 
	OLED_WriteCommand(0xA6);	//��������/��ת��ʾ
 
	OLED_WriteCommand(0x8D);	//���ó���
	OLED_WriteCommand(0x14);
 
	OLED_WriteCommand(0xAF);	//������ʾ

	OLED_Clear();				//OLED����
}

// ����
void OLED_DrawDot(unsigned char x, unsigned char y, unsigned char t)
{
	unsigned char pos, bx, temp = 0;
		
	// ��OLED�ķֱ���Ϊ128*64�����������127�����������63��������Ƿ� 
	if ( x > 127 || y > 63)  {
		return;
	}
	// ��Ϊ��OLED�ǰ�ҳ��ʾ��ÿҳ8�����أ�����/8���ڼ������ʾ�ĵ�����ҳ��
	pos = (y) / 8;
	// һ������8�����أ����Լ���һ�´���ʾ�ĵ㣬�ڵ�ǰ���еĵڼ�����
	bx = y % 8;
	// ��λ����temp�ĵ�bxλΪ1
	temp = 1 << (bx);
		
	if (t) {
		OLED_GRAM[x][pos]|=temp;  //��bxλ����1������λֵ����
	} else {
		OLED_GRAM[x][pos]&=~temp;  //��bxλ����0������λֵ����		
	}
}

// ����
void OLED_DrawLine(unsigned int x1, unsigned int y1, unsigned int x2,unsigned int y2)
{
	unsigned int t; 
	int offset_x, offset_y; 
	int incx, incy, uRow, uCol; 
	float K = 0.0f;

	offset_x = x2 - x1;
	offset_y = y2 - y1;
	uRow = x1; 
	uCol = y1;

	if(offset_x > 0) {
		incx = 1;
	} else if(offset_x == 0) {//��ֱ��
		incx = 0;
	} else {
		incx = -1;
		offset_x = - offset_x;
	}
 
	if(offset_y > 0) {
		incy = 1;
	} else if(offset_y == 0) {
		incy=0;    //ˮƽ��
	} else {
		incy = -1;
		offset_y = -offset_y;
	}

	if(incx == 0) {
		for(t = 0; t <= offset_y + 1; t++) { 
			OLED_DrawDot(uRow, uCol + t * incy, 1);
		}
	} else if (incy == 0) {
		for(t = 0; t <= offset_x + 1; t++){ 
			OLED_DrawDot(uRow + t * incx, uCol, 1);
		}
	} else {
		K = (float)(((float)y2 - (float)y1) * 1.000 / ((float)x2 - (float)x1));
		for(t = 0; t <= offset_x + 1; t++) { 
			OLED_DrawDot(uRow+t, (u8)(uCol + t*K), 1);
		}
	}
}

// ��Բ
void OLED_DrawCircle(u8 x,u8 y, u8 r)
{
	int a, b,num;
    a = 0;
    b = r;
    while(2 * b * b >= r * r) {
        OLED_DrawDot(x + a, y - b, 1);
        OLED_DrawDot(x - a, y - b, 1);
        OLED_DrawDot(x - a, y + b, 1);
        OLED_DrawDot(x + a, y + b, 1);
 
        OLED_DrawDot(x + b, y + a, 1);
        OLED_DrawDot(x + b, y - a, 1);
        OLED_DrawDot(x - b, y - a, 1);
        OLED_DrawDot(x - b, y + a, 1);
        
        a++;
        num = (a * a + b * b) - r*r;//���㻭�ĵ���Բ�ĵľ���
        if(num > 0) {
            b--;
            a--;
        }
    }
}
void OLED_DrawClear(void)
{
	int i;
	
	for (i = 0; i < 128; i++) {
		memset(OLED_GRAM[i], 0, sizeof(unsigned char) * 8);
	}
}

#define PWM_Y_LOW 56
#define PWM_Y_HIGH 48

void OLED_DrawPwm(int x)
{
	int i;

	for (i = 0; i < 128; i += (2*x)) {
		OLED_DrawLine(i, PWM_Y_LOW, i+x, PWM_Y_LOW); 		// �ϰ�����ں���
		OLED_DrawLine(i+x, PWM_Y_LOW, i+x, PWM_Y_HIGH); 	// �ϰ����������
		OLED_DrawLine(i+x, PWM_Y_HIGH, i+x+x, PWM_Y_HIGH); 	// �°�����ں���
		OLED_DrawLine(i+x+x, PWM_Y_HIGH, i+x+x, PWM_Y_LOW); // �°����������
	}
}

// ˢ����Ļ
void OLED_Refresh_Gram(void)
{
    unsigned char i, n;
    for(i = 0; i < 8; i++) {
		OLED_SetCursor(i, 0); 
        for(n = 0; n < 128; n++) { //дһPAGE��GDDRAM����
            OLED_WriteData(OLED_GRAM[n][i]);
        }
    }
}
