#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "tftlcd.h"
#include "key.h"
#include "malloc.h" 
#include "sd_sdio.h"
#include "flash.h"
#include "ff.h" 
#include "fatfs_app.h"

	
int main()
{
	u8 i=0;		
	u8 res=0;
	u32 total,free;
	
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //�ж����ȼ����� ��2��
	LED_Init();
	USART1_Init(115200);
	TFTLCD_Init();			//LCD��ʼ��
	EN25QXX_Init();				//��ʼ��EN25Q128	  
	my_mem_init(SRAMIN);		//��ʼ���ڲ��ڴ��
	
	FRONT_COLOR=RED;//��������Ϊ��ɫ 
	LCD_ShowString(10,10,tftlcd_data.width,tftlcd_data.height,16,"PRECHIN STM32F1");	
	LCD_ShowString(10,30,tftlcd_data.width,tftlcd_data.height,16,"Fatfs Test");	
	LCD_ShowString(10,50,tftlcd_data.width,tftlcd_data.height,16,"www.prechin.net");
	
	while(SD_Init())//��ⲻ��SD��
	{
		LCD_ShowString(10,100,tftlcd_data.width,tftlcd_data.height,16,"SD Card Error!");
		printf("SD Card Error!\r\n");
		delay_ms(500);					
	}
	
 	FRONT_COLOR=BLUE;	//��������Ϊ��ɫ 
	//���SD���ɹ� 			
	printf("SD Card OK!\r\n");	
	LCD_ShowString(10,100,tftlcd_data.width,tftlcd_data.height,16,"SD Card OK    ");
	
	FATFS_Init();				//Ϊfatfs��ر��������ڴ�				 
  	f_mount(fs[0],"0:",1); 		//����SD��
	res=f_mount(fs[1],"1:",1); 	//����FLASH.
	if(res==0X0D)//FLASH����,FAT�ļ�ϵͳ����,���¸�ʽ��FLASH
	{
		LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"Flash Disk Formatting...");	//��ʽ��FLASH
		res=f_mkfs("1:",1,4096);//��ʽ��FLASH,1,�̷�;1,����Ҫ������,8������Ϊ1����
		if(res==0)
		{
			f_setlabel((const TCHAR *)"1:PRECHIN");	//����Flash���̵�����Ϊ��PRECHIN
			LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"Flash Disk Format Finish");	//��ʽ�����
		}
		else 
			LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"Flash Disk Format Error ");	//��ʽ��ʧ��
		delay_ms(1000);
	}	
	LCD_Fill(10,80,tftlcd_data.width,80+16,WHITE);		//�����ʾ
	while(fatfs_getfree("0:",&total,&free))	//�õ�SD������������ʣ������
	{
		LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"SD Card Fatfs Error!");
		delay_ms(200);
		LED2=!LED2;
	}			
	LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"FATFS OK!");	 
	LCD_ShowString(10,100,tftlcd_data.width,tftlcd_data.height,16,"SD Total Size:     MB");	 
	LCD_ShowString(10,120,tftlcd_data.width,tftlcd_data.height,16,"SD  Free Size:     MB"); 	    
 	LCD_ShowNum(10+8*14,100,total>>10,5,16);	//��ʾSD�������� MB
 	LCD_ShowNum(10+8*14,120,free>>10,5,16);     //��ʾSD��ʣ������ MB
	
	while(1)
	{
		
		i++;
		if(i%10==0)
		{
			LED1=!LED1;
		}
		delay_ms(10);	
	}
}
