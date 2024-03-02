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



//ͨ�����ڴ�ӡSD�������Ϣ
void show_sdcard_info(void)
{
	switch(SDCardInfo.CardType)
	{
		case SDIO_STD_CAPACITY_SD_CARD_V1_1:printf("SD������:SDSC V1.1\r\n");break;
		case SDIO_STD_CAPACITY_SD_CARD_V2_0:printf("SD������:SDSC V2.0\r\n");break;
		case SDIO_HIGH_CAPACITY_SD_CARD:printf("SD������:SDHC V2.0\r\n");break;
		case SDIO_MULTIMEDIA_CARD:printf("SD������:MMC Card\r\n");break;
	}	
	printf("SD������:%d MB\r\n",(u32)(SDCardInfo.CardCapacity>>20));	//��ʾ����
}  

int main()
{
	u8 i=0;
	u8 key=0;
	
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //�ж����ȼ����� ��2��
	USART1_Init(115200);
	LED_Init();
	KEY_Init();
	ADC_Temp_Init();
	AT24CXX_Init();
	
	printf("����F103-׼��-Z100�ۺϲ���ʵ��\r\n");
	printf("Ӳ���汾��V1.0  ����汾��V1.0\r\n");
	printf("��ʼ����...\r\n");
	while(AT24CXX_Check())//���AT24C02�Ƿ�����
	{
		printf("\r\nAT24C02���Error!\r\n");
		delay_ms(1000);
	}
	printf("\r\nAT24C02���OK!\r\n");
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
		
		if(SD_Init());//��ⲻ��SD��
		else
		{
			if(i%100==0)
				show_sdcard_info();
		}			
		
		delay_ms(10);	
	}
}
