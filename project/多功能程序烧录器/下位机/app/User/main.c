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
#include "pwm.h"

#define BOOT_IS_IN_USED

#ifdef BOOT_IS_IN_USED
#define NVIC_VectTab_FLASH           ((uint32_t)0x08000000)
#define APP_VECTOR_OFFSET            (0x5000)
#endif

void CalcPlayFreq(int freq)
{
	int playfreq = 2;

	if (freq < 600) {
		playfreq = 32;
	} else if (freq > 9600) {
		playfreq = 1;
	} else {
		playfreq = 32 - freq / 300;
	}
	OLED_DrawPwm(playfreq);
}

int main()
{		
	u8 res=0;
	u32 total, free;
	uint32_t oldfreq = 0;
	uint32_t curfreq = 0;
	char str[16] = {0};

	// 向量表偏移量设置
	SCB->VTOR = NVIC_VectTab_FLASH | (APP_VECTOR_OFFSET & (uint32_t)0x1FFFFF80);
	__enable_irq();	//使能中断

	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	LED_Init();
	USART1_Init(115200);
	OLED_Init();
	OLED_Clear(); 
	TIM3_IC_Init();	
	my_mem_init(SRAMIN);		//初始化内部内存池
	
	while(SD_Init())//检测不到SD卡
	{
		printf("SD Card Error!\r\n");
		delay_ms(500);					
	}
	
	//检测SD卡成功 			
	printf("SD Card OK!\r\n");	
	
	FATFS_Init();				//为fatfs相关变量申请内存	
  	f_mount(fs[0],"0:",1); 		//挂载SD卡
	while(fatfs_getfree("0:",&total,&free))	//得到SD卡的总容量和剩余容量
	{
		delay_ms(200);
		LED2=!LED2;
	}	
	printf("SD Card Total Size: %dMB, Free Size: %dMB!\r\n", total, free);		
	
	oldfreq = Get_TIM3_CH1_Freq();
	CalcPlayFreq(oldfreq);
	OLED_Refresh_Gram();
	sprintf(str, "freq: %dHZ", oldfreq);
	OLED_ShowString(1, 2, str);
	while(1)
	{
		curfreq = Get_TIM3_CH1_Freq();
		if (curfreq != oldfreq) {
			OLED_Clear();
			OLED_DrawClear();
			CalcPlayFreq(oldfreq);
			OLED_Refresh_Gram(); // 定时刷新屏幕
			sprintf(str, "freq: %dHZ", curfreq);
			OLED_ShowString(1, 2, str);
			oldfreq = curfreq;
		}

		printf("freq:%d, duty:%d\r\n", Get_TIM3_CH1_Freq(), Get_TIM3_Duty()); 
		
		delay_ms(500);
	}
}
