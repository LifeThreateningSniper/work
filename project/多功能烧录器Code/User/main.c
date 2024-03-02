#include "system.h"
#include "SysTick.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "adc_temp.h"
#include "24cxx.h"
#include "rtc.h"
#include "flash.h"
#include "sd_sdio.h"



//通过串口打印SD卡相关信息
void show_sdcard_info(void)
{
	switch(SDCardInfo.CardType)
	{
		case SDIO_STD_CAPACITY_SD_CARD_V1_1:printf("SD卡类型:SDSC V1.1\r\n");break;
		case SDIO_STD_CAPACITY_SD_CARD_V2_0:printf("SD卡类型:SDSC V2.0\r\n");break;
		case SDIO_HIGH_CAPACITY_SD_CARD:printf("SD卡类型:SDHC V2.0\r\n");break;
		case SDIO_MULTIMEDIA_CARD:printf("SD卡类型:MMC Card\r\n");break;
	}	
	printf("SD卡容量:%d MB\r\n",(u32)(SDCardInfo.CardCapacity>>20));	//显示容量
}  

int main()
{
	u8 i=0;
	u8 key=0;
	
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	USART1_Init(115200);
	LED_Init();
	KEY_Init();
	ADC_Temp_Init();
	AT24CXX_Init();
	
	printf("普中F103-准端-Z100综合测试实验\r\n");
	printf("硬件版本：V1.0  软件版本：V1.0\r\n");
	printf("开始测试...\r\n");
	while(AT24CXX_Check())//检测AT24C02是否正常
	{
		printf("\r\nAT24C02检测Error!\r\n");
		delay_ms(1000);
	}
	printf("\r\nAT24C02检测OK!\r\n");
	EN25QXX_Init();
	RTC_Init();
	
	while(1)
	{
		key=KEY_Scan(0);
		switch(key)
		{
			case KEY_UP_PRESS: LED2=!LED2;break;
			case KEY0_PRESS: LED2=!LED2;break;
			case KEY1_PRESS: LED2=!LED2;break;
		}
		
		i++;
		if(i%20==0)
		{
			LED1=!LED1;
		}
		
		if(SD_Init());//检测不到SD卡
		else
		{
			if(i%100==0)
				show_sdcard_info();
		}			
		
		delay_ms(10);	
	}
}
