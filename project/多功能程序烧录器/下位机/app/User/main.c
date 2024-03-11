#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "oled.h"
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
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	LED_Init();
	USART1_Init(115200);
	OLED_Init(); 
	TIM3_IC_Init();	
	//EN25QXX_Init();				//初始化EN25Q128	  
	my_mem_init(SRAMIN);		//初始化内部内存池
	
	FRONT_COLOR=RED;//设置字体为红色 
	LCD_ShowString(10,10,tftlcd_data.width,tftlcd_data.height,16,"PRECHIN STM32F1");	
	LCD_ShowString(10,30,tftlcd_data.width,tftlcd_data.height,16,"Fatfs Test");	
	LCD_ShowString(10,50,tftlcd_data.width,tftlcd_data.height,16,"www.prechin.net");
	// while(SD_Init())//检测不到SD卡
	// {
		
	// 	LCD_ShowString(10,100,tftlcd_data.width,tftlcd_data.height,16,"SD Card Error!");
	// 	printf("SD Card Error!\r\n");
	// 	delay_ms(500);					
	// }
	
 	// FRONT_COLOR=BLUE;	//设置字体为蓝色 
	// //检测SD卡成功 			
	// printf("SD Card OK!\r\n");	
	// LCD_ShowString(10,100,tftlcd_data.width,tftlcd_data.height,16,"SD Card OK    ");
	
	// FATFS_Init();				//为fatfs相关变量申请内存	
	// printf("SD Card OK1111!\r\n");				 
  	// f_mount(fs[0],"0:",1); 		//挂载SD卡
	// printf("SD Card OK222!\r\n");
	// res=f_mount(fs[1],"1:",1); 	//挂载FLASH.
	// if(res==0X0D)//FLASH磁盘,FAT文件系统错误,重新格式化FLASH
	// {
	// 	LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"Flash Disk Formatting...");	//格式化FLASH
	// 	res=f_mkfs("1:",1,4096);//格式化FLASH,1,盘符;1,不需要引导区,8个扇区为1个簇
	// 	if(res==0)
	// 	{
	// 		f_setlabel((const TCHAR *)"1:PRECHIN");	//设置Flash磁盘的名字为：PRECHIN
	// 		LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"Flash Disk Format Finish");	//格式化完成
	// 	}
	// 	else 
	// 		LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"Flash Disk Format Error ");	//格式化失败
	// 	delay_ms(1000);
	// }	
	// LCD_Fill(10,80,tftlcd_data.width,80+16,WHITE);		//清除显示
	// while(fatfs_getfree("0:",&total,&free))	//得到SD卡的总容量和剩余容量
	// {
	// 	LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"SD Card Fatfs Error!");
	// 	delay_ms(200);
	// 	LED2=!LED2;
	// }	
	// printf("SD Card Total Size: %dMB, Free Size: %dMB!\r\n", total, free);		
	// LCD_ShowString(10,80,tftlcd_data.width,tftlcd_data.height,16,"FATFS OK!");	 
	// LCD_ShowString(10,100,tftlcd_data.width,tftlcd_data.height,16,"SD Total Size:     MB");	 
	// LCD_ShowString(10,120,tftlcd_data.width,tftlcd_data.height,16,"SD  Free Size:     MB"); 	    
 	// LCD_ShowNum(10+8*14,100,total>>10,5,16);	//显示SD卡总容量 MB
 	// LCD_ShowNum(10+8*14,120,free>>10,5,16);     //显示SD卡剩余容量 MB
	
	while(1)
	{
		printf("freq:%d, duty:%d\r\n", Get_TIM3_CH1_Freq(), Get_TIM3_Duty()); 
		delay_ms(1000);	
	}
}
