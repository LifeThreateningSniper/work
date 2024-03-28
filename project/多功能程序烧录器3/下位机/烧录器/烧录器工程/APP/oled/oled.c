#include "stm32f10x.h"
#include "SysTick.h"
#include "oled.h"
#include "OLED_Font.h"
#include "stdio.h"
#include "string.h"

unsigned char OLED_GRAM[128][8];

/*引脚配置*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOC, GPIO_Pin_0, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOC, GPIO_Pin_1, (BitAction)(x))

/*引脚初始化*/
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
  * @brief  I2C开始
  * @param  无
  * @retval 无
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}
 
/**
  * @brief  I2C停止
  * @param  无
  * @retval 无
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}
 
/**
  * @brief  I2C发送一个字节
  * @param  Byte 要发送的一个字节
  * @retval 无
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
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}
 
/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}
 
/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}
 
/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}
 
/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
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
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	}
}
 
/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
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
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
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
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
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
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
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
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
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
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
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
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)			//上电延时
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			//端口初始化

	OLED_WriteCommand(0xAE);	//关闭显示
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	//设置显示开始行
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置
 
	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);
 
	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);
 
	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);
 
	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭
 
	OLED_WriteCommand(0xA6);	//设置正常/倒转显示
 
	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);
 
	OLED_WriteCommand(0xAF);	//开启显示

	OLED_Clear();				//OLED清屏
}

// 画点
void OLED_DrawDot(unsigned char x, unsigned char y, unsigned char t)
{
	unsigned char pos, bx, temp = 0;
		
	// 此OLED的分辨率为128*64，横坐标大于127，纵坐标大于63，则参数非法 
	if ( x > 127 || y > 63)  {
		return;
	}
	// 因为此OLED是按页显示，每页8个像素，所以/8用于计算待显示的点在哪页中
	pos = (y) / 8;
	// 一列中有8个像素，所以计算一下待显示的点，在当前列中的第几个点
	bx = y % 8;
	// 移位，让temp的第bx位为1
	temp = 1 << (bx);
		
	if (t) {
		OLED_GRAM[x][pos]|=temp;  //第bx位，置1，其他位值不变
	} else {
		OLED_GRAM[x][pos]&=~temp;  //第bx位，置0，其他位值不变		
	}
}

// 画线
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
	} else if(offset_x == 0) {//垂直线
		incx = 0;
	} else {
		incx = -1;
		offset_x = - offset_x;
	}
 
	if(offset_y > 0) {
		incy = 1;
	} else if(offset_y == 0) {
		incy=0;    //水平线
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

// 画圆
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
        num = (a * a + b * b) - r*r;//计算画的点离圆心的距离
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
		OLED_DrawLine(i, PWM_Y_LOW, i+x, PWM_Y_LOW); 		// 上半个周期横线
		OLED_DrawLine(i+x, PWM_Y_LOW, i+x, PWM_Y_HIGH); 	// 上半个周期竖线
		OLED_DrawLine(i+x, PWM_Y_HIGH, i+x+x, PWM_Y_HIGH); 	// 下半个周期横线
		OLED_DrawLine(i+x+x, PWM_Y_HIGH, i+x+x, PWM_Y_LOW); // 下半个周期竖线
	}
}

// 刷新屏幕
void OLED_Refresh_Gram(void)
{
    unsigned char i, n;
    for(i = 0; i < 8; i++) {
		OLED_SetCursor(i, 0); 
        for(n = 0; n < 128; n++) { //写一PAGE的GDDRAM数据
            OLED_WriteData(OLED_GRAM[n][i]);
        }
    }
}
